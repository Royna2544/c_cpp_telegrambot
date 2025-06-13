#pragma once

#include <absl/log/log.h>
#include <unistd.h>

#include <array>
#include <cerrno>

using pipe_t = std::array<int, 2>;

/**
 * @brief A class representing a pipe with two file descriptors.
 *
 * This class represents a pipe with two file descriptors, one for reading and
 * one for writing. It provides methods to check if the pipe is valid,
 * invalidate the pipe, close the pipe, and get the read and write file
 * descriptors.
 */
struct Pipe {
    /**
     * @brief Enum representing the two endpoints of a pipe.
     *
     * This enum represents the two endpoints of a pipe, READ and WRITE.
     */
    enum class EndPoint {
        READ = 0,
        WRITE = 1,
    };

    /**
     * @brief Checks if the pipe is valid.
     *
     * This method checks if the pipe is valid by verifying if both file
     * descriptors are valid.
     *
     * @return True if the pipe is valid, false otherwise.
     */
    [[nodiscard]] bool vaild() const {
        return underlying[0] > 0 && underlying[1] > 0;
    }

    /**
     * @brief Closes the pipe.
     *
     * This method closes the pipe by calling the closeFd method on both file
     * descriptors.
     */
    void close() {
        ::close(underlying[0]);
        ::close(underlying[1]);
        underlying[0] = -1;
        underlying[1] = -1;
    }

    /**
     * @brief Creates a pipe.
     *
     * This method creates a pipe by calling the pipe system call and stores the
     * file descriptors in the underlying array.
     *
     * @return True if the pipe is successfully created, false otherwise.
     */
    bool pipe() {
        int rc = ::pipe(underlying.data());
        PLOG_IF(ERROR, rc != 0) << "Failed to create pipe";
        return rc == 0;
    }

    int operator[](EndPoint ep) const {
        return underlying[static_cast<size_t>(ep)];
    }

    /**
     * @brief Gets the read end file descriptor of the pipe.
     *
     * This method returns the file descriptor for reading from the pipe.
     *
     * @return The file descriptor for reading from the pipe.
     */
    [[nodiscard]] int readEnd() const { return operator[](EndPoint::READ); }

    /**
     * @brief Gets the write end file descriptor of the pipe.
     *
     * This method returns the file descriptor for writing to the pipe.
     *
     * @return The file descriptor for writing to the pipe.
     */
    [[nodiscard]] int writeEnd() const { return operator[](EndPoint::WRITE); }

    /**
     * @brief The underlying array of file descriptors.
     */
    pipe_t underlying;
};

inline std::ostream& operator<<(std::ostream& os, const Pipe& pipe) {
    os << "Pipe[valid=" << pipe.vaild() << ", readEnd=" << pipe.readEnd()
       << ", writeEnd=" << pipe.writeEnd() << "]";
    return os;
}
