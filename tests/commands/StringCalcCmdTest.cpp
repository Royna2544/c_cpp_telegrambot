#include "CommandModulesTest.hpp"

using testing::HasSubstr;

class StringCalcCommandTest : public CommandTestBase {
   public:
    StringCalcCommandTest() : CommandTestBase("calc") {}
    ~StringCalcCommandTest() override = default;
};

TEST_F(StringCalcCommandTest, NoExpression) {
    setCommandExtArgs();
    
    willSendReplyMessage(HasSubstr("Usage"));
    execute();
}

TEST_F(StringCalcCommandTest, SimpleExpression) {
    setCommandExtArgs({"2+2"});
    
    willSendReplyMessage(_);
    execute();
}

TEST_F(StringCalcCommandTest, ComplexExpression) {
    setCommandExtArgs({"(10+5)*3"});
    
    willSendReplyMessage(_);
    execute();
}
