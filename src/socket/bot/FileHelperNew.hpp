#pragma once

#include <SocketExports.h>

#include <SharedMalloc.hpp>
#include <TgBotSocket_Export.hpp>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ios>
#include <optional>
#include <trivial_helpers/fruit_inject.hpp>

// Represents a SHA-256 hash
struct Socket_API HashContainer {
    TgBotSocket::SHA256StringArray m_data;
};

inline std::ostream& operator<<(std::ostream& self, const HashContainer& data) {
    for (const auto& c : data.m_data) {
        self << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(c);
    }
    return self;
}

struct Socket_API VFSOperations {
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

struct Socket_API RealFS : public VFSOperations {
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

class Socket_API SocketFile2DataHelper {
    VFSOperations* vfs;

   public:
    APPLE_EXPLICIT_INJECT(SocketFile2DataHelper(VFSOperations* vfs))
        : vfs(vfs) {}

    enum class Pass {
        UPLOAD_FILE_DRY,
        UPLOAD_FILE,
        DOWNLOAD_FILE,
    };

    struct DataFromFileParam {
        std::filesystem::path filepath;
        std::filesystem::path destfilepath;
        TgBotSocket::data::UploadFile::Options options;
    };

    template <Pass P>
    bool DataToFile(const void* ptr,
                    TgBotSocket::Packet::Header::length_type len) {
        if constexpr (P == Pass::UPLOAD_FILE) {
            return DataToFile_UPLOAD_FILE(ptr, len);
        } else if constexpr (P == Pass::DOWNLOAD_FILE) {
            return DataToFile_DOWNLOAD_FILE(ptr, len);
        } else if constexpr (P == Pass::UPLOAD_FILE_DRY) {
            return DataToFile_UPLOAD_FILE_DRY(ptr, len);
        }
        return false;
    }

    template <Pass P>
    std::optional<TgBotSocket::Packet> DataFromFile(
        const DataFromFileParam& params,
        const TgBotSocket::Packet::Header::session_token_type& session_token) {
        if (P == Pass::UPLOAD_FILE_DRY) {
            return DataFromFile_UPLOAD_FILE_DRY(params, session_token);
        } else if (P == Pass::UPLOAD_FILE) {
            return DataFromFile_UPLOAD_FILE(params, session_token);
        } else if (P == Pass::DOWNLOAD_FILE) {
            return DataFromFile_DOWNLOAD_FILE(params, session_token);
        }
        return std::nullopt;
    }

   private:
    bool DataToFile_UPLOAD_FILE_DRY(
        const void* ptr, TgBotSocket::Packet::Header::length_type len);
    bool DataToFile_UPLOAD_FILE(const void* ptr,
                                TgBotSocket::Packet::Header::length_type len);
    bool DataToFile_DOWNLOAD_FILE(const void* ptr,
                                  TgBotSocket::Packet::Header::length_type len);

    std::optional<TgBotSocket::Packet> DataFromFile_UPLOAD_FILE(
        const DataFromFileParam& params,
        const TgBotSocket::Packet::Header::session_token_type& session_token);
    std::optional<TgBotSocket::Packet> DataFromFile_UPLOAD_FILE_DRY(
        const DataFromFileParam& params,
        const TgBotSocket::Packet::Header::session_token_type& session_token);
    std::optional<TgBotSocket::Packet> DataFromFile_DOWNLOAD_FILE(
        const DataFromFileParam& params,
        const TgBotSocket::Packet::Header::session_token_type& session_token);
};
