#include "PacketParser.hpp"

#include <absl/log/log.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <socket/api/CoreTypes.hpp>
#include <chrono>
#include <cstring>
#include <optional>
#include "../CommandMap.hpp"
#include <trivial_helpers/raii.hpp>

#ifdef ENABLE_HEXDUMP
#include <lib/hexdump.h>
#endif

template <>
struct fmt::formatter<TgBotSocket::PayloadType> : formatter<std::string_view> {
    using Command = TgBotSocket::Command;

    // parse is inherited from formatter<string_view>.
    auto format(TgBotSocket::PayloadType c, format_context& ctx) const
        -> format_context::iterator {
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

auto computeHMAC(const TgBotSocket::Packet& packet) {
    // Buffer to store the HMAC result
    TgBotSocket::Packet::hmac_type hmac_result{};

    std::unique_ptr<char, decltype(&free)> sha256(strdup("SHA256"), &free);
    const OSSL_PARAM params[] = {
        OSSL_PARAM_utf8_string("digest", sha256.get(), strlen(sha256.get())),
        OSSL_PARAM_END};

    std::unique_ptr<EVP_MAC, decltype(&EVP_MAC_free)> mac{
        EVP_MAC_fetch(nullptr, "HMAC", nullptr), &EVP_MAC_free};
    if (!mac) {
        LOG(ERROR) << "HMAC unsupported in this openssl";

        return hmac_result;
    }

    // Compute the HMAC
    std::unique_ptr<EVP_MAC_CTX, decltype(&EVP_MAC_CTX_free)> ctx{
        EVP_MAC_CTX_new(mac.get()), &EVP_MAC_CTX_free};

    if (!ctx) {
        LOG(ERROR) << "Cannot alloc ctx";
        return hmac_result;
    }

    const auto& key = packet.header.session_token;
    if (!EVP_MAC_init(ctx.get(), (const std::uint8_t*)key.data(), key.size(),
                      params)) {
        LOG(ERROR) << "Cannot init";
        return hmac_result;
    }

    // Add header
    if (!EVP_MAC_update(ctx.get(), (const std::uint8_t*)&packet.header,
                        sizeof(packet.header))) {
#ifdef ENABLE_HEXDUMP
        DLOG(INFO) << "ComputeHMAC - DUMP header";
        hexdump((const uint8_t*)&packet.header, sizeof(packet.header));
#endif
        LOG(ERROR) << "Cannot update #1";
        return hmac_result;
    }

    // Add data if exist
    if (packet.header.data_size != 0) {
#ifdef ENABLE_HEXDUMP
        DLOG(INFO) << "ComputeHMAC - DUMP data";
        hexdump(packet.data.get(), packet.data.size());
#endif
        if (!EVP_MAC_update(ctx.get(), packet.data.get(), packet.data.size())) {
            LOG(ERROR) << "Cannot update #2";
            return hmac_result;
        }
    }

    size_t mac_len;
    if (!EVP_MAC_final(ctx.get(), hmac_result.data(), &mac_len,
                       hmac_result.size())) {
        LOG(ERROR) << "Cannot finalize";
    }

#ifdef ENABLE_HEXDUMP
    DLOG(INFO) << "ComputeHMAC - DUMP computed HMAC result";
    hexdump((const uint8_t*)hmac_result.data(), hmac_result.size());
#endif
    return hmac_result;
}

SharedMalloc encrypt_payload(
    const TgBotSocket::Packet::Header::session_token_type key,
    const SharedMalloc& payload,
    TgBotSocket::Packet::Header::init_vector_type& iv) {
    using namespace TgBotSocket;
    
    // Generate random IV
    RAND_bytes(iv.data(), Crypto::IV_LENGTH);

    SharedMalloc encrypted(payload.size() + Crypto::TAG_LENGTH);
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
    auto* loc = encrypted.get();
    if (EVP_EncryptUpdate(ctx.get(), loc, &len, payload.get(),
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
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, Crypto::TAG_LENGTH,
                            loc + encrypted_payload_len) == 0) {
        LOG(ERROR) << "Error getting authentication tag";
        return {};
    }

    encrypted_payload_len += Crypto::TAG_LENGTH;
    encrypted.resize(encrypted_payload_len);

    DLOG(INFO) << "Encrypted payload of size " << encrypted_payload_len
               << " bytes using EVP AES_256";
    return encrypted;
}

SharedMalloc decrypt_payload(
    const TgBotSocket::Packet::Header::session_token_type& key,
    const SharedMalloc& encrypted,
    const TgBotSocket::Packet::Header::init_vector_type& iv) {
    using namespace TgBotSocket;
    constexpr int tag_size = Crypto::TAG_LENGTH;

    if (encrypted.size() == 0) {
        // No data
        return {};
    }

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
    if (EVP_DecryptUpdate(ctx.get(), decrypted.get(), &len, encrypted.get(),
                          decrypted_size) == 0) {
        LOG(ERROR) << "Failed during EVP_DecryptUpdate";
        return {};
    }
    decryped_len += len;

    // Set the authentication tag
    auto* tag_loc = encrypted.get() + decrypted_size;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, tag_size,
                            tag_loc) == 0) {
        LOG(ERROR) << "Failed to set authentication tag";
        return {};
    }

    // Finalize decryption
    auto* out_loc = decrypted.get() + decryped_len;
    if (EVP_DecryptFinal_ex(ctx.get(), out_loc, &len) <= 0) {
        LOG(ERROR) << "Authentication tag mismatch";
        return {};
    }

    decryped_len += len;
    decrypted.resize(decryped_len);

    LOG(INFO) << "Decrypted payload successfully, size: " << decryped_len;
    return decrypted;
}

}  // namespace

namespace TgBotSocket {

std::optional<Packet> readPacket(const TgBotSocket::Context& context) {
    TgBotSocket::Packet packet;

    const auto headerData = context.read(sizeof(TgBotSocket::Packet::Header));
    if (!headerData) {
        LOG(ERROR) << "While reading header, failed";
        return std::nullopt;
    }
    headerData->assignTo(packet.header);

    auto diff = packet.header.magic - Protocol::MAGIC_VALUE_BASE;
    if (diff != Protocol::DATA_VERSION) {
        LOG(WARNING) << "Invalid magic value, dropping buffer";
        constexpr int reasonable_datadiff = 5;
        // Only a small difference is worth logging.
        diff = ::std::abs(diff);
        if (diff >= 0 && diff < Protocol::DATA_VERSION) {
            LOG(INFO) << "This packet contains header data version " << diff
                      << ", but we have version " << Protocol::DATA_VERSION;
        } else {
            LOG(INFO) << "This is probably not a valid packet";
        }
        return std::nullopt;
    }

    LOG(INFO) << fmt::format(
        "Received Packet{{cmd={}, data_type={}, data_size={}}}",
        packet.header.cmd, packet.header.data_type, packet.header.data_size);

    if (packet.header.data_size != 0) {
        auto data = context.read(packet.header.data_size);
        if (!data) {
            LOG(ERROR) << "While reading data, failed";
            return std::nullopt;
        }
        packet.data = *data;
    }

    auto hmac = context.read(packet.hmac.size());
    if (!hmac) {
        LOG(ERROR) << "while reading hmac, failed";
        return std::nullopt;
    }
    hmac->assignTo(packet.hmac);

    if (packet.hmac == Packet::hmac_type{}) {
        switch (packet.header.cmd.operator TgBotSocket::Command()) {
            // till open session ack, we don't know the session key
            case Command::CMD_OPEN_SESSION:
            case Command::CMD_OPEN_SESSION_ACK:
                return packet;  // Pass
            default:
                LOG(ERROR) << "Unchecked packet (Not allowed!)";
                return std::nullopt;
        }
    } else if (packet.hmac != computeHMAC(packet)) {
#ifdef ENABLE_HEXDUMP
        DLOG(INFO) << "ComputeHMAC - DUMP expected HMAC result";
        hexdump((const uint8_t*)packet.hmac.data(), packet.hmac.size());
#endif
        LOG(ERROR) << "HMAC mismatch";
        return std::nullopt;
    }

    if (packet.header.data_size > 0 && !decryptPacket(packet)) {
        return std::nullopt;
    }
    return packet;
}

bool SOCKET_EXPORT decryptPacket(TgBotSocket::Packet& packet) {
    auto& header = packet.header;
    auto& data = packet.data;

    if (packet.header.data_size == 0) {
        // Nothing to decrypt - no data
        DLOG(INFO) << "No payload to decrypt";
        return true;
    }

    packet.data =
        decrypt_payload(header.session_token, data, packet.header.init_vector);
    if (!static_cast<bool>(packet.data)) {
        LOG(ERROR) << "Decryption failed";
        return false;
    }
    // Update the header data size
    packet.header.data_size = packet.data.size();
    return true;
}

Packet SOCKET_EXPORT
createPacket(const Command command, const void* data,
             Packet::Header::length_type length, const PayloadType payloadType,
             const Packet::Header::session_token_type& sessionToken) {
    Packet packet{};
    packet.header.cmd = command;
    packet.header.magic = Protocol::MAGIC_VALUE;
    packet.header.data_type = payloadType;
    packet.header.session_token = sessionToken;

    packet.header.nonce =
        ::std::chrono::duration_cast<::std::chrono::milliseconds>(
            ::std::chrono::system_clock::now().time_since_epoch())
            .count() +
        ::std::rand();  // Add some randomness to the nonce

    bool hasToken = sessionToken != Packet::Header::session_token_type{};

    if (data != nullptr && length > 0) {
        packet.data.resize(length);
        packet.data.assignFrom(data, length);

        if (hasToken) {
            packet.data = encrypt_payload(sessionToken, packet.data,
                                          packet.header.init_vector);
            packet.header.data_size = packet.data.size();
        } else {
            LOG(WARNING)
                << "No session token provided, encryption will be skipped";
        }
    }
    if (hasToken) {
        packet.hmac = computeHMAC(packet);
    }
    return packet;
}

std::optional<nlohmann::json> SOCKET_EXPORT
parseAndCheck(const void* buf, TgBotSocket::Packet::Header::length_type length,
              const std::initializer_list<const char*> nodes) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(std::string(static_cast<const char*>(buf), length));
    } catch (const nlohmann::json::parse_error& e) {
        LOG(WARNING) << "Failed to parse json: " << e.what();
        return std::nullopt;
    }
    if (!root.is_object()) {
        LOG(WARNING) << "Expected an object in json";
        return std::nullopt;
    }
    for (const auto& node : nodes) {
        if (!root.contains(node)) {
            LOG(WARNING) << fmt::format("Missing node '{}' in json", node);
            return std::nullopt;
        }
    }
    return root;
}

Packet SOCKET_EXPORT
nodeToPacket(const Command& command, const nlohmann::json& json,
             const Packet::Header::session_token_type& session_token) {
    std::string result = json.dump();
    auto packet = createPacket(command, result.c_str(), result.size(),
                               PayloadType::Json, session_token);
    return packet;
}

}  // namespace TgBotSocket
