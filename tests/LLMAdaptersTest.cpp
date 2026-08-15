#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "llm/AnthropicApi.hpp"

// Match the production include order: AskConfirmTool instantiates the JSON
// serializer for optional<json> before the LM Studio adapter is included.
static_assert(sizeof(nlohmann::adl_serializer<std::optional<nlohmann::json>>) >
              0);

#include "llm/LMStudioApi.hpp"
#include "llm/OpenAIApi.hpp"
#include "llm/TelegramOutput.hpp"
#include "llm/ToolSafety.hpp"

// These tests exercise only the nlohmann::json ADL (de)serialization of each
// adapter's request/response structs — no HTTP is performed.

// ----------------------------- OpenAI --------------------------------------

TEST(OpenAIAdapter, ChatRequestRoundTrip) {
    llm::openai::ChatRequest req;
    req.model = "gpt-test";
    req.messages = {{"system", "sys"}, {"user", "hi"}};
    req.max_tokens = 256;

    const nlohmann::json j = req;
    EXPECT_EQ(j.at("model"), "gpt-test");
    EXPECT_EQ(j.at("max_tokens"), 256);
    ASSERT_EQ(j.at("messages").size(), 2U);
    EXPECT_EQ(j["messages"][0]["role"], "system");
    EXPECT_EQ(j["messages"][1]["content"], "hi");

    const auto back = j.get<llm::openai::ChatRequest>();
    EXPECT_EQ(back.model, "gpt-test");
    ASSERT_EQ(back.messages.size(), 2U);
    EXPECT_EQ(back.messages[1].role, "user");
}

TEST(OpenAIAdapter, ChatResponseExtractsContent) {
    const auto j = nlohmann::json::parse(R"({
        "choices": [
            {"index": 0,
             "message": {"role": "assistant", "content": "hello there"},
             "finish_reason": "stop"}
        ],
        "usage": {"total_tokens": 5}
    })");
    const auto resp = j.get<llm::openai::ChatResponse>();
    ASSERT_FALSE(resp.choices.empty());
    EXPECT_EQ(resp.choices.front().message.content, "hello there");
}

TEST(OpenAIAdapter, ModelListIgnoresExtraFields) {
    const auto j = nlohmann::json::parse(
        R"({"object":"list","data":[{"id":"m1","object":"model"},{"id":"m2"}]})");
    const auto list = j.get<llm::openai::ModelList>();
    ASSERT_EQ(list.data.size(), 2U);
    EXPECT_EQ(list.data[0].id, "m1");
    EXPECT_EQ(list.data[1].id, "m2");
}

// --------------------------- Anthropic -------------------------------------

TEST(AnthropicAdapter, MessagesRequestSerialization) {
    llm::anthropic::MessagesRequest req;
    req.model = "claude-x";
    req.max_tokens = 1000;
    req.system = "be brief";
    req.messages = {{"user", "hi"}};

    const nlohmann::json j = req;
    EXPECT_EQ(j.at("model"), "claude-x");
    EXPECT_EQ(j.at("max_tokens"), 1000);
    EXPECT_EQ(j.at("system"), "be brief");
    ASSERT_EQ(j.at("messages").size(), 1U);
    EXPECT_EQ(j["messages"][0]["role"], "user");
    EXPECT_EQ(j["messages"][0]["content"], "hi");
}

TEST(AnthropicAdapter, DefaultMaxTokensIsSet) {
    // max_tokens is required by the API; the struct must default it.
    const llm::anthropic::MessagesRequest req;
    EXPECT_EQ(req.max_tokens, llm::anthropic::kMaxTokens);
}

TEST(AnthropicAdapter, MessagesResponseExtractsTextBlock) {
    const auto j = nlohmann::json::parse(R"({
        "id": "msg_1", "type": "message", "role": "assistant",
        "content": [{"type": "text", "text": "answer"}],
        "stop_reason": "end_turn"
    })");
    const auto resp = j.get<llm::anthropic::MessagesResponse>();
    ASSERT_FALSE(resp.content.empty());
    EXPECT_EQ(resp.content.front().type, "text");
    EXPECT_EQ(resp.content.front().text, "answer");
}

TEST(AnthropicAdapter, ModelListReadsIdAndDisplayName) {
    const auto j = nlohmann::json::parse(
        R"({"data":[{"id":"claude-a","display_name":"Claude A","type":"model"}]})");
    const auto list = j.get<llm::anthropic::ModelList>();
    ASSERT_EQ(list.data.size(), 1U);
    EXPECT_EQ(list.data[0].id, "claude-a");
    EXPECT_EQ(list.data[0].display_name, "Claude A");
}

// --------------------------- LM Studio -------------------------------------

TEST(LMStudioAdapter, ChatRequestSerialization) {
    LMStudioApi::ChatRequest req;
    req.model = "lm";
    req.input = std::string("hi");
    req.system_prompt = "sys";
    req.previous_response_id = "resp_1";

    const nlohmann::json j = req;
    EXPECT_EQ(j.at("model"), "lm");
    EXPECT_EQ(j.at("input"), "hi");
    EXPECT_EQ(j.at("system_prompt"), "sys");
    EXPECT_EQ(j.at("previous_response_id"), "resp_1");
}

TEST(LMStudioAdapter, ClassifierRequestIsSmallDeterministicAndStateless) {
    const nlohmann::json j = LMStudioApi::makeClassifierRequest(
        "lm", "classify this", "send a message");

    EXPECT_EQ(j.at("model"), "lm");
    EXPECT_EQ(j.at("input"), "send a message");
    EXPECT_EQ(j.at("system_prompt"), "classify this");
    EXPECT_EQ(j.at("temperature"), 0.0f);
    EXPECT_EQ(j.at("max_output_tokens"), 32);
    EXPECT_EQ(j.at("reasoning"), "off");
    EXPECT_EQ(j.at("store"), false);
    EXPECT_FALSE(j.contains("previous_response_id"));
}

TEST(LMStudioAdapter, ChatResponseFindsMessageOutput) {
    const auto j = nlohmann::json::parse(R"({
        "model_instance_id": "x",
        "output": [
            {"type": "reasoning", "content": "think"},
            {"type": "message", "content": "final"}
        ],
        "stats": {"input_tokens": 1, "total_output_tokens": 2,
                  "reasoning_output_tokens": 1, "tokens_per_second": 1.0,
                  "time_to_first_token_seconds": 0.1},
        "response_id": "resp_2"
    })");
    const auto resp = j.get<LMStudioApi::ChatResponse>();
    ASSERT_EQ(resp.output.size(), 2U);
    EXPECT_EQ(resp.response_id, std::optional<std::string>("resp_2"));

    const auto it = std::ranges::find_if(
        resp.output, [](const LMStudioApi::ChatResponse::Output& o) {
            return o.type == "message";
        });
    ASSERT_NE(it, resp.output.end());
    EXPECT_EQ(it->content, "final");
}

TEST(LMStudioAdapter, ModelResponseParsesModel) {
    const auto j = nlohmann::json::parse(R"({
        "models": [{
            "type": "llm", "publisher": "p", "key": "k", "display_name": "D",
            "quantization": {"name": "q4", "bits_per_weight": 4},
            "size_bytes": 100, "params_string": "7B", "loaded_instances": [],
            "max_context_length": 4096, "format": "gguf"
        }]
    })");
    const auto resp = j.get<LMStudioApi::ModelResponse>();
    ASSERT_EQ(resp.models.size(), 1U);
    EXPECT_EQ(resp.models[0].key, "k");
    EXPECT_EQ(resp.models[0].type, LMStudioApi::LLMType::llm);
}

// ------------------------ Tool-call safety --------------------------------

TEST(ToolCallSafety, RejectsOversizedAndDuplicateBatchesAtomically) {
    llm::tool_safety::ToolCallBudget budget;
    const std::vector<std::string> tooMany{"1", "2", "3", "4", "5"};
    EXPECT_TRUE(budget.acceptBatch(tooMany).has_value());
    EXPECT_EQ(budget.acceptedCalls(), 0U);

    const std::vector<std::string> first{"1", "2", "3", "4"};
    EXPECT_FALSE(budget.acceptBatch(first).has_value());
    EXPECT_EQ(budget.acceptedCalls(), 4U);

    const std::vector<std::string> duplicate{"4", "5"};
    EXPECT_TRUE(budget.acceptBatch(duplicate).has_value());
    EXPECT_EQ(budget.acceptedCalls(), 4U);

    const std::vector<std::string> second{"5", "6", "7", "8"};
    EXPECT_FALSE(budget.acceptBatch(second).has_value());
    EXPECT_EQ(budget.acceptedCalls(), 8U);
    const std::vector<std::string> ninth{"9"};
    EXPECT_TRUE(budget.acceptBatch(ninth).has_value());
}

TEST(ToolExecutionSafety, EnforcesExactAllowlist) {
    const std::vector<llm::Tool> exposed{
        {"kernelbuild", "", nlohmann::json::object()}};
    llm::tool_safety::ToolExecutionPolicy policy(exposed);

    int executions = 0;
    llm::ToolExecutor delegate = [&](const std::string&, const nlohmann::json&,
                                     bool&) {
        ++executions;
        return std::string("executed");
    };
    bool isError = false;
    const auto result = policy.execute(
        "send_message", {{"user_id", 1}, {"text", "hello"}}, delegate, isError);

    EXPECT_TRUE(isError);
    EXPECT_EQ(executions, 0);
    EXPECT_NE(result.find("not exposed"), std::string::npos);
}

TEST(ToolExecutionSafety,
     ApprovalIsExactConsumedAndCannotAuthorizeASecondWrite) {
    const std::vector<llm::Tool> exposed{
        {"ask", "", nlohmann::json::object()},
        {"send_message", "", nlohmann::json::object()}};
    llm::tool_safety::ToolExecutionPolicy policy(exposed);
    const nlohmann::json approved{{"user_id", 7}, {"text", "hello"}};
    const nlohmann::json changed{{"user_id", 7}, {"text", "changed"}};

    int executions = 0;
    llm::ToolExecutor delegate = [&](const std::string&, const nlohmann::json&,
                                     bool&) {
        ++executions;
        return std::string("executed");
    };

    bool isError = false;
    policy.execute("send_message", approved, delegate, isError);
    EXPECT_TRUE(isError);
    EXPECT_EQ(executions, 0);

    ASSERT_TRUE(policy.recordApproval("send_message", approved));
    isError = false;
    policy.execute("send_message", changed, delegate, isError);
    EXPECT_TRUE(isError);
    EXPECT_EQ(executions, 0);

    isError = false;
    EXPECT_EQ(policy.execute("send_message", approved, delegate, isError),
              "executed");
    EXPECT_FALSE(isError);
    EXPECT_EQ(executions, 1);

    EXPECT_FALSE(policy.recordApproval("send_message", approved));
    isError = false;
    policy.execute("send_message", approved, delegate, isError);
    EXPECT_TRUE(isError);
    EXPECT_EQ(executions, 1);
}

TEST(TelegramOutputSafety, SplitsOnUtf8BoundariesAndReassemblesExactly) {
    std::string input;
    for (int i = 0; i < 1500; ++i) {
        input += "a한";
    }

    const auto chunks = llm::telegram_output::splitForMarkdown(input);
    ASSERT_GT(chunks.size(), 1U);
    std::string reassembled;
    for (const auto& chunk : chunks) {
        EXPECT_LE(chunk.size(), llm::telegram_output::kRawMarkdownChunkBytes);
        ASSERT_FALSE(chunk.empty());
        EXPECT_FALSE(llm::telegram_output::isUtf8Continuation(
            static_cast<unsigned char>(chunk.front())));
        reassembled += chunk;
    }
    EXPECT_EQ(reassembled, input);
}

TEST(TelegramOutputSafety, CapsMessageFanoutAndMarksTruncation) {
    const std::string huge(100000, '*');
    const auto chunks = llm::telegram_output::splitForMarkdown(huge);

    ASSERT_EQ(chunks.size(), llm::telegram_output::kMaxTelegramChunks);
    EXPECT_NE(chunks.back().find("[response truncated]"), std::string::npos);
    for (const auto& chunk : chunks) {
        EXPECT_LE(chunk.size(), llm::telegram_output::kRawMarkdownChunkBytes);
    }
}

TEST(ToolCallSafety, BoundsToolResults) {
    std::string large(llm::tool_safety::kMaxToolResultBytes + 500, 'x');
    const auto bounded = llm::tool_safety::boundedToolResult(std::move(large));
    EXPECT_NE(bounded.find("[tool result truncated]"), std::string::npos);
    EXPECT_EQ(bounded.size(), llm::tool_safety::kMaxToolResultBytes);
}

TEST(ToolCancellationSafety, RejectsExecutionAfterCancellation) {
    int executions = 0;
    llm::ToolExecutor delegate = [&executions](const std::string&,
                                               const nlohmann::json&, bool&) {
        ++executions;
        return std::string("executed");
    };
    bool isError = false;
    std::string result;

    EXPECT_EQ(llm::tool_safety::executeUnlessCancelled(
                  [] { return true; }, delegate, "kernelbuild",
                  nlohmann::json::object(), isError, result),
              llm::tool_safety::ToolExecutionStatus::Cancelled);
    EXPECT_EQ(executions, 0);
    EXPECT_TRUE(result.empty());
}

TEST(ToolCancellationSafety, RechecksBetweenCallsInOneBatch) {
    bool cancelled = false;
    int executions = 0;
    llm::ToolExecutor delegate = [&](const std::string&, const nlohmann::json&,
                                     bool&) {
        ++executions;
        cancelled = true;
        return std::string("executed");
    };
    const std::function<bool()> cancellationCheck = [&] { return cancelled; };
    bool isError = false;
    std::string result;

    EXPECT_EQ(llm::tool_safety::executeUnlessCancelled(
                  cancellationCheck, delegate, "get_chat_name",
                  nlohmann::json::object(), isError, result),
              llm::tool_safety::ToolExecutionStatus::Executed);
    EXPECT_EQ(result, "executed");

    result.clear();
    EXPECT_EQ(llm::tool_safety::executeUnlessCancelled(
                  cancellationCheck, delegate, "kernelbuild",
                  nlohmann::json::object(), isError, result),
              llm::tool_safety::ToolExecutionStatus::Cancelled);
    EXPECT_EQ(executions, 1);
    EXPECT_TRUE(result.empty());
}

TEST(CurlSafety, InteractiveLimitsStayFiniteAndMemoryBounded) {
    EXPECT_EQ(CurlUtils::kInteractiveConnectTimeoutSeconds, 15);
    EXPECT_EQ(CurlUtils::kInteractiveIdleTimeoutSeconds, 30);
    EXPECT_EQ(CurlUtils::kInteractiveTotalTimeoutSeconds, 180);
    EXPECT_EQ(CurlUtils::kMaxInMemoryResponseBytes, 4U * 1024U * 1024U);
    EXPECT_EQ(CurlUtils::kMaxJsonRequestBytes, 4U * 1024U * 1024U);
}

TEST(CurlSafety, SilentGenerationCanExceedIdleDeadlineBeforeFirstByte) {
    using Watchdog = CurlUtils::detail::InteractiveTransferWatchdog;
    const auto start = Watchdog::Clock::time_point{};
    Watchdog watchdog;

    EXPECT_FALSE(watchdog.observe(0, start));
    EXPECT_FALSE(watchdog.observe(
        0, start + std::chrono::seconds(
                       CurlUtils::kInteractiveIdleTimeoutSeconds + 1)));
    EXPECT_FALSE(watchdog.started());

    EXPECT_FALSE(watchdog.observe(
        128, start + std::chrono::seconds(
                         CurlUtils::kInteractiveIdleTimeoutSeconds + 2)));
    EXPECT_TRUE(watchdog.started());
}

TEST(CurlSafety, StartedResponseIsCancelledAfterTrueIdlePeriod) {
    using Watchdog = CurlUtils::detail::InteractiveTransferWatchdog;
    const auto start = Watchdog::Clock::time_point{};
    Watchdog watchdog;

    EXPECT_FALSE(watchdog.observe(128, start));
    EXPECT_FALSE(watchdog.observe(
        128, start + std::chrono::seconds(
                         CurlUtils::kInteractiveIdleTimeoutSeconds - 1)));
    EXPECT_TRUE(watchdog.observe(
        128, start + std::chrono::seconds(
                         CurlUtils::kInteractiveIdleTimeoutSeconds)));
}

TEST(CurlSafety, ResponseProgressRestartsIdleDeadline) {
    using Watchdog = CurlUtils::detail::InteractiveTransferWatchdog;
    const auto start = Watchdog::Clock::time_point{};
    Watchdog watchdog;

    EXPECT_FALSE(watchdog.observe(1, start));
    EXPECT_FALSE(watchdog.observe(2, start + std::chrono::seconds(29)));
    EXPECT_FALSE(watchdog.observe(2, start + std::chrono::seconds(58)));
    EXPECT_TRUE(watchdog.observe(2, start + std::chrono::seconds(59)));
}
