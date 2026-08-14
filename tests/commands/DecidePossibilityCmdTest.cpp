#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>

#include "CommandModulesTest.hpp"
#include "gmock/gmock.h"

namespace {

using testing::HasSubstr;
using testing::InSequence;

void useDecisionStrings(MockLocaleStrings& strings) {
    ON_CALL(strings, get(Strings::DECIDE_DECIDING_OBJECT))
        .WillByDefault(testing::Return("Deciding '{}'..."));
    ON_CALL(strings, get(Strings::DECIDE_TRY_PREFIX))
        .WillByDefault(testing::Return("Try {}: "));
    ON_CALL(strings, get(Strings::YES)).WillByDefault(testing::Return("Yes"));
    ON_CALL(strings, get(Strings::NO)).WillByDefault(testing::Return("No"));
    ON_CALL(strings, get(Strings::SO_YES))
        .WillByDefault(testing::Return("So, yes"));
    ON_CALL(strings, get(Strings::SO_IDK))
        .WillByDefault(testing::Return("So, idk"));
    ON_CALL(strings, get(Strings::SO_NO))
        .WillByDefault(testing::Return("So, no"));
}

void usePossibilityStrings(MockLocaleStrings& strings) {
    ON_CALL(strings, get(Strings::TOTAL_ITEMS_PREFIX))
        .WillByDefault(testing::Return("Total"));
    ON_CALL(strings, get(Strings::TOTAL_ITEMS_SUFFIX))
        .WillByDefault(testing::Return("items"));
    ON_CALL(strings, get(Strings::INVALID_ARGS_PASSED))
        .WillByDefault(testing::Return("Invalid arguments"));
    ON_CALL(strings, get(Strings::GIVE_MORE_THAN_ONE))
        .WillByDefault(testing::Return("Give more than one choice"));
}

std::size_t countOccurrences(const std::string_view text,
                             const std::string_view needle) {
    std::size_t count = 0;
    for (std::size_t pos = 0;
         (pos = text.find(needle, pos)) != std::string_view::npos;
         pos += needle.size()) {
        ++count;
    }
    return count;
}

}  // namespace

struct DecideCommandTest : CommandTestBase {
    DecideCommandTest() : CommandTestBase("decide") {}
};

TEST_F(DecideCommandTest, UsesFairCoinAndCompletesWithinOneSecond) {
    using namespace std::chrono_literals;
    useDecisionStrings(strings);
    setCommandExtArgs({"ship it"});

    EXPECT_CALL(*random, generate(0, 1))
        .Times(10)
        .WillRepeatedly(testing::Return(1));
    const auto sent = willSendReplyMessage(HasSubstr("Deciding 'ship it'"));

    std::string progressText;
    std::string finalText;
    EXPECT_CALL(*botApi,
                editMessage_impl(sent.message, testing::_, testing::IsNull(),
                                 TgBotApi::ParseMode::None))
        .Times(2)
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&progressText),
                                 testing::Return(sent.message)))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&finalText),
                                 testing::Return(sent.message)));
    EXPECT_CALL(*botApi, setMessageReaction_impl(
                             TEST_CHAT_ID, defaultProvidedMessage->messageId,
                             testing::_, true))
        .WillOnce(testing::Return(true));
    EXPECT_CALL(*botApi,
                submitCommandWork("decide", TgBotApi::WorkClass::Outbound,
                                  testing::_, testing::_))
        .Times(3);

    const auto started = std::chrono::steady_clock::now();
    execute();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_LT(elapsed, 1s);
    EXPECT_EQ(countOccurrences(progressText, "Try "), 5U);
    EXPECT_EQ(countOccurrences(finalText, "Try "), 10U);
    EXPECT_THAT(finalText, HasSubstr("So, yes"));
}

struct PossibilityCommandTest : CommandTestBase {
    PossibilityCommandTest() : CommandTestBase("possibility") {}

    void executeNewlineInput(const std::string& input) {
        defaultProvidedMessage->text = "/possibility\n" + input;
        defaultProvidedMessage->entities->front()->length =
            static_cast<std::int32_t>(std::string_view("/possibility").size());
        MessageExt ext(defaultProvidedMessage, SplitMessageText::ByNewline);
        module->info.function(botApi, &ext, &strings,
                              provideInject.get<Providers*>());
    }
};

TEST_F(PossibilityCommandTest, StableDedupeAndPercentagesTotalOneHundred) {
    usePossibilityStrings(strings);
    EXPECT_CALL(*random, shuffle(testing::_));
    {
        InSequence sequence;
        EXPECT_CALL(*random, generate(0, 100)).WillOnce(testing::Return(60));
        EXPECT_CALL(*random, generate(0, 40)).WillOnce(testing::Return(30));
    }

    std::string output;
    EXPECT_CALL(
        *botApi,
        sendMessage_impl(TEST_CHAT_ID, testing::_, createMessageReplyMatcher(),
                         testing::IsNull(), TgBotApi::ParseMode::None))
        .WillOnce(testing::DoAll(testing::SaveArg<1>(&output),
                                 testing::Return(createDefaultMessage())));

    executeNewlineInput("alpha\nbeta\nalpha\ngamma");

    EXPECT_THAT(output, HasSubstr("Total 3 items"));
    EXPECT_EQ(countOccurrences(output, "alpha : "), 1U);
    EXPECT_THAT(output, HasSubstr("alpha : 60%"));
    EXPECT_THAT(output, HasSubstr("beta : 30%"));
    EXPECT_THAT(output, HasSubstr("gamma : 10%"));
    EXPECT_THAT(output, HasSubstr("Total: 100%"));

    std::size_t allocationTotal = 0;
    std::istringstream lines(output);
    for (std::string line; std::getline(lines, line);) {
        const auto separator = line.find(" : ");
        const auto percent = line.rfind('%');
        if (separator == std::string::npos || percent == std::string::npos) {
            continue;
        }
        allocationTotal +=
            std::stoul(line.substr(separator + 3, percent - separator - 3));
    }
    EXPECT_EQ(allocationTotal, 100U);
}

TEST_F(PossibilityCommandTest, RejectsMoreThanOneHundredUniqueChoices) {
    usePossibilityStrings(strings);
    EXPECT_CALL(*random, shuffle(testing::_)).Times(0);
    EXPECT_CALL(*random, generate(testing::_, testing::_)).Times(0);

    std::stringstream input;
    for (int i = 0; i < 101; ++i) {
        if (i != 0) {
            input << '\n';
        }
        input << "choice-" << i;
    }
    willSendReplyMessage(HasSubstr("at most 100 unique choices"));

    executeNewlineInput(input.str());
}

TEST_F(PossibilityCommandTest, RejectsOversizedChoice) {
    usePossibilityStrings(strings);
    EXPECT_CALL(*random, shuffle(testing::_)).Times(0);
    EXPECT_CALL(*random, generate(testing::_, testing::_)).Times(0);
    willSendReplyMessage(HasSubstr("1-64 bytes"));

    executeNewlineInput(std::string(65, 'a') + "\nsecond");
}

TEST_F(PossibilityCommandTest, RejectsRenderedOutputOverTelegramLimit) {
    usePossibilityStrings(strings);
    EXPECT_CALL(*random, shuffle(testing::_));
    EXPECT_CALL(*random, generate(testing::_, testing::_))
        .Times(99)
        .WillRepeatedly(testing::Return(0));

    std::stringstream input;
    for (int i = 0; i < 100; ++i) {
        if (i != 0) {
            input << '\n';
        }
        input << std::string(35, 'x') << '-' << i;
    }
    willSendReplyMessage(HasSubstr("combined output exceeds 4096 bytes"));

    executeNewlineInput(input.str());
}
