#include "PacketParser.hpp"

#include <absl/log/log.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <TgBotSocket_Export.hpp>
#include <chrono>
#include <cstring>
#include <optional>
#include <socket/TgBotCommandMap.hpp>
#include <trivial_helpers/raii.hpp>

#include "SharedMalloc.hpp"

template <>
struct fmt::formatter<TgBotSocket::PayloadType> : formatter<std::string_view> {
    using Command = TgBotSocket::Command;

    // parse is inherited from formatter<string_view>.
    auto format(TgBotSocket::PayloadType c,
                format_context& ctx) const -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case TgBotSocket::PayloadType::Binary:
                name = "binary";
                break;
            case TgBotSocket::PayloadType::Json:
                name = "json";
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

namespace {

auto computeHMAC(const SharedMalloc& data,
                 const TgBotSocket::Packet::Header::session_token_type& key) {
    // Buffer to store the HMAC result
    TgBotSocket::Packet::Header::hmac_type hmac_result{};
    unsigned int hmac_length = 0;

    // Compute the HMAC
    HMAC(EVP_sha256(), key.data(), key.size(),
         static_cast<const std::uint8_t*>(data.get()), data.size(),
         hmac_result.data(), &hmac_length);
    return hmac_result;
}

SharedMalloc encrypt_payload(
    const TgBotSocket::Packet::Header::session_token_type key,
    const SharedMalloc& payload,
    TgBotSocket::Packet::Header::init_vector_type& iv) {
    using Header = TgBotSocket::Packet::Header;
    // Generate random IV
    RAND_bytes(iv.data(), Header::IV_LENGTH);

    SharedMalloc encrypted(payload.size() + Header::TAG_LENGTH);
    int len = 0;
    int encrypted_payload_len = 0;

    // Initialize encryption
    auto ctx = RAII<EVP_CIPHER_CTX*>::create<void>(EVP_CIPHER_CTX_new(),
                                                   &EVP_CIPHER_CTX_free);
    if (ctx == nullptr) {
        LOG(ERROR) << "Error initializing encryption context";
        return {};
    }

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                           nullptr) == 0) {
        LOG(ERROR) << "Error initializing encryption";
        return {};
    }

    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr,
                           reinterpret_cast<const std::uint8_t*>(key.data()),
                           iv.data()) == 0) {
        LOG(ERROR) << "Error initializing encryption with key";
    }

    // Encrypt the plaintext
    auto* loc = static_cast<std::uint8_t*>(encrypted.get());
    if (EVP_EncryptUpdate(ctx.get(), loc, &len,
                          static_cast<std::uint8_t*>(payload.get()),
                          payload.size()) == 0) {
        LOG(ERROR) << "Error encrypting payload";
        return {};
    }
    encrypted_payload_len += len;

    // Finalize encryption
    if (EVP_EncryptFinal_ex(ctx.get(), loc + encrypted_payload_len, &len) ==
        0) {
        LOG(ERROR) << "Error finalizing encryption";
        return {};
    }
    encrypted_payload_len += len;

    // Get the authentication tag and append it
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, Header::TAG_LENGTH,
                            loc + encrypted_payload_len) == 0) {
        LOG(ERROR) << "Error getting authentication tag";
        return {};
    }

    encrypted_payload_len += Header::TAG_LENGTH;
    encrypted.resize(encrypted_payload_len);

    ctx.reset();
    DLOG(INFO) << "Encrypted payload of size " << encrypted_payload_len
               << " bytes using EVP AES_256";
    return encrypted;
}

SharedMalloc decrypt_payload(
    const TgBotSocket::Packet::Header::session_token_type& key,
    const SharedMalloc& encrypted,
    const TgBotSocket::Packet::Header::init_vector_type& iv) {
    constexpr int tag_size = TgBotSocket::Packet::Header::TAG_LENGTH;

    // Ensure the encrypted size is valid
    if (encrypted.size() < tag_size) {
        LOG(ERROR) << "Encrypted payload size too small to contain tag";
        return {};
    }

    const size_t decrypted_size = encrypted.size() - tag_size;
    SharedMalloc decrypted(decrypted_size);
    int len = 0;
    int decryped_len = 0;

    // Initialize decryption
    auto ctx = RAII<EVP_CIPHER_CTX*>::create<void>(EVP_CIPHER_CTX_new(),
                                                   &EVP_CIPHER_CTX_free);
    if (ctx == nullptr) {
        LOG(ERROR) << "Failed to create EVP_CIPHER_CTX";
        return {};
    }

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr,
                           nullptr) == 0) {
        LOG(ERROR) << "Failed to initialize decryption cipher";
        return {};
    }

    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.data()),
                           iv.data()) == 0) {
        LOG(ERROR) << "Failed to set key and IV for decryption";
        return {};
    }

    // Decrypt the ciphertext
    if (EVP_DecryptUpdate(ctx.get(),
                          static_cast<std::uint8_t*>(decrypted.get()), &len,
                          static_cast<const std::uint8_t*>(encrypted.get()),
                          decrypted_size) == 0) {
        LOG(ERROR) << "Failed during EVP_DecryptUpdate";
        return {};
    }
    decryped_len += len;

    // Set the authentication tag
    auto* tag_loc =
        static_cast<std::uint8_t*>(encrypted.get()) + decrypted_size;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag_size,
                            tag_loc) == 0) {
        LOG(ERROR) << "Failed to set authentication tag";
        return {};
    }

    // Finalize decryption
    auto* out_loc = static_cast<std::uint8_t*>(decrypted.get()) + decryped_len;
    if (EVP_DecryptFinal_ex(ctx.get(), out_loc, &len) <= 0) {
        LOG(ERROR) << "Authentication tag mismatch";
        return {};
    }

    decryped_len += len;
    decrypted.resize(decryped_len);

    ctx.reset();
    LOG(INFO) << "Decrypted payload successfully, size: " << decryped_len;
    return decrypted;
}

}  // namespace

namespace TgBotSocket {

std::optional<Packet> readPacket(const TgBotSocket::Context& context) {
    TgBotSocket::Packet::Header header;
    decltype(header.magic) magic{};

    // Read header and check magic value, despite the header size, the magic was
    // always the first element of the struct.
    const auto magicData = context.read(sizeof(magic));
    if (!magicData) {
        LOG(ERROR) << "While reading magic, failed";
        return std::nullopt;
    }
    magicData->assignTo(magic);

    auto diff = magic - TgBotSocket::Packet::Header::MAGIC_VALUE_BASE;
    if (diff != TgBotSocket::Packet::Header::DATA_VERSION) {
        LOG(WARNING) << "Invalid magic value, dropping buffer";
        constexpr int reasonable_datadiff = 5;
        // Only a small difference is worth logging.
        diff = abs(diff);
        if (diff >= 0 && diff < TgBotSocket::Packet::Header::DATA_VERSION +
                                    reasonable_datadiff) {
            LOG(INFO) << "This packet contains header data version " << diff
                      << ", but we have version "
                      << TgBotSocket::Packet::Header::DATA_VERSION;
        } else {
            LOG(INFO) << "This is probably not a valid packet";
        }
        return std::nullopt;
    }

    // Read rest of the packet header.
    const auto headerData =
        context.read(sizeof(TgBotSocket::Packet::Header) - sizeof(magic));
    if (!headerData) {
        LOG(ERROR) << "While reading header, failed";
        return std::nullopt;
    }
    headerData->assignTo((decltype(magic)*)&header + 1,
                         sizeof(header) - sizeof(magic));
    header.magic = magic;

    LOG(INFO) << fmt::format("Received Packet{{cmd={}, data_type={}}}",
                             header.cmd, header.data_type);

    TgBotSocket::Packet packet{
        .header = header, .data = {}  // Will be filled in the next step.
    };

    packet.header = header;
    if (packet.header.data_size == 0) {
        // In this case, no need to fetch or verify data
        return packet;
    }
    auto data = context.read(header.data_size);
    if (!data) {
        LOG(ERROR) << "While reading data, failed";
        return std::nullopt;
    }
    packet.data = *data;
    if (!decryptPacket(packet)) {
        return std::nullopt;
    }
    return packet;
}

bool Socket_API decryptPacket(TgBotSocket::Packet& packet) {
    auto& header = packet.header;
    auto& data = packet.data;

    if (header.session_token ==
        TgBotSocket::Packet::Header::session_token_type{}) {
        LOG(WARNING)
            << "No session token provided, decryption will be skipped.";
        return true;
    }
    if (packet.header.hmac != computeHMAC(data, header.session_token)) {
        LOG(ERROR) << "HMAC mismatch";
        return false;
    }
    packet.data =
        decrypt_payload(header.session_token, data, packet.header.init_vector);
    if (!static_cast<bool>(packet.data)) {
        LOG(ERROR) << "Decryption failed";
        return false;
    }
    // Update the header data size
    // TODO: Is this okay?
    packet.header.data_size = packet.data.size();
    return true;
}

Packet Socket_API
createPacket(const Command command, const void* data,
             Packet::Header::length_type length, const PayloadType payloadType,
             const Packet::Header::session_token_type& sessionToken) {
    Packet packet{.header = {}, .data = {}};
    packet.header.cmd = command;
    packet.header.magic = Packet::Header::MAGIC_VALUE;
    packet.header.data_type = payloadType;
    packet.header.session_token = sessionToken;

    using namespace std::chrono;
    packet.header.nonce =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count() +
        rand();  // Add some randomness to the nonce

    if (data != nullptr && length > 0) {
        packet.data.resize(length);
        packet.data.assignFrom(data, length);

        if (sessionToken != Packet::Header::session_token_type{}) {
            packet.data = encrypt_payload(sessionToken, packet.data,
                                          packet.header.init_vector);
            packet.header.hmac = computeHMAC(packet.data, sessionToken);
        } else {
            LOG(WARNING)
                << "No session token provided, encryption will be skipped";
        }
        packet.header.data_size = packet.data.size();
    }
    return packet;
}

std::optional<Json::Value> Socket_API
parseAndCheck(const void* buf, TgBotSocket::Packet::Header::length_type length,
              const std::initializer_list<const char*> nodes) {
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(std::string(static_cast<const char*>(buf), length),
                      root)) {
        LOG(WARNING) << "Failed to parse json: "
                     << reader.getFormattedErrorMessages();
        return std::nullopt;
    }
    if (!root.isObject()) {
        LOG(WARNING) << "Expected an object in json";
        return std::nullopt;
    }
    for (const auto& node : nodes) {
        if (!root.isMember(node)) {
            LOG(WARNING) << fmt::format("Missing node '{}' in json", node);
            return std::nullopt;
        }
    }
    return root;
}

Packet Socket_API
nodeToPacket(const Command& command, const Json::Value& json,
             const Packet::Header::session_token_type& session_token) {
    std::string result;
    Json::FastWriter writer;
    result = writer.write(json);
    auto packet = createPacket(command, result.c_str(), result.size(),
                               PayloadType::Json, session_token);
    return packet;
}

}  // namespace TgBotSocket