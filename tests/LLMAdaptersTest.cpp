#include <gtest/gtest.h>

#include <algorithm>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "llm/AnthropicApi.hpp"
#include "llm/LMStudioApi.hpp"
#include "llm/OpenAIApi.hpp"

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
        resp.output,
        [](const LMStudioApi::ChatResponse::Output& o) { return o.type == "message"; });
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
