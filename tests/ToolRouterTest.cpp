#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "ToolRouter.hpp"

namespace {

using llm::tool_router::Domain;
using llm::tool_router::PendingBuilds;

TEST(ToolRouterTest, RoutesExplicitKernelBuildWithoutClassifier) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "Build the Eureka Kernel for a30 using defaults"),
              Domain::KernelBuild);
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "I want Eureka on a30 again, using the defaults."),
              Domain::KernelBuild);
}

TEST(ToolRouterTest, RoutesExplicitRomBuildWithoutClassifier) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "Prepare LineageOS Android 16 for a30"),
              Domain::RomBuild);
}

TEST(ToolRouterTest, RoutesExactResponseWithoutClassifier) {
    EXPECT_EQ(
        llm::tool_router::deterministicRoute("Say exactly FINAL TEST ASK OK"),
        Domain::Chat);
    EXPECT_EQ(
        llm::tool_router::deterministicRoute("Please say exactly: READY."),
        Domain::Chat);
}

TEST(ToolRouterTest, ExactResponseOverridesPendingBuildFollowUp) {
    EXPECT_EQ(
        llm::tool_router::deterministicRoute("Say exactly FINAL TEST ASK OK",
                                             PendingBuilds{.kernel = true}),
        Domain::Chat);
    EXPECT_EQ(llm::tool_router::deterministicRoute("Please say exactly: READY",
                                                   PendingBuilds{.rom = true}),
              Domain::Chat);
}

TEST(ToolRouterTest, MixedExactResponseRequestsUseClassifier) {
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "Say exactly hello, then send it to Bob on Telegram"));
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "Say exactly build the Eureka Kernel"));
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "Please say exactly yes, then ask me whether to continue"));
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "Send exactly FINAL TEST ASK OK to Bob"));
    EXPECT_FALSE(
        llm::tool_router::deterministicRoute("Look up Alice's Telegram ID"));
}

TEST(ToolRouterTest, LeavesDiscussionForClassifier) {
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "How do I build an Android kernel?"));
    EXPECT_FALSE(
        llm::tool_router::deterministicRoute("What is the Eureka Kernel?"));
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "I need an explanation of the kernel build system"));
}

TEST(ToolRouterTest, LeavesUnrelatedBuildForClassifier) {
    EXPECT_FALSE(
        llm::tool_router::deterministicRoute("Build a small website for me"));
}

TEST(ToolRouterTest, PendingPlanRoutesShortFollowUp) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "a30", PendingBuilds{.kernel = true}),
              Domain::KernelBuild);
    EXPECT_EQ(llm::tool_router::deterministicRoute("userdebug",
                                                   PendingBuilds{.rom = true}),
              Domain::RomBuild);
}

TEST(ToolRouterTest, PendingPlanDoesNotHijackAnotherCapability) {
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "send Bob a message saying hello", PendingBuilds{.kernel = true}));
    EXPECT_FALSE(llm::tool_router::deterministicRoute(
        "Can you explain what this build plan will do?",
        PendingBuilds{.rom = true}));

    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "device a30, branch android-4.14, compiler clang 18, "
                  "defconfig vendor_a30_defconfig, and clean build",
                  PendingBuilds{.kernel = true}),
              Domain::KernelBuild);
}

TEST(ToolRouterTest, ExplicitNewBuildOverridesPendingPlan) {
    EXPECT_EQ(llm::tool_router::deterministicRoute(
                  "Build LineageOS for a30", PendingBuilds{.kernel = true}),
              Domain::RomBuild);
}

TEST(ToolRouterTest, ParsesStrictAndDecoratedClassifierLabels) {
    EXPECT_EQ(llm::tool_router::parseClassifierResult("kernel_build"),
              Domain::KernelBuild);
    EXPECT_EQ(
        llm::tool_router::parseClassifierResult(R"({"label":"chat_registry"})"),
        Domain::ChatRegistry);
    EXPECT_EQ(llm::tool_router::parseClassifierResult(
                  "The correct label is telegram."),
              Domain::Telegram);
    EXPECT_FALSE(llm::tool_router::parseClassifierResult("unknown"));
    EXPECT_FALSE(llm::tool_router::parseClassifierResult("telegram or chat"));
}

TEST(ToolRouterTest, DeterministicRouteSkipsClassifier) {
    bool classifierCalled = false;
    const auto selected = llm::tool_router::selectDomain(
        "Build the Eureka Kernel for a30", {},
        [&](std::string_view, std::string_view) -> std::optional<std::string> {
            classifierCalled = true;
            return "rom_build";
        });

    EXPECT_EQ(selected, Domain::KernelBuild);
    EXPECT_FALSE(classifierCalled);

    EXPECT_EQ(llm::tool_router::selectDomain(
                  "I want Eureka on a30 again, using the defaults.", {},
                  [&](std::string_view,
                      std::string_view) -> std::optional<std::string> {
                      classifierCalled = true;
                      return "rom_build";
                  }),
              Domain::KernelBuild);
    EXPECT_FALSE(classifierCalled);
}

TEST(ToolRouterTest, ExactResponseSkipsClassifierAndExposesNoTools) {
    bool classifierCalled = false;
    const auto selected = llm::tool_router::selectDomain(
        "Say exactly FINAL TEST ASK OK", {},
        [&](std::string_view, std::string_view) -> std::optional<std::string> {
            classifierCalled = true;
            return "telegram";
        });

    EXPECT_EQ(selected, Domain::Chat);
    EXPECT_FALSE(classifierCalled);
    EXPECT_TRUE(llm::tool_router::toolNamesForDomain(selected).empty());
}

TEST(ToolRouterTest, AmbiguousRequestUsesClassifier) {
    std::string_view receivedPrompt;
    std::string_view receivedQuery;
    const auto selected = llm::tool_router::selectDomain(
        "Message the release manager", {},
        [&](std::string_view prompt,
            std::string_view query) -> std::optional<std::string> {
            receivedPrompt = prompt;
            receivedQuery = query;
            return "telegram";
        });

    EXPECT_EQ(selected, Domain::Telegram);
    EXPECT_EQ(receivedPrompt, llm::tool_router::kClassifierPrompt);
    EXPECT_EQ(receivedQuery, "Message the release manager");
}

TEST(ToolRouterTest, ClassifierFailureFallsBackToAllTools) {
    // An unreachable or incoherent classifier must not silently answer a real
    // tool request as plain chat; both failure modes expose every tool.
    EXPECT_EQ(llm::tool_router::selectDomain(
                  "Message the release manager", {},
                  [](std::string_view, std::string_view)
                      -> std::optional<std::string> { return std::nullopt; }),
              Domain::All);
    EXPECT_EQ(llm::tool_router::selectDomain(
                  "Message the release manager", {},
                  [](std::string_view,
                     std::string_view) -> std::optional<std::string> {
                      return "kernel_build or telegram";
                  }),
              Domain::All);
    EXPECT_EQ(llm::tool_router::selectDomain(
                  "Say exactly hello, then send it to Bob on Telegram", {},
                  [](std::string_view, std::string_view)
                      -> std::optional<std::string> { return std::nullopt; }),
              Domain::All);
}

TEST(ToolRouterTest, ExplicitChatLabelIsStillHonoured) {
    EXPECT_EQ(llm::tool_router::selectDomain(
                  "Do something ambiguous", {},
                  [](std::string_view, std::string_view)
                      -> std::optional<std::string> { return "chat"; }),
              Domain::Chat);
    EXPECT_TRUE(llm::tool_router::toolNamesForDomain(Domain::Chat).empty());
}

TEST(ToolRouterTest, ExposesOnlyToolsForSelectedCapability) {
    const auto names = [](Domain domain) {
        const auto selected = llm::tool_router::toolNamesForDomain(domain);
        return std::vector<std::string_view>(selected.begin(), selected.end());
    };

    EXPECT_EQ(names(Domain::Chat), std::vector<std::string_view>{});
    EXPECT_EQ(names(Domain::KernelBuild),
              std::vector<std::string_view>{"kernelbuild"});
    EXPECT_EQ(names(Domain::RomBuild),
              std::vector<std::string_view>{"rombuild"});
    EXPECT_EQ(names(Domain::Build),
              (std::vector<std::string_view>{"kernelbuild", "rombuild"}));
    EXPECT_EQ(names(Domain::Telegram),
              (std::vector<std::string_view>{"send_message", "get_chat_id",
                                             "get_chat_name", "ask"}));
    EXPECT_EQ(names(Domain::ChatRegistry),
              (std::vector<std::string_view>{"get_chat_id", "get_chat_name",
                                             "save_chat_info", "ask"}));
    EXPECT_EQ(names(Domain::Confirmation),
              std::vector<std::string_view>{"ask"});
    EXPECT_EQ(names(Domain::All),
              (std::vector<std::string_view>{
                  "kernelbuild", "rombuild", "send_message", "get_chat_id",
                  "get_chat_name", "save_chat_info", "ask"}));
}

TEST(ToolRouterTest, ActingDomainsCanConfirmBeforeActing) {
    // send_message and save_chat_info have no confirmation step of their own,
    // so "ask" must travel with them; the builder domains stage their own
    // Telegram review and deliberately keep it out.
    const auto has = [](Domain domain, std::string_view tool) {
        const auto names = llm::tool_router::toolNamesForDomain(domain);
        return std::ranges::find(names, tool) != names.end();
    };

    EXPECT_TRUE(has(Domain::Telegram, "ask"));
    EXPECT_TRUE(has(Domain::ChatRegistry, "ask"));
    EXPECT_TRUE(has(Domain::All, "ask"));
    EXPECT_FALSE(has(Domain::KernelBuild, "ask"));
    EXPECT_FALSE(has(Domain::RomBuild, "ask"));
    EXPECT_FALSE(has(Domain::Build, "ask"));
}

}  // namespace
