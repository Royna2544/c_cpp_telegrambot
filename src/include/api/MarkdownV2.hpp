#pragma once

#include <absl/strings/str_replace.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tgbot::markdownv2 {

// Escape arbitrary text so it is safe to send to Telegram with
// ParseMode::MarkdownV2. The backslash is escaped first; absl::StrReplaceAll is
// single-pass, so it does not re-escape the backslashes it inserts for the
// other special characters.
inline std::string escape(std::string_view in) {
    static const std::vector<std::pair<std::string, std::string>> replacements =
        {{"\\", "\\\\"}, {"_", "\\_"}, {"*", "\\*"}, {"[", "\\["},
         {"]", "\\]"},   {"(", "\\("}, {")", "\\)"}, {"~", "\\~"},
         {"`", "\\`"},   {">", "\\>"}, {"#", "\\#"}, {"+", "\\+"},
         {"-", "\\-"},   {"=", "\\="}, {"|", "\\|"}, {"{", "\\{"},
         {"}", "\\}"},   {".", "\\."}, {"!", "\\!"}};
    return absl::StrReplaceAll(in, replacements);
}

}  // namespace tgbot::markdownv2
