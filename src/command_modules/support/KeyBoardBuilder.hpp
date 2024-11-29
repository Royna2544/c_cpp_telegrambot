#pragma once

#include <tgbot/types/InlineKeyboardMarkup.h>

#include <concepts>
#include <stdexcept>

template <typename T>
concept isSTLContainer = requires() {
    typename T::iterator;
    typename T::const_iterator;
    { T{}.begin() } -> std::same_as<typename T::iterator>;
    { T{}.end() } -> std::same_as<typename T::iterator>;
    { T{}.size() } -> std::convertible_to<int>;
};

class KeyboardBuilder {
    TgBot::InlineKeyboardMarkup::Ptr keyboard;
    int x = 1;

   public:
    using Button = std::pair<std::string, std::string>;
    using ListOfButton = std::initializer_list<Button>;
    // When we have x, we use that
    explicit KeyboardBuilder(int x) : x(x) {
        if (x <= 0) {
            throw std::logic_error("x must be positive");
        }
        keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    }
    // When we don't have x, we assume it is 1, oneline keyboard
    KeyboardBuilder() {
        keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    }

    // Enable method chaining, templated for STL containers
    template <isSTLContainer ListLike = ListOfButton>
    KeyboardBuilder& addKeyboard(const ListLike& list) {
        // We call resize because we know the number of rows and columns
        // If list size has a remainder, we need to add extra row
        // The thing is... appending could take place after the last element,
        // Or in a new row.
        // We will say new row for now...
        // Create a new keyboard
        if (list.size() == 0) {
            return *this;  // No buttons, nothing to do.
        }

        decltype(keyboard->inlineKeyboard) addingList(
            list.size() / x + (list.size() % x == 0 ? 0 : 1));
        for (size_t i = 0; i < addingList.size(); ++i) {
            addingList[i].resize(std::min<size_t>(x, list.size() - i * x));
        }

        int idx = 0;
        // Add buttons to keyboard
        for (const auto& [text, callbackData] : list) {
            auto button = std::make_shared<TgBot::InlineKeyboardButton>();
            button->text = text;
            button->callbackData = callbackData;
            // We can use cool math to fill keyboard in a nice way
            addingList[idx / x][idx % x] = button;
            ++idx;
        }

        // Insert into main keyboard, if any.
        keyboard->inlineKeyboard.insert(keyboard->inlineKeyboard.end(),
                                        addingList.begin(), addingList.end());
        return *this;
    }
    KeyboardBuilder& addKeyboard(const Button& button) {
        return addKeyboard<std::initializer_list<Button>>({button});
    }
    TgBot::InlineKeyboardMarkup::Ptr get() { return keyboard; }
};