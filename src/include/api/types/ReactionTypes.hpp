#include <api/types/fwd.hpp>

#include <string>
#include <variant>

namespace api::types {

/**
 * @brief The reaction is based on an emoji.
 */
struct ReactionTypeEmoji {
    /**
     * @brief Type of the reaction, must be "emoji".
     */
    std::string type = "emoji";

    /**
     * @brief Reaction emoji.
     *
     * Currently, it can be one of "👍", "👎", "❤", "🔥", "🥰", "👏", "😁",
     * "🤔", "🤯", "😱", "🤬", "😢", "🎉", "🤩", "🤮", "💩", "🙏", "👌", "🕊",
     * "🤡", "🥱", "🥴", "😍", "🐳", "❤‍🔥", "🌚", "🌭", "💯", "🤣", "⚡",
     * "🍌", "🏆", "💔", "🤨", "😐", "🍓", "🍾", "💋", "🖕", "😈", "😴", "😭",
     * "🤓", "👻", "👨‍💻", "👀", "🎃", "🙈", "😇", "😨", "🤝", "✍", "🤗",
     * "🫡", "🎅", "🎄", "☃", "💅", "🤪", "🗿", "🆒", "💘", "🙉", "🦄", "😘",
     * "💊", "🙊", "😎", "👾", "🤷‍♂", "🤷", "🤷‍♀",
     * "😡"
     *
     * See https://core.telegram.org/bots/api#reactiontypeemoji
     */
    std::string emoji;
};


/**
 * @brief The reaction is based on a custom emoji.
 */
struct ReactionTypeCustomEmoji {
    /**
     * @brief Type of the reaction, must be "custom_emoji".
     */
    std::string type = "custom_emoji";

    /**
     * @brief Custom emoji identifier
     */
    std::string customEmojiId;
};

using ReactionType = std::variant<ReactionTypeEmoji, ReactionTypeCustomEmoji>;

}  // namespace api::types