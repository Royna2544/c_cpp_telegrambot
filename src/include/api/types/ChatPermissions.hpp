#pragma once

namespace api::types {

/**
 * @brief Describes actions that a non-administrator user is allowed to take in
 * a chat.
 */
struct ChatPermissions {
    /**
     * @brief Optional. True, if the user is allowed to send text messages,
     * contacts, giveaways, giveaway winners, invoices, locations and venues
     */
    std::optional<bool> canSendMessages;

    /**
     * @brief Optional. True, if the user is allowed to send audios
     */
    std::optional<bool> canSendAudios;

    /**
     * @brief Optional. True, if the user is allowed to send documents
     */
    std::optional<bool> canSendDocuments;

    /**
     * @brief Optional. True, if the user is allowed to send photos
     */
    std::optional<bool> canSendPhotos;

    /**
     * @brief Optional. True, if the user is allowed to send videos
     */
    std::optional<bool> canSendVideos;

    /**
     * @brief Optional. True, if the user is allowed to send video notes
     */
    std::optional<bool> canSendVideoNotes;

    /**
     * @brief Optional. True, if the user is allowed to send voice notes
     */
    std::optional<bool> canSendVoiceNotes;

    /**
     * @brief Optional. True, if the user is allowed to send polls
     */
    std::optional<bool> canSendPolls;

    /**
     * @brief Optional. True, if the user is allowed to send animations, games,
     * stickers and use inline bots
     */
    std::optional<bool> canSendOtherMessages;

    /**
     * @brief Optional. True, if the user is allowed to add web page previews to
     * their messages
     */
    std::optional<bool> canAddWebPagePreviews;

    /**
     * @brief Optional. True, if the user is allowed to change the chat title,
     * photo and other settings.
     *
     * Ignored in public supergroups
     */
    std::optional<bool> canChangeInfo;

    /**
     * @brief Optional. True, if the user is allowed to invite new users to the
     * chat
     */
    std::optional<bool> canInviteUsers;

    /**
     * @brief Optional. True, if the user is allowed to pin messages.
     *
     * Ignored in public supergroups
     */
    std::optional<bool> canPinMessages;

    /**
     * @brief Optional. True, if the user is allowed to create forum topics.
     *
     * If omitted defaults to the value of canPinMessages
     */
    std::optional<bool> canManageTopics;
};

}  // namespace api::types