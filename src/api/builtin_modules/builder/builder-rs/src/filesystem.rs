//! Filesystem seam for the build services.
//!
//! The build workflows make many filesystem decisions (does this repo dir
//! exist? what configs are in this directory? read this error log) interleaved
//! with mutations (create/remove dirs, write scripts). Routing those through a
//! trait lets the workflow be driven end-to-end against an in-memory mock
//! instead of real disk.
//!
//! Scope: the *workflow-gating* operations. Byte-streaming file handles (zip
//! archiving, the per-command log file, artifact upload) and the unix-only
//! mountpoint/umount cleanup deliberately stay on real I/O — abstracting them
//! adds no workflow-decision coverage. `metadata`/`set_permissions` are exposed
//! as the narrow `file_len`/`set_mode` helpers because `std::fs::Metadata` has
//! no public constructor and so can't be produced by a mock.

use std::io;
use std::path::{Path, PathBuf};

/// Filesystem operations used by the build workflow. Production uses
/// [`RealFilesystem`]; tests inject a mock.
pub trait Filesystem: Send + Sync {
    fn exists(&self, path: &Path) -> bool;
    fn is_dir(&self, path: &Path) -> bool;
    fn is_file(&self, path: &Path) -> bool;
    /// List the immediate entries of `path` (full paths).
    fn read_dir(&self, path: &Path) -> io::Result<Vec<PathBuf>>;
    fn read_to_string(&self, path: &Path) -> io::Result<String>;
    fn write(&self, path: &Path, contents: &[u8]) -> io::Result<()>;
    fn create_dir(&self, path: &Path) -> io::Result<()>;
    fn create_dir_all(&self, path: &Path) -> io::Result<()>;
    fn remove_file(&self, path: &Path) -> io::Result<()>;
    fn remove_dir_all(&self, path: &Path) -> io::Result<()>;
    fn copy(&self, from: &Path, to: &Path) -> io::Result<u64>;
    /// File size in bytes (`metadata().len()`), without exposing `Metadata`.
    fn file_len(&self, path: &Path) -> io::Result<u64>;
    /// Set the unix file mode (`chmod`), replacing the
    /// `metadata().permissions()` + `set_permissions()` dance.
    #[cfg(unix)]
    fn set_mode(&self, path: &Path, mode: u32) -> io::Result<()>;
}

/// Real filesystem backed by `std::fs` (the production implementation).
pub struct RealFilesystem;

impl Filesystem for RealFilesystem {
    fn exists(&self, path: &Path) -> bool {
        path.exists()
    }
    fn is_dir(&self, path: &Path) -> bool {
        path.is_dir()
    }
    fn is_file(&self, path: &Path) -> bool {
        path.is_file()
    }
    fn read_dir(&self, path: &Path) -> io::Result<Vec<PathBuf>> {
        let mut out = Vec::new();
        for entry in std::fs::read_dir(path)? {
            out.push(entry?.path());
        }
        Ok(out)
    }
    fn read_to_string(&self, path: &Path) -> io::Result<String> {
        std::fs::read_to_string(path)
    }
    fn write(&self, path: &Path, contents: &[u8]) -> io::Result<()> {
        std::fs::write(path, contents)
    }
    fn create_dir(&self, path: &Path) -> io::Result<()> {
        std::fs::create_dir(path)
    }
    fn create_dir_all(&self, path: &Path) -> io::Result<()> {
        std::fs::create_dir_all(path)
    }
    fn remove_file(&self, path: &Path) -> io::Result<()> {
        std::fs::remove_file(path)
    }
    fn remove_dir_all(&self, path: &Path) -> io::Result<()> {
        std::fs::remove_dir_all(path)
    }
    fn copy(&self, from: &Path, to: &Path) -> io::Result<u64> {
        std::fs::copy(from, to)
    }
    fn file_len(&self, path: &Path) -> io::Result<u64> {
        Ok(std::fs::metadata(path)?.len())
    }
    #[cfg(unix)]
    fn set_mode(&self, path: &Path, mode: u32) -> io::Result<()> {
        use std::os::unix::fs::PermissionsExt;
        let mut perms = std::fs::metadata(path)?.permissions();
        perms.set_mode(mode);
        std::fs::set_permissions(path, perms)
    }
}

#[cfg(test)]
pub(crate) mod mock {
    //! In-memory filesystem for tests: answers queries from seeded state and
    //! records mutations, performing no real disk I/O.
    use super::Filesystem;
    use std::collections::{HashMap, HashSet};
    use std::io;
    use std::path::{Path, PathBuf};
    use std::sync::Mutex;

    #[derive(Default)]
    pub(crate) struct MockFilesystem {
        files: Mutex<HashMap<PathBuf, Vec<u8>>>,
        dirs: Mutex<HashSet<PathBuf>>,
        /// Paths whose fallible operations should return an io error, to
        /// exercise the workflow's "filesystem op failed → handle → end" paths.
        fail: Mutex<HashSet<PathBuf>>,
        pub removed: Mutex<Vec<PathBuf>>,
        pub written: Mutex<Vec<PathBuf>>,
    }

    impl MockFilesystem {
        /// Seed a file with contents (also registers its ancestor dirs).
        pub fn add_file(&self, path: impl Into<PathBuf>, contents: impl Into<Vec<u8>>) {
            let path = path.into();
            self.register_ancestors(&path);
            self.files.lock().unwrap().insert(path, contents.into());
        }

        /// Make every fallible operation on `path` return an io error.
        pub fn fail_on(&self, path: impl Into<PathBuf>) {
            self.fail.lock().unwrap().insert(path.into());
        }

        fn check_fail(&self, path: &Path) -> io::Result<()> {
            if self.fail.lock().unwrap().contains(path) {
                Err(io::Error::new(io::ErrorKind::Other, "mock filesystem failure"))
            } else {
                Ok(())
            }
        }

        /// Seed a directory as existing.
        pub fn add_dir(&self, path: impl Into<PathBuf>) {
            let path = path.into();
            self.register_ancestors(&path);
            self.dirs.lock().unwrap().insert(path);
        }

        fn register_ancestors(&self, path: &Path) {
            let mut dirs = self.dirs.lock().unwrap();
            let mut cur = path.parent();
            while let Some(p) = cur {
                if p.as_os_str().is_empty() {
                    break;
                }
                dirs.insert(p.to_path_buf());
                cur = p.parent();
            }
        }
    }

    impl Filesystem for MockFilesystem {
        fn exists(&self, path: &Path) -> bool {
            self.files.lock().unwrap().contains_key(path)
                || self.dirs.lock().unwrap().contains(path)
        }
        fn is_dir(&self, path: &Path) -> bool {
            self.dirs.lock().unwrap().contains(path)
        }
        fn is_file(&self, path: &Path) -> bool {
            self.files.lock().unwrap().contains_key(path)
        }
        fn read_dir(&self, path: &Path) -> io::Result<Vec<PathBuf>> {
            self.check_fail(path)?;
            let mut out = Vec::new();
            for p in self.files.lock().unwrap().keys() {
                if p.parent() == Some(path) {
                    out.push(p.clone());
                }
            }
            for p in self.dirs.lock().unwrap().iter() {
                if p.parent() == Some(path) {
                    out.push(p.clone());
                }
            }
            Ok(out)
        }
        fn read_to_string(&self, path: &Path) -> io::Result<String> {
            self.check_fail(path)?;
            match self.files.lock().unwrap().get(path) {
                Some(b) => Ok(String::from_utf8_lossy(b).into_owned()),
                None => Err(io::Error::new(io::ErrorKind::NotFound, "no such mock file")),
            }
        }
        fn write(&self, path: &Path, contents: &[u8]) -> io::Result<()> {
            self.check_fail(path)?;
            self.register_ancestors(path);
            self.written.lock().unwrap().push(path.to_path_buf());
            self.files
                .lock()
                .unwrap()
                .insert(path.to_path_buf(), contents.to_vec());
            Ok(())
        }
        fn create_dir(&self, path: &Path) -> io::Result<()> {
            self.dirs.lock().unwrap().insert(path.to_path_buf());
            Ok(())
        }
        fn create_dir_all(&self, path: &Path) -> io::Result<()> {
            self.register_ancestors(path);
            self.dirs.lock().unwrap().insert(path.to_path_buf());
            Ok(())
        }
        fn remove_file(&self, path: &Path) -> io::Result<()> {
            self.removed.lock().unwrap().push(path.to_path_buf());
            self.files.lock().unwrap().remove(path);
            Ok(())
        }
        fn remove_dir_all(&self, path: &Path) -> io::Result<()> {
            self.check_fail(path)?;
            self.removed.lock().unwrap().push(path.to_path_buf());
            self.dirs.lock().unwrap().remove(path);
            Ok(())
        }
        fn copy(&self, from: &Path, to: &Path) -> io::Result<u64> {
            self.check_fail(from)?;
            self.check_fail(to)?;
            let bytes = self
                .files
                .lock()
                .unwrap()
                .get(from)
                .cloned()
                .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, "no such mock file"))?;
            let len = bytes.len() as u64;
            self.files.lock().unwrap().insert(to.to_path_buf(), bytes);
            Ok(len)
        }
        fn file_len(&self, path: &Path) -> io::Result<u64> {
            self.check_fail(path)?;
            match self.files.lock().unwrap().get(path) {
                Some(b) => Ok(b.len() as u64),
                None => Err(io::Error::new(io::ErrorKind::NotFound, "no such mock file")),
            }
        }
        #[cfg(unix)]
        fn set_mode(&self, _path: &Path, _mode: u32) -> io::Result<()> {
            Ok(())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::mock::MockFilesystem;
    use super::Filesystem;
    use std::path::Path;

    #[test]
    fn mock_answers_queries_and_records_mutations() {
        let fs = MockFilesystem::default();
        fs.add_dir("/repo");
        fs.add_file("/repo/manifest.xml", "<manifest/>");

        // Queries reflect seeded state (ancestors auto-registered).
        assert!(fs.exists(Path::new("/repo")));
        assert!(fs.is_dir(Path::new("/repo")));
        assert!(fs.is_file(Path::new("/repo/manifest.xml")));
        assert!(!fs.exists(Path::new("/repo/missing")));
        assert_eq!(
            fs.read_to_string(Path::new("/repo/manifest.xml")).unwrap(),
            "<manifest/>"
        );
        assert_eq!(fs.read_dir(Path::new("/repo")).unwrap().len(), 1);

        // Mutations are recorded and reflected.
        fs.write(Path::new("/repo/out.txt"), b"hi").unwrap();
        assert!(fs.is_file(Path::new("/repo/out.txt")));
        assert_eq!(fs.file_len(Path::new("/repo/out.txt")).unwrap(), 2);
        assert_eq!(fs.copy(Path::new("/repo/out.txt"), Path::new("/repo/copy.txt")).unwrap(), 2);
        fs.remove_file(Path::new("/repo/out.txt")).unwrap();
        assert!(!fs.is_file(Path::new("/repo/out.txt")));

        assert!(fs.written.lock().unwrap().iter().any(|p| p.ends_with("out.txt")));
        assert!(fs.removed.lock().unwrap().iter().any(|p| p.ends_with("out.txt")));
    }
}
