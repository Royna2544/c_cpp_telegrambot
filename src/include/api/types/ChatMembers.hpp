#pragma once

#include <api/types/Chat.hpp>
#include <api/types/ChatInviteLink.hpp>
#include <api/types/User.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace api::types {

using ChatMember =
    std::variant<struct ChatMemberAdministrator, struct ChatMemberBanned,
                 struct ChatMemberLeft, struct ChatMemberMember,
                 struct ChatMemberOwner, struct ChatMemberRestricted>;

/**
 * @brief Represents a [chat
 * member](https://core.telegram.org/bots/api#chatmember) that has some
 * additional privileges.
 */
struct ChatMemberAdministrator {
    static constexpr std::string_view STATUS = "administrator";

    /**
     * @brief The member's status in the chat
     */
    std::string status;

    /**
     * @brief Information about the user
     */
    std::optional<api::types::User> user;

    /**
     * @brief True, if the bot is allowed to edit administrator privileges of
     * that user
     */
    bool canBeEdited{};

    /**
     * @brief True, if the user's presence in the chat is hidden
     */
    bool isAnonymous{};

    /**
     * @brief True, if the administrator can access the chat event log, get
     * boost list, see hidden supergroup and channel members, report spam
     * messages and ignore slow mode.
     *
     * Implied by any other administrator privilege.
     */
    bool canManageChat{};

    /**
     * @brief True, if the administrator can delete messages of other users
     */
    bool canDeleteMessages{};

    /**
     * @brief True, if the administrator can manage video chats
     */
    bool canManageVideoChats{};

    /**
     * @brief True, if the administrator can restrict, ban or unban chat
     * members, or access supergroup statistics
     */
    bool canRestrictMembers{};

    /**
     * @brief True, if the administrator can add new administrators with a
     * subset of their own privileges or demote administrators that they have
     * promoted, directly or indirectly (promoted by administrators that were
     * appointed by the user)
     */
    bool canPromoteMembers{};

    /**
     * @brief True, if the user is allowed to change the chat title, photo and
     * other settings
     */
    bool canChangeInfo{};

    /**
     * @brief True, if the user is allowed to invite new users to the chat
     */
    bool canInviteUsers{};

    /**
     * @brief True, if the administrator can post stories to the chat
     */
    bool canPostStories{};

    /**
     * @brief True, if the administrator can edit stories posted by other users
     */
    bool canEditStories{};

    /**
     * @brief True, if the administrator can delete stories posted by other
     * users
     */
    bool canDeleteStories{};

    /**
     * @brief Optional. True, if the administrator can post messages in the
     * channel, or access channel statistics; for channels only
     */
    std::optional<bool> canPostMessages;

    /**
     * @brief Optional. True, if the administrator can edit messages of other
     * users and can pin messages; for channels only
     */
    std::optional<bool> canEditMessages;

    /**
     * @brief Optional. True, if the user is allowed to pin messages; for groups
     * and supergroups only
     */
    std::optional<bool> canPinMessages;

    /**
     * @brief Optional. True, if the user is allowed to create, rename, close,
     * and reopen forum topics; for supergroups only
     */
    std::optional<bool> canManageTopics;

    /**
     * @brief Optional. Custom title for this user
     */
    std::optional<std::string> customTitle;
};

/**
 * @brief Represents a chat member that was banned in the chat and can't return
 * to the chat or view chat messages.
 */
struct ChatMemberBanned {
    static constexpr std::string_view STATUS = "kicked";

    /**
     * @brief The member's status in the chat
     */
    std::string status;

    /**
     * @brief Information about the user
     */
    std::optional<api::types::User> user;

    /**
     * @brief Date when restrictions will be lifted for this user; Unix time.
     *
     * If 0, then the user is banned forever
     */
    std::uint32_t untilDate{};
};

/**
 * @brief Represents a chat member that isn't currently a member of the chat,
 * but may join it themselves.
 */
struct ChatMemberLeft {
    static constexpr std::string_view STATUS = "left";

    /**
     * @brief The member's status in the chat
     */
    std::string status;

    /**
     * @brief Information about the user
     */
    std::optional<api::types::User> user;
};

/**
 * @brief Represents a chat member that has no additional privileges or
 * restrictions.
 */
struct ChatMemberMember {
    static constexpr std::string_view STATUS = "member";

    /**
     * @brief The member's status in the chat
     */
    std::string status;

    /**
     * @brief Information about the user
     */
    std::optional<api::types::User> user;
};

/**
 * @brief Represents a chat member that owns the chat and has all administrator
 * privileges.
 */
struct ChatMemberOwner {
    static constexpr std::string_view STATUS = "creator";

    /**
     * @brief The member's status in the chat
     */
    std::string status;

    /**
     * @brief Information about the user
     */
    std::optional<api::types::User> user;

    /**
     * @brief Custom title for this user
     */
    std::string customTitle;

    /**
     * @brief True, if the user's presence in the chat is hidden
     */
    bool isAnonymous{};
};

/**
 * @brief Represents a [chat
 * member](https://core.telegram.org/bots/api#chatmember) that is under certain
 * restrictions in the chat.
 *
 * Supergroups only.
 */
struct ChatMemberRestricted {
    static constexpr std::string_view STATUS = "restricted";

    /**
     * @brief The member's status in the chat
     */
    std::string status;

    /**
     * @brief Information about the user
     */
    std::optional<api::types::User> user;

    /**
     * @brief True, if the user is a member of the chat at the moment of the
     * request
     */
    bool isMember{};

    /**
     * @brief True, if the user is allowed to send text messages, contacts,
     * giveaways, giveaway winners, invoices, locations and venues
     */
    bool canSendMessages{};

    /**
     * @brief True, if the user is allowed to send audios
     */
    bool canSendAudios{};

    /**
     * @brief True, if the user is allowed to send documents
     */
    bool canSendDocuments{};

    /**
     * @brief True, if the user is allowed to send photos
     */
    bool canSendPhotos{};

    /**
     * @brief True, if the user is allowed to send videos
     */
    bool canSendVideos{};

    /**
     * @brief True, if the user is allowed to send video notes
     */
    bool canSendVideoNotes{};

    /**
     * @brief True, if the user is allowed to send voice notes
     */
    bool canSendVoiceNotes{};

    /**
     * @brief True, if the user is allowed to send polls
     */
    bool canSendPolls{};

    /**
     * @brief True, if the user is allowed to send animations, games, stickers
     * and use inline bots
     */
    bool canSendOtherMessages{};

    /**
     * @brief True, if the user is allowed to add web page previews to their
     * messages
     */
    bool canAddWebPagePreviews{};

    /**
     * @brief True, if the user is allowed to change the chat title, photo and
     * other settings
     */
    bool canChangeInfo{};

    /**
     * @brief True, if the user is allowed to invite new users to the chat
     */
    bool canInviteUsers{};

    /**
     * @brief True, if the user is allowed to pin messages
     */
    bool canPinMessages{};

    /**
     * @brief True, if the user is allowed to create forum topics
     */
    bool canManageTopics{};

    /**
     * @brief Date when restrictions will be lifted for this user; Unix time.
     *
     * If 0, then the user is restricted forever
     */
    std::uint32_t untilDate{};
};

/**
 * @brief This object represents changes in the status of a chat member.
 *
 * @ingroup types
 */
class ChatMemberUpdated {
   public:
    /**
     * @brief The member's status in the chat
     */
    std::string status;

    /**
     * @brief Information about the user
     */
    std::optional<api::types::User> user;

    /**
     * @brief Chat the user belongs to
     */
    std::optional<api::types::Chat> chat;

    /**
     * @brief Performer of the action, which resulted in the change
     */
    std::optional<api::types::User> from;

    /**
     * @brief Date the change was done in Unix time
     */
    std::uint32_t date;

    /**
     * @brief Previous information about the chat member
     */
    std::optional<api::types::ChatMember> oldChatMember;

    /**
     * @brief New information about the chat member
     */
    std::optional<api::types::ChatMember> newChatMember;

    /**
     * @brief Optional. Chat invite link, which was used by the user to join the
     * chat; for joining by invite link events only.
     */
    std::optional<ChatInviteLink> inviteLink;

    /**
     * @brief Optional. True, if the user joined the chat via a chat folder
     * invite link
     */
    std::optional<bool> viaChatFolderInviteLink;
};
}  // namespace api::types