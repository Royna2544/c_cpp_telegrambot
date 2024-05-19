#include <absl/log/log.h>
#include <socket/TgBotSocket.h>

#include <optional>
#include <string_view>

#include "TgBotSocketFileHelper.hpp"

using TgBotCommandData::UploadFile;

bool fileData_tofile(const void* ptr, size_t len) {
    const auto* data = static_cast<const char*>(ptr);
    UploadFile destfilepath{};
    FILE* file = nullptr;
    size_t ret = 0;
    size_t file_size = len - sizeof(UploadFile);

    LOG(INFO) << "This buffer has a size of " << len << " bytes";
    LOG(INFO) << "Which is " << file_size << " bytes excluding the header";

    strncpy(destfilepath, data, sizeof(UploadFile));
    if ((file = fopen(destfilepath, "wb")) == nullptr) {
        LOG(ERROR) << "Failed to open file: " << destfilepath;
        return false;
    }
    ret = fwrite(data + sizeof(UploadFile), file_size, 1, file);
    if (ret != 1) {
        LOG(ERROR) << "Failed to write to file: " << destfilepath << " (Wrote "
                   << ret << " bytes)";
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

std::optional<TgBotCommandPacket> fileData_fromFile(TgBotCommand cmd,
                                                    const std::string_view filename,
                                                    const std::string_view destfilepath) {
    constexpr size_t datahdr_size = sizeof(TgBotCommandData::UploadFile);
    size_t size = 0;
    size_t total_size = 0;
    char* buf = nullptr;
    FILE* fp = nullptr;
    std::optional<TgBotCommandPacket> pkt;

    fp = fopen(filename.data(), "rb");
    if (fp != nullptr) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        total_size = size + datahdr_size;
        LOG(INFO) << "Sending file " << filename;
        buf = static_cast<char*>(malloc(total_size));
        LOG(INFO) << "mem-alloc buffer of size " << total_size << " bytes";
        // Copy data header to the beginning of the buffer.
        if (buf != nullptr) {
            strncpy(buf, destfilepath.data(), datahdr_size);
            char* moved_buf = buf + datahdr_size;
            fread(moved_buf, 1, size, fp);
            pkt = TgBotCommandPacket(cmd, buf, total_size);
            free(buf);
        }
        fclose(fp);
    } else {
        LOG(ERROR) << "Failed to open file " << filename;
    }
    return pkt;
}