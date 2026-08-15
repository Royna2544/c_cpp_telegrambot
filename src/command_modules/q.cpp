#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <fmt/format.h>
#include <tgbot/types/MessageEntity.h>
#include <tgbot/types/MessageOriginChannel.h>
#include <tgbot/types/MessageOriginChat.h>
#include <tgbot/types/MessageOriginHiddenUser.h>
#include <tgbot/types/MessageOriginUser.h>

#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <quote/QuoteRenderer.hpp>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#endif

namespace {

constexpr std::uintmax_t kMaximumAssetBytes = 8U * 1024U * 1024U;

[[nodiscard]] bool createOwnerOnlyDirectory(
    const std::filesystem::path& path) noexcept {
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;FA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return false;
    }
    SECURITY_ATTRIBUTES attributes{
        .nLength = sizeof(attributes),
        .lpSecurityDescriptor = descriptor,
        .bInheritHandle = FALSE,
    };
    const bool created = CreateDirectoryW(path.c_str(), &attributes) != FALSE;
    LocalFree(descriptor);
    return created;
#else
    return mkdir(path.c_str(), S_IRWXU) == 0;
#endif
}

class TemporaryDirectory {
   public:
    TemporaryDirectory() {
        try {
            std::error_code ec;
            const auto root = std::filesystem::temp_directory_path(ec);
            if (ec)
                return;
            std::random_device entropy;
            for (int attempt = 0; attempt < 16; ++attempt) {
                const auto suffix =
                    (static_cast<std::uint64_t>(entropy()) << 32U) | entropy();
                auto candidate = root / fmt::format("glider-q-{:016x}", suffix);
                if (!createOwnerOnlyDirectory(candidate)) {
                    continue;
                }
                path_ = std::move(candidate);
                return;
            }
        } catch (const std::exception&) {
            // resolve() turns an empty path into a localized renderer error.
        }
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    ~TemporaryDirectory() {
        if (path_.empty())
            return;
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

   private:
    std::filesystem::path path_;
};

class TelegramQuoteAssetResolver final : public quote::QuoteAssetResolver {
   public:
    explicit TelegramQuoteAssetResolver(TgBotApi::Ptr api) : api_(api) {}

    compat::expected<quote::QuoteAsset, quote::QuoteError> resolve(
        std::string_view assetId) override {
        TemporaryDirectory temporary;
        if (temporary.path().empty()) {
            return compat::unexpected<quote::QuoteError>(quote::QuoteError{
                .code = quote::QuoteErrorCode::AssetUnavailable,
                .message = "cannot create private quote temporary directory",
            });
        }
        const auto file = temporary.path() / "asset";
        if (!api_->downloadFileWithinLimit(file, assetId, kMaximumAssetBytes)) {
            return compat::unexpected<quote::QuoteError>(quote::QuoteError{
                .code = quote::QuoteErrorCode::AssetUnavailable,
                .message = "Telegram asset download failed",
            });
        }
        std::error_code ec;
        const auto size = std::filesystem::file_size(file, ec);
        if (ec || size == 0 || size > kMaximumAssetBytes) {
            return compat::unexpected<quote::QuoteError>(quote::QuoteError{
                .code = quote::QuoteErrorCode::LimitExceeded,
                .message = "Telegram quote asset exceeds the 8 MiB limit",
            });
        }
        std::ifstream input(file, std::ios::binary);
        if (!input) {
            return compat::unexpected<quote::QuoteError>(quote::QuoteError{
                .code = quote::QuoteErrorCode::AssetUnavailable,
                .message = "cannot open downloaded Telegram quote asset",
            });
        }
        quote::QuoteAsset asset;
        asset.bytes.resize(static_cast<std::size_t>(size));
        input.read(reinterpret_cast<char*>(asset.bytes.data()),
                   static_cast<std::streamsize>(asset.bytes.size()));
        if (!input || input.gcount() !=
                          static_cast<std::streamsize>(asset.bytes.size())) {
            return compat::unexpected<quote::QuoteError>(quote::QuoteError{
                .code = quote::QuoteErrorCode::AssetUnavailable,
                .message = "short read from downloaded Telegram quote asset",
            });
        }
        return asset;
    }

   private:
    TgBotApi::Ptr api_;
};

[[nodiscard]] std::optional<quote::QuoteEntityType> mapEntityType(
    TgBot::MessageEntity::Type type) {
    using Telegram = TgBot::MessageEntity::Type;
    using Native = quote::QuoteEntityType;
    switch (type) {
        case Telegram::Bold:
            return Native::Bold;
        case Telegram::Italic:
            return Native::Italic;
        case Telegram::Underline:
            return Native::Underline;
        case Telegram::Strikethrough:
            return Native::Strikethrough;
        case Telegram::Code:
            return Native::Code;
        case Telegram::Pre:
            return Native::Pre;
        case Telegram::TextLink:
            return Native::TextLink;
        case Telegram::CustomEmoji:
            return Native::CustomEmoji;
        default:
            return std::nullopt;
    }
}

[[nodiscard]] std::vector<quote::QuoteEntity> convertEntities(
    const std::optional<std::vector<TgBot::MessageEntity::Ptr>>& entities) {
    std::vector<quote::QuoteEntity> result;
    if (!entities)
        return result;
    result.reserve(entities->size());
    for (const auto& entity : *entities) {
        if (!entity || entity->offset < 0 || entity->length <= 0)
            continue;
        auto type = mapEntityType(entity->type);
        if (!type)
            continue;
        result.push_back(quote::QuoteEntity{
            .type = *type,
            .offset = static_cast<std::size_t>(entity->offset),
            .length = static_cast<std::size_t>(entity->length),
            .url = entity->url.value_or(""),
            // Telegram custom-emoji IDs are retained in the typed request.
            // The default Telegram resolver accepts file IDs, so Pango's open
            // emoji fallback remains visible unless a future resolver maps the
            // custom ID to sticker bytes.
            .customEmojiId = entity->customEmojiId.value_or(""),
        });
    }
    return result;
}

[[nodiscard]] quote::QuoteSender senderFromUser(const User::Ptr& user) {
    quote::QuoteSender sender;
    if (!user) {
        sender.name = "Unknown";
        return sender;
    }
    sender.id = user->id;
    sender.name = user->firstName;
    if (user->lastName && !user->lastName->empty()) {
        if (!sender.name.empty())
            sender.name += ' ';
        sender.name += *user->lastName;
    }
    sender.username = user->username.value_or("");
    return sender;
}

[[nodiscard]] quote::QuoteSender senderFromChat(const Chat::Ptr& chat) {
    quote::QuoteSender sender;
    if (!chat) {
        sender.name = "Unknown";
        return sender;
    }
    sender.id = chat->id;
    sender.name = chat->title.value_or(chat->firstName.value_or("Unknown"));
    sender.username = chat->username.value_or("");
    if (chat->photo && *chat->photo) {
        sender.avatar = quote::QuoteSenderAvatar{
            .assetId = (*chat->photo)->bigFileId,
        };
    }
    return sender;
}

[[nodiscard]] compat::expected<quote::QuoteSender, std::string>
senderForMessage(const Message::Ptr& target) {
    if (!target) {
        return compat::unexpected<std::string>("missing message");
    }
    if (target->forwardOrigin && *target->forwardOrigin) {
        const auto& origin = *target->forwardOrigin;
        if (origin->type == TgBot::MessageOriginUser::TYPE) {
            return senderFromUser(
                std::static_pointer_cast<TgBot::MessageOriginUser>(origin)
                    ->senderUser);
        }
        if (origin->type == TgBot::MessageOriginHiddenUser::TYPE) {
            const auto hidden =
                std::static_pointer_cast<TgBot::MessageOriginHiddenUser>(
                    origin);
            return quote::QuoteSender{.name = hidden->senderUserName};
        }
        if (origin->type == TgBot::MessageOriginChat::TYPE) {
            return senderFromChat(
                std::static_pointer_cast<TgBot::MessageOriginChat>(origin)
                    ->senderChat);
        }
        if (origin->type == TgBot::MessageOriginChannel::TYPE) {
            return senderFromChat(
                std::static_pointer_cast<TgBot::MessageOriginChannel>(origin)
                    ->chat);
        }
        return compat::unexpected<std::string>(origin->type);
    }
    if (target->senderChat && *target->senderChat) {
        return senderFromChat(*target->senderChat);
    }
    if (target->from && *target->from)
        return senderFromUser(*target->from);
    return compat::unexpected<std::string>("sender-less message");
}

void resolveAvatar(TgBotApi::Ptr api, quote::QuoteSender& sender) {
    try {
        if (sender.id > 0) {
            const auto photos = api->getUserProfilePhotos(sender.id);
            if (photos && photos->totalCount > 0 && !photos->photos.empty() &&
                !photos->photos.front().empty() &&
                photos->photos.front().back()) {
                sender.avatar = quote::QuoteSenderAvatar{
                    .assetId = photos->photos.front().back()->fileId,
                };
            }
        } else if (sender.id < 0 && !sender.avatar) {
            const auto chat = api->getChat(sender.id);
            if (chat && chat->photo && *chat->photo) {
                sender.avatar = quote::QuoteSenderAvatar{
                    .assetId = (*chat->photo)->bigFileId,
                };
            }
        }
    } catch (const std::exception& error) {
        // Avatar lookup is optional. Avoid failing the whole quote and do not
        // log quote text, usernames, file IDs, or other sensitive request data.
        DLOG(WARNING) << "Optional quote avatar lookup failed: "
                      << error.what();
    }
}

[[nodiscard]] std::optional<quote::QuoteMedia> extractMedia(
    const Message::Ptr& target, const StringResLoader::PerLocaleMap* res,
    TgBotApi::Ptr api, ChatId chatId) {
    if (target->sticker && *target->sticker) {
        const auto& sticker = *target->sticker;
        std::string assetId = sticker->fileId;
        if ((sticker->isAnimated || sticker->isVideo) && sticker->thumbnail &&
            *sticker->thumbnail) {
            assetId = (*sticker->thumbnail)->fileId;
        }
        return quote::QuoteMedia{
            .type = quote::QuoteMediaType::Sticker,
            .assetId = std::move(assetId),
            .width = static_cast<std::uint32_t>(std::max(0, sticker->width)),
            .height = static_cast<std::uint32_t>(std::max(0, sticker->height)),
            .animated = sticker->isAnimated || sticker->isVideo,
        };
    }
    if (target->photo && !target->photo->empty()) {
        const auto photo = std::find_if(
            target->photo->rbegin(), target->photo->rend(),
            [](const auto& candidate) { return candidate != nullptr; });
        if (photo == target->photo->rend())
            return std::nullopt;
        return quote::QuoteMedia{
            .type = quote::QuoteMediaType::Photo,
            .assetId = (*photo)->fileId,
            .width = static_cast<std::uint32_t>(std::max(0, (*photo)->width)),
            .height = static_cast<std::uint32_t>(std::max(0, (*photo)->height)),
        };
    }
    if (target->video && *target->video) {
        const auto& video = *target->video;
        if (!video->thumbnail || !*video->thumbnail) {
            api->sendMessage(
                chatId,
                res->get(Strings::QUOTE_UNSUPPORTED_VIDEO_NO_THUMBNAIL));
            return std::nullopt;
        }
        const auto& thumbnail = *video->thumbnail;
        return quote::QuoteMedia{
            .type = quote::QuoteMediaType::Video,
            .assetId = thumbnail->fileId,
            .width = static_cast<std::uint32_t>(std::max(0, thumbnail->width)),
            .height =
                static_cast<std::uint32_t>(std::max(0, thumbnail->height)),
        };
    }
    if (target->animation && *target->animation) {
        const auto& animation = *target->animation;
        if (!animation->thumbnail || !*animation->thumbnail) {
            api->sendMessage(
                chatId,
                res->get(Strings::QUOTE_UNSUPPORTED_ANIMATION_NO_THUMBNAIL));
            return std::nullopt;
        }
        const auto& thumbnail = *animation->thumbnail;
        return quote::QuoteMedia{
            .type = quote::QuoteMediaType::Animation,
            .assetId = thumbnail->fileId,
            .width = static_cast<std::uint32_t>(std::max(0, thumbnail->width)),
            .height =
                static_cast<std::uint32_t>(std::max(0, thumbnail->height)),
        };
    }
    return std::nullopt;
}

[[nodiscard]] bool messageHasUnsupportedThumbnailMedia(
    const Message::Ptr& target) {
    return (target->video && *target->video &&
            (!(*target->video)->thumbnail || !*(*target->video)->thumbnail)) ||
           (target->animation && *target->animation &&
            (!(*target->animation)->thumbnail ||
             !*(*target->animation)->thumbnail));
}

}  // namespace

DECLARE_COMMAND_HANDLER(q) {
    if (!message->reply()->exists()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::QUOTE_REPLY_REQUIRED));
        return;
    }
    const auto target = message->reply()->message();
    auto senderResult = senderForMessage(target);
    if (!senderResult.has_value()) {
        api->sendMessage(
            message->get<MessageAttrs::Chat>(),
            fmt::format(
                fmt::runtime(res->get(Strings::QUOTE_UNSUPPORTED_ORIGIN_TYPE)),
                senderResult.error()));
        return;
    }

    quote::QuoteMessage nativeMessage;
    nativeMessage.sender = std::move(senderResult.value());
    nativeMessage.text = target->text.value_or(target->caption.value_or(""));
    nativeMessage.entities = convertEntities(
        target->text ? target->entities : target->captionEntities);
    if (message->message()->quote && *message->message()->quote) {
        nativeMessage.text = (*message->message()->quote)->text;
        nativeMessage.entities =
            convertEntities((*message->message()->quote)->entities);
    }
    if (auto media = extractMedia(target, res, api,
                                  message->get<MessageAttrs::Chat>()->id)) {
        nativeMessage.media.push_back(std::move(*media));
    } else if (messageHasUnsupportedThumbnailMedia(target)) {
        return;
    }
    if (target->voice && *target->voice) {
        nativeMessage.voice = quote::QuoteVoice{
            .durationSeconds = static_cast<std::uint32_t>(
                std::max(0, (*target->voice)->duration)),
        };
    }
    if (target->replyToMessage && *target->replyToMessage) {
        auto replySender = senderForMessage(*target->replyToMessage);
        if (replySender.has_value()) {
            const auto& replyTarget = *target->replyToMessage;
            quote::QuoteReply nativeReply{
                .sender = std::move(replySender.value()),
                .text = replyTarget->text.value_or(
                    replyTarget->caption.value_or("")),
                .entities = convertEntities(replyTarget->text
                                                ? replyTarget->entities
                                                : replyTarget->captionEntities),
            };
            if (auto replyMedia =
                    extractMedia(replyTarget, res, api,
                                 message->get<MessageAttrs::Chat>()->id)) {
                nativeReply.media = std::move(*replyMedia);
            } else if (messageHasUnsupportedThumbnailMedia(replyTarget)) {
                return;
            }
            nativeMessage.reply = std::move(nativeReply);
        }
    }

    bool transparent =
        !nativeMessage.media.empty() &&
        nativeMessage.media.front().type == quote::QuoteMediaType::Sticker;
    std::optional<ChatId> senderOverride;
    const std::string args = message->get<MessageAttrs::ExtraText>();
    if (!args.empty()) {
        for (std::string_view raw : absl::StrSplit(args, ',')) {
            raw = absl::StripAsciiWhitespace(raw);
            const auto separator = raw.find('=');
            if (separator == std::string_view::npos)
                continue;
            const auto key =
                absl::StripAsciiWhitespace(raw.substr(0, separator));
            const auto value =
                absl::StripAsciiWhitespace(raw.substr(separator + 1));
            if (key == "text") {
                nativeMessage.text.assign(value);
                nativeMessage.entities.clear();
                nativeMessage.media.clear();
                transparent = false;
            } else if (key == "media") {
                if (value.empty())
                    continue;
                nativeMessage.text.clear();
                nativeMessage.entities.clear();
                nativeMessage.media = {quote::QuoteMedia{
                    .type = quote::QuoteMediaType::Photo,
                    .assetId = std::string(value),
                }};
                transparent = false;
            } else if (key == "id") {
                ChatId id{};
                const auto [end, error] = std::from_chars(
                    value.data(), value.data() + value.size(), id);
                if (error != std::errc{} ||
                    end != value.data() + value.size()) {
                    api->sendMessage(
                        message->get<MessageAttrs::Chat>(),
                        fmt::format(
                            fmt::runtime(res->get(Strings::QUOTE_INVALID_ID)),
                            "invalid numeric ID"));
                    return;
                }
                senderOverride = id;
            }
        }
    }

    quote::QuoteRenderRequest request;
    request.type = quote::QuoteOutputType::Quote;
    request.format = quote::QuoteOutputFormat::WebP;
    request.background = transparent ? "transparent" : "//#292232";
    request.width = 512;
    // Match quote-api's default high-resolution layout pass.  The renderer
    // shrink-wraps that 2x card and downsamples the completed sticker, keeping
    // short quotes and sender labels readable in Telegram.
    request.scale = 2.0;
    request.telegramSticker = true;
    request.maximumSourceBytes = 24U * 1024U * 1024U;
    request.maximumEncodedBytes = 512U * 1024U;
    request.deadline = std::chrono::seconds(60);
    request.messages.push_back(std::move(nativeMessage));

    const ChatId chatId = message->get<MessageAttrs::Chat>()->id;
    const auto fontDirectory =
        provider->cmdline->getPath(FS::PathType::RESOURCES) / "quote" / "fonts";
    const std::string failureText(res->get(Strings::QUOTE_GENERATE_FAILED));
    const std::string invalidIdText(res->get(Strings::QUOTE_INVALID_ID));
    const auto job = api->submitCommandWork(
        "q", TgBotApi::WorkClass::Media,
        [api, chatId, request = std::move(request), senderOverride, failureText,
         invalidIdText, fontDirectory](std::stop_token stop) mutable {
            if (stop.stop_requested())
                return;
            if (senderOverride) {
                try {
                    const auto chat = api->getChat(*senderOverride);
                    if (!chat)
                        throw std::runtime_error("chat not found");
                    request.messages.front().sender = senderFromChat(chat);
                    if (chat->type == TgBot::Chat::Type::Private) {
                        auto user = std::make_shared<User>();
                        user->id = *senderOverride;
                        user->firstName = chat->firstName.value_or("");
                        user->lastName = chat->lastName;
                        user->username = chat->username;
                        request.messages.front().sender = senderFromUser(user);
                    }
                } catch (const std::exception& error) {
                    const auto text =
                        fmt::format(fmt::runtime(invalidIdText), error.what());
                    if (!api->submitCommandWork(
                            "q", TgBotApi::WorkClass::Outbound,
                            [api, chatId, text](std::stop_token sendStop) {
                                if (!sendStop.stop_requested())
                                    api->sendMessage(chatId, text);
                            })) {
                        LOG(WARNING)
                            << "Quote invalid-ID response queue is full";
                    }
                    return;
                }
            }
            if (stop.stop_requested())
                return;
            resolveAvatar(api, request.messages.front().sender);
            if (stop.stop_requested())
                return;
            TelegramQuoteAssetResolver resolver(api);
            quote::QuoteRenderer renderer(fontDirectory, &resolver);
            auto rendered = renderer.render(request);
            if (stop.stop_requested())
                return;

            if (!rendered.has_value()) {
                LOG(ERROR) << "Native quote rendering failed (code "
                           << static_cast<int>(rendered.error().code)
                           << "): " << rendered.error().message;
                if (!api->submitCommandWork(
                        "q", TgBotApi::WorkClass::Outbound,
                        [api, chatId, failureText](std::stop_token sendStop) {
                            if (!sendStop.stop_requested())
                                api->sendMessage(chatId, failureText);
                        })) {
                    LOG(WARNING) << "Quote failure response queue is full";
                }
                return;
            }

            auto file = std::make_shared<InputFile>();
            file->fileName = rendered.value().fileName;
            file->mimeType = rendered.value().mimeType;
            file->data.assign(
                reinterpret_cast<const char*>(rendered.value().bytes.data()),
                rendered.value().bytes.size());
            if (!api->submitCommandWork("q", TgBotApi::WorkClass::Outbound,
                                        [api, chatId, file = std::move(file)](
                                            std::stop_token sendStop) mutable {
                                            if (!sendStop.stop_requested())
                                                api->sendSticker(
                                                    chatId, std::move(file));
                                        })) {
                LOG(WARNING) << "Quote outbound queue is full";
            }
        },
        {.deadline = std::chrono::seconds(60)});
    if (!job) {
        api->sendMessage(chatId, failureText);
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::HideDescription | DynModule::Flags::Enforced,
    .name = "q",
    .description = "Quote a message",
    .function = COMMAND_HANDLER_NAME(q),
};
