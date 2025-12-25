#pragma once

#include <SocketExports.h>

#include <SharedMalloc.hpp>
#include <socket/api/DataStructures.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ios>
#include <optional>
#include <trivial_helpers/fruit_inject.hpp>

// Represents a SHA-256 hash
struct SOCKET_EXPORT HashContainer {
    TgBotSocket::SHA256StringArray m_data;
};

inline std::ostream& operator<<(std::ostream& self, const HashContainer& data) {
    for (const auto& c : data.m_data) {
        self << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(c);
    }
    return self;
}

struct SOCKET_EXPORT VFSOperations {
    virtual ~VFSOperations() = default;

    /**
     * @brief Writes a block of memory to a file.
     *
     * @param filename The path where the file should be written.
     * @param startAddr A pointer to the start of the memory block to be
     * written.
     * @param size The size of the memory block in bytes.
     * @return true if the file was written successfully, false otherwise.
     */
    virtual bool writeFile(const std::filesystem::path& filename,
                           const uint8_t* startAddr, const size_t size) = 0;

    /**
     * @brief Reads the contents of a file into memory.
     *
     * @param filename The path of the file to be read.
     * @return std::optional<SharedMalloc> containing the data read from the
     * file if successful, otherwise std::nullopt if the file could not be read.
     */
    virtual std::optional<SharedMalloc> readFile(
        const std::filesystem::path& filename) = 0;

    /**
     * @brief Checks if a file or directory exists at the specified path.
     *
     * @param path The path of the file or directory to check.
     * @return true if the file or directory exists, false otherwise.
     */
    virtual bool exists(const std::filesystem::path& path) = 0;

    /**
     * @brief Calculates the SHA-256 hash of a given memory block.
     *
     * @param memory The memory block whose SHA-256 hash is to be calculated.
     * @param data The container where the calculated hash will be stored.
     */
    virtual void SHA256(const SharedMalloc& memory, HashContainer& data) = 0;
};

struct SOCKET_EXPORT RealFS : public VFSOperations {
    ~RealFS() override = default;
    APPLE_INJECT(RealFS()) = default;

    /**
     * @brief Writes a block of memory to a file.
     *
     * @param filename The path where the file should be written.
     * @param startAddr A pointer to the start of the memory block to be
     * written.
     * @param size The size of the memory block in bytes.
     * @return true if the file was written successfully, false otherwise.
     */
    bool writeFile(const std::filesystem::path& filename,
                   const uint8_t* startAddr, const size_t size) override;

    /**
     * @brief Reads the contents of a file into memory.
     *
     * @param filename The path of the file to be read.
     * @return std::optional<SharedMalloc> containing the data read from the
     * file if successful, otherwise std::nullopt if the file could not be read.
     */
    std::optional<SharedMalloc> readFile(
        const std::filesystem::path& filename) override;

    /**
     * @brief Checks if a file or directory exists at the specified path.
     *
     * @param path The path of the file or directory to check.
     * @return true if the file or directory exists, false otherwise.
     */
    bool exists(const std::filesystem::path& path) override;

    /**
     * @brief Calculates the SHA-256 hash of a given memory block.
     *
     * @param memory The memory block whose SHA-256 hash is to be calculated.
     * @param data The container where the calculated hash will be stored.
     */
    void SHA256(const SharedMalloc& memory, HashContainer& data) override;
};

class SOCKET_EXPORT SocketFile2DataHelper {
    VFSOperations* vfs;

   public:
    APPLE_EXPLICIT_INJECT(SocketFile2DataHelper(VFSOperations* vfs))
        : vfs(vfs) {}

    struct Params {
        std::filesystem::path filepath;
        std::filesystem::path destfilepath;
        TgBotSocket::SHA256StringArray hash;
        TgBotSocket::data::FileTransferMeta::Options options;

        // Added field, used only in parsing
        TgBotSocket::Packet::Header::length_type file_size;
        const std::uint8_t* filebuffer;
    };

    bool ReceiveTransferMeta(const Params& params);

    std::optional<TgBotSocket::Packet> CreateTransferMeta(
        const Params& params,
        const TgBotSocket::Packet::Header::session_token_type& session_token,
        const TgBotSocket::PayloadType type,
        bool isRequest);
};
