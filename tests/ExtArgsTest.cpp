#include <gtest/gtest.h>
#include <tgbot/types/Message.h>
#include <ExtArgs.h>

using TgBot::Message;

TEST(ExtArgsTest, ParseExtArgs) {
  std::string extraargs;
  const std::string message = "/start extra_arg1 extra_arg2";
  const Message::Ptr message_ptr = std::make_shared<Message>();
  message_ptr->text = message;

  parseExtArgs(message_ptr, extraargs);

  EXPECT_EQ(extraargs, "extra_arg1 extra_arg2");
}