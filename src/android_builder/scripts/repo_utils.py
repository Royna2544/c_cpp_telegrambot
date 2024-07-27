import subprocess_utils

def repo_init(url: str, branch: str) -> bool:
    return subprocess_utils.run_command(
        f'repo init -u {url} -b {branch} --git-lfs --depth=1'
    )
        
def repo_sync(jobs: int) -> bool:
    return subprocess_utils.run_command(
        f'repo sync -c -j{jobs} --force-sync --no-clone-bundle --no-tags --force-remove-dirty'
    )