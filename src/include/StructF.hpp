#pragma once

#include <absl/log/absl_log.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string_view>

struct F {
    using size_type = size_t;

    // Constructor for FILE* managed by *this
    F() : handle(nullptr, &fclose), external_managed(false) {}
    // Constructor for externally managed FILE*
    explicit F(FILE* handle)
        : handle(handle, [](FILE*) { return 0; }), external_managed(true) {}

    operator FILE*() const { return handle.get(); }
    // Do not allow implicit cast to bool. Which is implied by operator FILE*
    operator bool() const = delete;

    // No copy constructor
    F(const F&) = delete;
    F& operator=(const F&) = delete;
    F(F&& other) noexcept
        : handle(std::move(other.handle)),
          external_managed(other.external_managed) {
        // Reset the flag in the moved-from object.
        other.external_managed = false;
    }

    F& operator=(F&& other) noexcept {
        if (this != &other) {
            handle = std::move(other.handle);
            external_managed = other.external_managed;
            // Reset the flag in the moved-from object.
            other.external_managed = false;
        }
        return *this;
    }

    F& operator=(std::nullptr_t) {
        handle.reset();
        external_managed = false;
        return *this;
    }

    /**
     * @enum Mode
     * @brief Defines file operation modes and bitmasks for configuring file
     * access.
     *
     * This enum class represents different modes for file operations. Each mode
     * can be used individually or combined using bitwise operations to specify
     * multiple modes.
     *
     * The modes available are:
     * - Read:       Enables reading from a file (1).
     * - Write:      Enables writing to a file (1 << 1, or 2).
     * - Append:     Enables appending data to the end of the file (1 << 2, or
     * 4).
     * - BinaryMask: Indicates that the file should be treated as binary (1 <<
     * 3, or 8).
     *
     * Example usage:
     * @code
     * Mode combinedMode = Mode::Read | Mode::Write;
     * if (combinedMode & Mode::Read) {
     *     // Perform read operation
     * }
     * @endcode
     */
    enum class Mode : std::int8_t {
        Read = 1,
        Write = 1 << 1,
        Append = 1 << 2,
        BinaryMask = 1 << 3,

        ReadBinary = Read | BinaryMask,
        WriteBinary = Write | BinaryMask,
        AppendBinary = Append | BinaryMask,
    };

    struct Result {
        bool success;
        enum class Reason : std::int8_t {
            kNone,
            kHandleNull,
            kIOFailure,
        } reason;

        operator bool() const { return success; }

        static Result ok() {
            return Result{.success = true, .reason = Reason::kNone};
        }
        static Result error(Reason reason) {
            return Result{.success = false, .reason = reason};
        }
        static Result ok_or(const bool cond, const Reason ifFailed) {
            return cond ? Result::ok() : Result::error(ifFailed);
        }
    };

    /**
     * @brief Opens a file with the specified filename and mode.
     *
     * Opens a file for reading or writing based on the mode provided.
     * The mode is expected to be compatible with the `fopen` file mode.
     *
     * @param filename The name of the file to open.
     * @param mode The mode in which to open the file. This could be an integer
     * or enum representing modes like read, write, append, etc.
     * @return A `Result` indicating whether the operation was successful or
     * failed.
     *         - Success: `Result::ok()`
     *         - Failure: `Result::error(Reason::kIOFailure)` if the file could
     * not be opened.
     */
    [[nodiscard]] Result open(const std::string_view filename, const Mode mode);

    // Overload!
    [[nodiscard]] Result open(const std::filesystem::path& filename,
                              const Mode mode) {
        // Calls the string overload
        return open(filename.string(), mode);
    }

    // Explicit overload to std::string (Which can be converted to both fs::path
    // and string_view)
    [[nodiscard]] Result open(const std::string& filename, const Mode mode) {
        const std::string_view fileView = filename;
        // Calls the string_view overload
        return open(fileView, mode);
    }

    [[nodiscard]] Result open(const char* filename, const Mode mode) {
        return open(std::string_view(filename), mode);
    }

    /**
     * @brief Writes data to the file.
     *
     * Writes `nBytes` number of elements of size `size` from the buffer pointed
     * to by `ptr` into the file. It performs an unbuffered write operation
     * using `std::fwrite`.
     *
     * @param ptr A pointer to the data buffer to write.
     * @param size The size of each element in bytes.
     * @param nBytes The number of elements to write.
     * @return A `Result` indicating whether the write operation was successful
     * or failed.
     *         - Success: `Result::ok()`
     *         - Failure: `Result::error(Reason::kHandleNull)` if the file
     * handle is null. `Result::error(Reason::kIOFailure)` if not all bytes were
     * written.
     */
    [[nodiscard]] Result write(const void* __restrict ptr, size_t size,
                               size_t nBytes) const;

    /**
     * @brief Reads data from the file.
     *
     * Reads `nBytes` number of elements of size `size` into the buffer pointed
     * to by `ptr` from the file. It performs an unbuffered read operation using
     * `std::fread`.
     *
     * @param ptr A pointer to the buffer where the data should be stored.
     * @param size The size of each element to be read, in bytes.
     * @param nBytes The number of elements to read.
     * @return A `Result` indicating whether the read operation was successful
     * or failed.
     *         - Success: `Result::ok()`
     *         - Failure: `Result::error(Reason::kHandleNull)` if the file
     * handle is null. `Result::error(Reason::kIOFailure)` if not all bytes were
     * read.
     */
    [[nodiscard]] Result read(void* ptr, size_t size, size_t nBytes) const;

    /**
     * @brief Writes a null-terminated string to the file.
     *
     * Writes the given string to the file using `std::fputs`.
     *
     * @param text The string to write to the file.
     * @return A `Result` indicating whether the operation was successful or
     * failed.
     *         - Success: `Result::ok()`
     *         - Failure: `Result::error(Reason::kHandleNull)` if the file
     * handle is null. `Result::error(Reason::kIOFailure)` if the string could
     * not be written.
     */
    [[nodiscard]] Result puts(const std::string_view text) const;

    /**
     * @brief Writes a single character to the file.
     *
     * Writes a single character to the file using `std::fputc`.
     *
     * @param c The character to write.
     * @return A `Result` indicating whether the operation was successful or
     * failed.
     *         - Success: `Result::ok()`
     *         - Failure: `Result::error(Reason::kHandleNull)` if the file
     * handle is null. `Result::error(Reason::kIOFailure)` if the character
     * could not be written.
     */
    [[nodiscard]] Result putc(const char c) const;

    /**
     * @brief Closes the currently open file.
     *
     * Closes the file if the file handle is valid and the file is not
     * externally managed (i.e., `stdout`, `stderr`). If the file is externally
     * managed, it will not be closed.
     */
    Result close();

    /**
     * @brief Calculates the file size of the underlying fp.
     *
     * Calculates and returns the size of the file in bytes.
     */
    [[nodiscard]] size_type size() const;

    // Represents the invalid file size returned by size().
    constexpr static size_type INVALID_SIZE = -1;

   private:
    using HANDLE = std::unique_ptr<FILE, int (*)(FILE*)>;
    HANDLE handle;
    bool external_managed;

    static std::string_view constructFileMode(const Mode mode);
};

inline std::ostream& operator<<(std::ostream& self,
                                const F::Result::Reason& reason) {
    switch (reason) {
        case F::Result::Reason::kNone:
            return self << "kNone";
        case F::Result::Reason::kHandleNull:
            return self << "kHandleNull";
        case F::Result::Reason::kIOFailure:
            return self << "kIOFailure";
    }
    return self;
}

inline F::Result F::open(const std::string_view filename, const Mode mode) {
    Result closeRes = close();
    if (!closeRes) {
        return closeRes;
    }
    handle.reset(std::fopen(filename.data(), constructFileMode(mode).data()));
    if (handle == nullptr) {
        ABSL_LOG(ERROR) << "Failed to open file: " << std::quoted(filename);
        return Result::error(Result::Reason::kIOFailure);
    }
    return Result::ok();
}

inline F::Result F::write(const void* __restrict ptr, size_t size,
                          size_t nBytes) const {
    if (handle == nullptr) {
        ABSL_LOG(ERROR) << "File handle is null";
        return Result::error(Result::Reason::kHandleNull);
    }
    return Result::ok_or(std::fwrite(ptr, size, nBytes, handle.get()) == nBytes,
                         Result::Reason::kIOFailure);
}

inline F::Result F::read(void* ptr, size_t size, size_t nBytes) const {
    if (handle == nullptr) {
        ABSL_LOG(ERROR) << "File handle is null";
        return Result::error(Result::Reason::kHandleNull);
    }
    return Result::ok_or(std::fread(ptr, size, nBytes, handle.get()) == nBytes,
                         Result::Reason::kIOFailure);
}

inline F::Result F::puts(const std::string_view text) const {
    if (handle == nullptr) {
        ABSL_LOG(ERROR) << "File handle is null";
        return Result::error(Result::Reason::kHandleNull);
    }
    return Result::ok_or(std::fputs(text.data(), handle.get()) != EOF,
                         Result::Reason::kIOFailure);
}

inline F::Result F::putc(const char c) const {
    if (handle == nullptr) {
        ABSL_LOG(ERROR) << "File handle is null";
        return Result::error(Result::Reason::kHandleNull);
    }
    return Result::ok_or(std::fputc(c, handle.get()) != EOF,
                         Result::Reason::kIOFailure);
}

inline F::Result F::close() {
    if (handle && !external_managed) {
        if (fclose(handle.release()) != 0) {
            ABSL_LOG(ERROR) << "Failed to close file";
            return Result::error(Result::Reason::kIOFailure);
        }
    }
    handle.reset();
    external_managed = false;
    return Result::ok();
}

inline F::size_type F::size() const {
    if (handle == nullptr) {
        ABSL_LOG(ERROR) << "File handle is null";
        return INVALID_SIZE;
    }
    long current = ftell(handle.get());
    if (current == -1) {
        ABSL_LOG(ERROR) << "Failed to get current position in file";
        return INVALID_SIZE;
    }
    F::size_type size = fseek(handle.get(), 0, SEEK_END);
    if (size == -1) {
        ABSL_LOG(ERROR) << "Failed to seek to end of file";
        return INVALID_SIZE;
    }
    if (fseek(handle.get(), current, SEEK_SET) == -1) {
        ABSL_LOG(ERROR) << "Failed to seek back to current position in file";
        return INVALID_SIZE;
    }
    return size;
}

inline std::string_view F::constructFileMode(const Mode mode) {
    switch (mode) {
        case Mode::Read:
            return "r";
        case Mode::Write:
            return "w";
        case Mode::Append:
            return "a";
        case Mode::ReadBinary:
            return "rb";
        case Mode::WriteBinary:
            return "wb";
        case Mode::AppendBinary:
            return "ab";
        default:
            ABSL_LOG(ERROR) << "Invalid file mode: " << static_cast<int>(mode);
            return "";
    }
}

struct StdoutF : public F {
    StdoutF() : F(stdout) {}
};

struct StderrF : public F {
    StderrF() : F(stderr) {}
};