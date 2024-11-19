#pragma once

#include <memory>

#include "api/TgBotApiImpl.hpp"

class TgBotApiImpl::OnMyChatMemberImpl {
   public:
    enum class BotState {
        UNKNOWN = 0,
        IS_MEMBER = 1,
        IS_ADMIN = 1 << 1 | IS_MEMBER,
        IS_RESTRICTED = 1 << 2 | IS_MEMBER,
        IS_BANNED = 1 << 3,
        IS_KICKED = 1 << 4,
    };

   private:
    // A interface for reporting status changes for chat.
    struct Reporter {
        virtual ~Reporter() = default;

        virtual void onStatusChange(const Chat::Ptr& chat,
                                    const BotState oldStatus,
                                    const BotState newStatus) = 0;
    };

    class MessageReport : public Reporter {
        TgBotApiImpl::Ptr _api;
        UserId _ownerId;

       public:
        MessageReport(TgBotApiImpl::Ptr api, const UserId ownerId);
        ~MessageReport() override = default;

        void onStatusChange(const Chat::Ptr& chat, const BotState oldStatus,
                            const BotState newStatus) override;
    };
    class LoggingReport : public Reporter {
       public:
        LoggingReport() = default;
        ~LoggingReport() override = default;

        void onStatusChange(const Chat::Ptr& chat, const BotState oldStatus,
                            const BotState newStatus) override;
    };

    std::vector<std::unique_ptr<Reporter>> reporters;

    static BotState parseState(const TgBot::ChatMember::Ptr& update);
    void onMyChatMemberFunction(const TgBot::ChatMemberUpdated::Ptr& update);

   public:
    explicit OnMyChatMemberImpl(TgBotApiImpl::Ptr api);
};

inline constexpr TgBotApiImpl::OnMyChatMemberImpl::BotState operator|(
    const TgBotApiImpl::OnMyChatMemberImpl::BotState& lhs,
    const TgBotApiImpl::OnMyChatMemberImpl::BotState& rhs) {
    return static_cast<TgBotApiImpl::OnMyChatMemberImpl::BotState>(
        static_cast<int>(lhs) | static_cast<int>(rhs));
}