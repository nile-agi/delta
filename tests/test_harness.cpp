/**
 * Harness tests.
 *
 * These run the real agent loop against a scripted stand-in for llama-server, so the behaviour
 * that matters -- tool results reaching the model, the approval gate, context compaction -- is
 * exercised without needing a tool-capable model on the machine.
 *
 * Built as its own executable rather than joining the Catch2 suite, which needs a dependency
 * that is not always present. Run it with: ./build/delta-harness-tests
 */

#include "agent/agent_database.h"
#include "agent/context_manager.h"
#include "agent/harness.h"
#include "agent/memory_store.h"
#include "agent/tool_files.h"
#include "agent/tool_registry.h"
#include "agent/tool_shell.h"
#include "agent/tool_web.h"
#include "vendor/llama.cpp/vendor/cpp-httplib/httplib.h"
#include "vendor/json.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#if !defined(_WIN32)
#include <unistd.h>
#endif

using json = nlohmann::json;
using namespace delta::agent;

// ---------------------------------------------------------------- assertions

static int g_failures = 0;
static int g_checks = 0;
static std::string g_current_test;

static void check(bool condition, const std::string& what) {
    g_checks++;
    if (condition) {
        std::cout << "    ok   " << what << "\n";
    } else {
        g_failures++;
        std::cout << "  FAIL   " << what << "  [" << g_current_test << "]\n";
    }
}

template <typename A, typename B> static void check_eq(const A& actual, const B& expected, const std::string& what) {
    g_checks++;
    if (actual == expected) {
        std::cout << "    ok   " << what << "\n";
    } else {
        g_failures++;
        std::cout << "  FAIL   " << what << "  [" << g_current_test << "]\n";
        std::cout << "           expected: " << expected << "\n";
        std::cout << "           actual:   " << actual << "\n";
    }
}

static void test(const std::string& name) {
    g_current_test = name;
    std::cout << "\n- " << name << "\n";
}

// ------------------------------------------------------- scripted llm server

/**
 * Stands in for llama-server. Each entry in `script` is the assistant message to return for the
 * corresponding request, so a test can drive the loop through an exact sequence of turns. Every
 * request body is recorded, which is how the tests assert what the harness actually sent.
 */
class ScriptedServer {
  public:
    explicit ScriptedServer(std::vector<json> script) : script_(std::move(script)) {
        server_.Get("/props", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(json{{"n_ctx", 4096}}.dump(), "application/json");
        });

        server_.Post("/tokenize", [](const httplib::Request& req, httplib::Response& res) {
            // A token per 4 characters is close enough for budgeting tests.
            size_t n = 0;
            try {
                n = json::parse(req.body).value("content", std::string("")).size() / 4 + 1;
            } catch (...) {
            }
            res.set_content(json{{"tokens", std::vector<int>(n, 1)}}.dump(), "application/json");
        });

        server_.Post("/v1/chat/completions", [this](const httplib::Request& req, httplib::Response& res) {
            json body;
            try {
                body = json::parse(req.body);
            } catch (...) {
                body = json::object();
            }
            size_t index;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                requests_.push_back(body);
                index = requests_.size() - 1;
            }

            const json message =
                index < script_.size() ? script_[index] : json{{"role", "assistant"}, {"content", "done"}};
            res.set_content(sse_for(message), "text/event-stream");
        });
    }

    void start() {
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }

    void stop() {
        server_.stop();
        if (thread_.joinable())
            thread_.join();
    }

    std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

    std::vector<json> requests() {
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_;
    }

  private:
    // Renders an assistant message as the SSE stream llama-server would produce, splitting
    // tool-call arguments across two frames so the client's reassembly is exercised too.
    static std::string sse_for(const json& message) {
        std::string out;
        auto frame = [&out](const json& delta, const char* finish) {
            json choice{{"index", 0}, {"delta", delta}};
            choice["finish_reason"] = finish ? json(finish) : json(nullptr);
            out += "data: " + json{{"object", "chat.completion.chunk"}, {"choices", json::array({choice})}}.dump() +
                   "\n\n";
        };

        const std::string content = message.value("content", "");
        if (!content.empty()) {
            const size_t mid = content.size() / 2;
            frame({{"content", content.substr(0, mid)}}, nullptr);
            frame({{"content", content.substr(mid)}}, nullptr);
        }

        if (message.contains("tool_calls")) {
            int index = 0;
            for (const auto& call : message["tool_calls"]) {
                const std::string args = call["function"].value("arguments", "{}");
                const size_t mid = args.size() / 2;
                frame({{"tool_calls", json::array({{{"index", index},
                                                    {"id", call.value("id", "call_" + std::to_string(index))},
                                                    {"function",
                                                     {{"name", call["function"].value("name", "")},
                                                      {"arguments", args.substr(0, mid)}}}}})}},
                      nullptr);
                frame({{"tool_calls",
                        json::array({{{"index", index}, {"function", {{"arguments", args.substr(mid)}}}}})}},
                      nullptr);
                index++;
            }
            frame(json::object(), message.value("finish_reason", "tool_calls").c_str());
        } else {
            frame(json::object(), "stop");
        }

        out += "data: [DONE]\n\n";
        return out;
    }

    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;
    std::vector<json> script_;
    std::vector<json> requests_;
    std::mutex mutex_;
};

// ------------------------------------------------------------------- helpers

static json tool_call(const std::string& name, const json& arguments, const std::string& id) {
    return {{"id", id}, {"type", "function"}, {"function", {{"name", name}, {"arguments", arguments.dump()}}}};
}

static json assistant_calling(const std::string& name, const json& arguments, const std::string& id = "call_0") {
    return {{"role", "assistant"}, {"content", ""}, {"tool_calls", json::array({tool_call(name, arguments, id)})}};
}

static json user(const std::string& text) {
    return {{"role", "user"}, {"content", text}};
}

/** Collects every event the harness emits, so tests can assert on the stream the UI would see. */
struct EventLog {
    std::vector<std::pair<EventType, json>> events;

    EventSink sink() {
        return [this](const HarnessEvent& event) {
            events.push_back({event.type, event.data});
            return true;
        };
    }

    int count(EventType type) const {
        int n = 0;
        for (const auto& [t, data] : events) {
            (void)data;
            if (t == type)
                n++;
        }
        return n;
    }

    json first(EventType type) const {
        for (const auto& [t, data] : events) {
            if (t == type)
                return data;
        }
        return nullptr;
    }

    std::string text() const {
        std::string out;
        for (const auto& [t, data] : events) {
            if (t == EventType::Content)
                out += data.value("text", "");
        }
        return out;
    }
};

// Tracks how a test tool was called, so a test can assert the harness really executed it.
struct ToolSpy {
    std::atomic<int> calls{0};
    json last_arguments;
    std::mutex mutex;
};

static ToolSpy g_read_spy;
static ToolSpy g_write_spy;
static ToolSpy g_destructive_spy;

static void register_test_tools() {
    auto& registry = ToolRegistry::instance();
    const json no_params = {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}};

    registry.register_tool({"test_read", "A safe read", no_params, ToolRisk::Safe, "testing"},
                           [](const json& args) -> ToolResult {
                               g_read_spy.calls++;
                               std::lock_guard<std::mutex> lock(g_read_spy.mutex);
                               g_read_spy.last_arguments = args;
                               return {true, json{{"count", 2}, {"items", {"alpha", "beta"}}}.dump(), ""};
                           });

    registry.register_tool({"test_write", "A write", no_params, ToolRisk::Caution, "testing"},
                           [](const json& args) -> ToolResult {
                               g_write_spy.calls++;
                               std::lock_guard<std::mutex> lock(g_write_spy.mutex);
                               g_write_spy.last_arguments = args;
                               return {true, json{{"created", true}, {"id", "evt-42"}}.dump(), ""};
                           });

    registry.register_tool({"test_destroy", "A destructive action", no_params, ToolRisk::Destructive, "testing"},
                           [](const json& args) -> ToolResult {
                               g_destructive_spy.calls++;
                               std::lock_guard<std::mutex> lock(g_destructive_spy.mutex);
                               g_destructive_spy.last_arguments = args;
                               return {true, json{{"deleted", true}}.dump(), ""};
                           });

    registry.register_tool({"test_fail", "Always fails", no_params, ToolRisk::Safe, "testing"},
                           [](const json&) -> ToolResult { return {false, "", "disk is on fire"}; });
}

static RunOptions test_options() {
    RunOptions options;
    options.enabled_categories = {"testing"};
    options.max_iterations = 6;
    options.max_tokens = 256;
    options.approval_timeout_seconds = 3;
    return options;
}

/** Finds the tool-role messages in a recorded request body. */
static std::vector<json> tool_messages(const json& request) {
    std::vector<json> out;
    if (!request.contains("messages"))
        return out;
    for (const auto& message : request["messages"]) {
        if (message.value("role", "") == "tool")
            out.push_back(message);
    }
    return out;
}

// --------------------------------------------------------------------- tests

static void test_multi_step_loop() {
    test("a tool call is executed and its result is fed back for another turn");

    ScriptedServer server({assistant_calling("test_read", {{"query", "everything"}}),
                           {{"role", "assistant"}, {"content", "You have alpha and beta."}}});
    server.start();

    const int before = g_read_spy.calls;
    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("what do I have?")}), log.sink());
    server.stop();

    check(result.success, "the run succeeded");
    check_eq(result.stop_reason, std::string("stop"), "it stopped because the model stopped");
    check_eq(result.tool_calls, 1, "one tool ran");
    check_eq(g_read_spy.calls - before, 1, "the handler was actually invoked");
    check_eq(result.content, std::string("You have alpha and beta."), "the model's own words are the final answer");
    check_eq(log.count(EventType::ToolStart), 1, "a tool_start event was emitted");
    check_eq(log.count(EventType::ToolResult), 1, "a tool_result event was emitted");

    auto requests = server.requests();
    check_eq(requests.size(), size_t(2), "the model was called twice");
    if (requests.size() >= 2) {
        auto tools = tool_messages(requests[1]);
        check_eq(tools.size(), size_t(1), "the second call carried the tool result");
        if (!tools.empty()) {
            check(tools[0].value("content", "").find("alpha") != std::string::npos,
                  "the tool result content reached the model verbatim");
        }
    }
}

static void test_write_result_reaches_model() {
    test("a write tool's result goes back to the model instead of a templated reply");

    ScriptedServer server(
        {assistant_calling("test_write", {{"title", "Standup"}}), {{"role", "assistant"}, {"content", "Booked it."}}});
    server.start();

    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("book standup")}), log.sink());
    server.stop();

    // The loop this replaced returned a hand-written sentence here and never called the model
    // again, so the second request is the whole point of this test.
    auto requests = server.requests();
    check_eq(requests.size(), size_t(2), "the model was called again after the write");
    if (requests.size() >= 2) {
        auto tools = tool_messages(requests[1]);
        check_eq(tools.size(), size_t(1), "the write result was in the transcript");
        if (!tools.empty())
            check(tools[0].value("content", "").find("evt-42") != std::string::npos,
                  "the write result content was passed through unchanged");
    }
    check_eq(result.content, std::string("Booked it."), "the reply is the model's, not a template");
}

static void test_tool_failure_is_reported_to_model() {
    test("a failing tool reports the error back rather than aborting the run");

    ScriptedServer server(
        {assistant_calling("test_fail", json::object()), {{"role", "assistant"}, {"content", "That did not work."}}});
    server.start();

    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("do the thing")}), log.sink());
    server.stop();

    check(result.success, "the run still succeeded");
    auto requests = server.requests();
    check_eq(requests.size(), size_t(2), "the model got a chance to react to the failure");
    if (requests.size() >= 2) {
        auto tools = tool_messages(requests[1]);
        if (!tools.empty())
            check(tools[0].value("content", "").find("disk is on fire") != std::string::npos,
                  "the error text was given to the model");
    }
}

static void test_chained_tools() {
    test("the model can chain several tools in one turn");

    ScriptedServer server({assistant_calling("test_read", json::object(), "c1"),
                           assistant_calling("test_write", {{"title", "follow up"}}, "c2"),
                           {{"role", "assistant"}, {"content", "Looked it up and booked it."}}});
    server.start();

    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("look it up then book a follow up")}), log.sink());
    server.stop();

    check_eq(result.tool_calls, 2, "both tools ran");
    check_eq(result.iterations, 3, "it took three model calls");
    check_eq(result.stop_reason, std::string("stop"), "it finished on its own");
    check_eq(server.requests().size(), size_t(3), "each step went back to the model");
}

static void test_destructive_tool_requires_approval() {
    test("a destructive tool waits for approval and runs once allowed");

    ScriptedServer server(
        {assistant_calling("test_destroy", {{"id", "evt-42"}}), {{"role", "assistant"}, {"content", "Deleted."}}});
    server.start();

    const int before = g_destructive_spy.calls;
    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    // Answer the approval from another thread, the way the HTTP endpoint does.
    std::atomic<bool> answered{false};
    std::thread approver([&answered] {
        for (int i = 0; i < 200 && !answered; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    EventLog log;
    std::string approval_id;
    auto sink = [&](const HarnessEvent& event) {
        log.events.push_back({event.type, event.data});
        if (event.type == EventType::ApprovalRequired) {
            approval_id = event.data.value("id", "");
            // Resolve on a detached thread: the harness is blocked inside this callback's caller.
            std::thread([id = approval_id] {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ApprovalBroker::instance().resolve(id, "allow");
            }).detach();
        }
        return true;
    };

    auto result = harness.run(json::array({user("delete event 42")}), sink);
    answered = true;
    approver.join();
    server.stop();

    check(!approval_id.empty(), "an approval was requested");
    check_eq(log.count(EventType::ApprovalRequired), 1, "exactly one approval request");
    check_eq(g_destructive_spy.calls - before, 1, "the tool ran after approval");
    check_eq(result.tool_calls, 1, "the call was counted");

    json resolved = log.first(EventType::ApprovalResolved);
    check(resolved.is_object() && resolved.value("decision", "") == "allow", "the decision was reported as allow");
}

static void test_approval_endpoint_resumes_a_parked_run() {
    test("the approval endpoint's own logic unblocks a parked run and reports 200");

    // Drives answer_approval() -- the exact function POST /v1/agent/approve calls -- against a
    // real run parked on the broker, so the endpoint's success path is covered end to end and
    // not just the broker underneath it.
    ScriptedServer server(
        {assistant_calling("test_destroy", {{"id", "evt-77"}}), {{"role", "assistant"}, {"content", "Removed it."}}});
    server.start();

    const int before = g_destructive_spy.calls;
    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    std::atomic<int> endpoint_status{0};
    json endpoint_body;
    std::mutex body_mutex;

    auto sink = [&](const HarnessEvent& event) {
        if (event.type == EventType::ApprovalRequired) {
            std::thread([&, id = event.data.value("id", "")] {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                const auto result = answer_approval({{"id", id}, {"decision", "allow"}});
                {
                    std::lock_guard<std::mutex> lock(body_mutex);
                    endpoint_body = result.body;
                }
                endpoint_status = result.status;
            }).detach();
        }
        return true;
    };

    auto result = harness.run(json::array({user("delete event 77")}), sink);
    server.stop();

    check_eq(endpoint_status.load(), 200, "the endpoint reported success");
    {
        std::lock_guard<std::mutex> lock(body_mutex);
        check(endpoint_body.value("ok", false), "the response body says ok");
        check_eq(endpoint_body.value("decision", ""), std::string("allow"), "it echoed the decision back");
    }
    check_eq(g_destructive_spy.calls - before, 1, "the parked tool ran once the answer arrived");
    check(result.success, "the run completed");
}

static void test_approval_endpoint_rejects_bad_input() {
    test("the approval endpoint rejects malformed and unknown requests");

    check_eq(answer_approval({{"id", "apr_x"}, {"decision", "maybe"}}).status, 400, "an invalid decision is a 400");
    check_eq(answer_approval({{"decision", "allow"}}).status, 400, "a missing id is a 400");
    check_eq(answer_approval(json("not an object")).status, 400, "a non-object body is a 400");
    check_eq(answer_approval({{"id", "apr_missing"}, {"decision", "allow"}}).status, 404, "an unknown id is a 404");

    // Answering the same request twice must not succeed twice.
    const std::string id = ApprovalBroker::instance().open("test_destroy", json::object());
    check_eq(answer_approval({{"id", id}, {"decision", "deny"}}).status, 200, "the first answer is accepted");
    check_eq(answer_approval({{"id", id}, {"decision", "allow"}}).status, 404,
             "a second answer for the same request is a 404");
    ApprovalBroker::instance().cancel(id);
}

static void test_denied_approval_tells_the_model() {
    test("a denied approval feeds a refusal back instead of running the tool");

    ScriptedServer server({assistant_calling("test_destroy", {{"id", "evt-99"}}),
                           {{"role", "assistant"}, {"content", "Understood, leaving it alone."}}});
    server.start();

    const int before = g_destructive_spy.calls;
    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    auto sink = [](const HarnessEvent& event) {
        if (event.type == EventType::ApprovalRequired) {
            std::thread([id = event.data.value("id", "")] {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ApprovalBroker::instance().resolve(id, "deny");
            }).detach();
        }
        return true;
    };

    auto result = harness.run(json::array({user("delete event 99")}), sink);
    server.stop();

    check_eq(g_destructive_spy.calls - before, 0, "the tool did not run");
    check_eq(result.tool_calls, 0, "no tool call was counted");

    auto requests = server.requests();
    check_eq(requests.size(), size_t(2), "the model was told what happened");
    if (requests.size() >= 2) {
        auto tools = tool_messages(requests[1]);
        check_eq(tools.size(), size_t(1), "a tool message was still added, so the transcript stays valid");
        if (!tools.empty())
            check(tools[0].value("content", "").find("declined") != std::string::npos,
                  "the model was told the user declined");
    }
}

static void test_approval_timeout_denies() {
    test("an unanswered approval times out and is treated as a refusal");

    ScriptedServer server(
        {assistant_calling("test_destroy", {{"id", "evt-1"}}), {{"role", "assistant"}, {"content", "Left it."}}});
    server.start();

    const int before = g_destructive_spy.calls;
    Harness harness(server.url(), "test-model", true);
    RunOptions options = test_options();
    options.approval_timeout_seconds = 1; // nobody is going to answer
    harness.set_options(options);

    EventLog log;
    auto result = harness.run(json::array({user("delete it")}), log.sink());
    server.stop();

    check_eq(g_destructive_spy.calls - before, 0, "the tool did not run");
    json resolved = log.first(EventType::ApprovalResolved);
    check(resolved.is_object() && resolved.value("decision", "") == "timeout", "the timeout was reported");
    check(result.success, "the run still completed");
}

static void test_blocking_mode_refuses_without_asking() {
    test("with no channel to ask on, a destructive tool is refused rather than stalling");

    ScriptedServer server({assistant_calling("test_destroy", {{"id", "evt-7"}}),
                           {{"role", "assistant"}, {"content", "I cannot do that here."}}});
    server.start();

    const int before = g_destructive_spy.calls;
    Harness harness(server.url(), "test-model", true);
    RunOptions options = test_options();
    options.policy.can_ask = false;
    harness.set_options(options);

    const auto started = std::chrono::steady_clock::now();
    auto result = harness.run(json::array({user("delete it")}), nullptr);
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
    server.stop();

    check_eq(g_destructive_spy.calls - before, 0, "the tool did not run");
    check(elapsed < 2, "it refused immediately instead of waiting for a timeout");
    check(result.success, "the run completed");
}

static void test_iteration_budget() {
    test("a model that never stops calling tools is cut off by the iteration budget");

    // Longer than the budget, so the loop must be what stops it.
    std::vector<json> script;
    for (int i = 0; i < 20; i++)
        script.push_back(assistant_calling("test_read", json::object(), "c" + std::to_string(i)));

    ScriptedServer server(script);
    server.start();

    Harness harness(server.url(), "test-model", true);
    RunOptions options = test_options();
    options.max_iterations = 4;
    harness.set_options(options);

    EventLog log;
    auto result = harness.run(json::array({user("loop forever")}), log.sink());
    server.stop();

    check_eq(result.iterations, 4, "it stopped at the budget");
    check_eq(result.stop_reason, std::string("max_iterations"), "the stop reason says so");
    check(result.success, "the user still gets a reply");
    check(!result.content.empty(), "the reply is not empty");
}

static void test_client_abort_stops_the_run() {
    test("a disconnected client aborts the run immediately");

    ScriptedServer server({assistant_calling("test_read", json::object()),
                           {{"role", "assistant"}, {"content", "should never get here"}}});
    server.start();

    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    // Refusing the first event is how the server signals the client went away.
    auto result = harness.run(json::array({user("hello")}), [](const HarnessEvent&) { return false; });
    server.stop();

    check(result.client_aborted, "the run reports the abort");
    check_eq(result.stop_reason, std::string("client_aborted"), "the stop reason says so");
}

static void test_truncated_tool_call_is_not_executed() {
    test("a tool call cut off by the token limit is refused, not run with empty arguments");

    json cut_off = assistant_calling("test_write", {{"title", "half a"}});
    cut_off["finish_reason"] = "length";
    ScriptedServer server({cut_off, {{"role", "assistant"}, {"content", "Let me try again."}}});
    server.start();

    const int before = g_write_spy.calls;
    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("add it")}), log.sink());
    server.stop();

    check_eq(g_write_spy.calls - before, 0, "the tool did not run");
    check(result.success, "the run still completes");
    auto requests = server.requests();
    check_eq(requests.size(), size_t(2), "the model got a second turn");
    if (requests.size() == 2) {
        auto tools = tool_messages(requests[1]);
        check_eq(tools.size(), size_t(1), "the call was answered with a tool message");
        if (!tools.empty())
            check(tools[0].value("content", "").find("cut off") != std::string::npos,
                  "the tool message explains the call was cut off");
    }
    json first_result = log.first(EventType::ToolResult);
    check(first_result.is_object() && !first_result.value("success", true), "the UI sees a failed step");
}

static void test_unparseable_arguments_are_rejected() {
    test("a tool call with unparseable arguments is refused, not run with {}");

    json bad = {
        {"role", "assistant"},
        {"content", ""},
        {"tool_calls", json::array({{{"id", "call_bad"},
                                     {"type", "function"},
                                     {"function", {{"name", "test_write"}, {"arguments", "{\"title\": oops"}}}}})}};
    ScriptedServer server({bad, {{"role", "assistant"}, {"content", "Sorry."}}});
    server.start();

    const int before = g_write_spy.calls;
    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("add it")}), log.sink());
    server.stop();

    check_eq(g_write_spy.calls - before, 0, "the tool did not run");
    check(result.success, "the run still completes");
    auto requests = server.requests();
    if (requests.size() == 2) {
        auto tools = tool_messages(requests[1]);
        check_eq(tools.size(), size_t(1), "the call was answered with a tool message");
        if (!tools.empty()) {
            check_eq(tools[0].value("tool_call_id", ""), std::string("call_bad"), "keyed by the call id");
            check(tools[0].value("content", "").find("error") != std::string::npos, "and it carries an error");
        }
    } else {
        check(false, "the model got a second turn");
    }
}

static void test_client_abort_during_tool_result_stops_before_next_turn() {
    test("a client that disconnects while a tool result is reported stops the run before the next model call");

    ScriptedServer server({assistant_calling("test_read", json::object()),
                           {{"role", "assistant"}, {"content", "should never get here"}}});
    server.start();

    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    auto result = harness.run(json::array({user("hello")}),
                              [](const HarnessEvent& event) { return event.type != EventType::ToolResult; });
    server.stop();

    check(result.client_aborted, "the run reports the abort");
    check_eq(server.requests().size(), size_t(1), "no second request was made to the model");
}

static void test_transcript_delta_carries_tool_turns_into_the_next_run() {
    test("the run returns the messages it added, so the next turn can see what tools did");

    ScriptedServer server({assistant_calling("test_read", json::object(), "call_r"),
                           {{"role", "assistant"}, {"content", "I found alpha and beta."}}});
    server.start();

    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());
    EventLog log;
    auto result = harness.run(json::array({user("what is there?")}), log.sink());
    server.stop();

    check(result.success, "the first run succeeded");
    check_eq(result.transcript_delta.size(), size_t(3), "assistant call, tool result and final reply were returned");
    if (result.transcript_delta.size() == 3) {
        check(result.transcript_delta[0].contains("tool_calls"), "the first entry is the assistant's tool call");
        check_eq(result.transcript_delta[1].value("role", ""), std::string("tool"), "the second is the tool result");
        check_eq(result.transcript_delta[2].value("content", ""), std::string("I found alpha and beta."),
                 "the last is the final reply");
    }

    // Feed the delta back as history and check the model sees the earlier tool result.
    json history = json::array({user("what is there?")});
    for (const auto& msg : result.transcript_delta)
        history.push_back(msg);
    history.push_back(user("and the first one was?"));

    ScriptedServer second({{{"role", "assistant"}, {"content", "alpha"}}});
    second.start();
    Harness again(second.url(), "test-model", true);
    again.set_options(test_options());
    auto result2 = again.run(history, log.sink());
    second.stop();

    check(result2.success, "the second run succeeded");
    auto requests = second.requests();
    bool saw_alpha = false;
    if (!requests.empty()) {
        for (const auto& msg : tool_messages(requests[0]))
            if (msg.value("content", "").find("alpha") != std::string::npos)
                saw_alpha = true;
    }
    check(saw_alpha, "the model was shown the earlier tool result");
}

static void test_scratchpad_survives_an_unfinished_run() {
    test("a plan keyed by the conversation survives max_iterations and is shown on the next turn");

    auto& memory = MemoryStore::instance();
    const std::string convo = "conv_keep_going";
    memory.clear_plan(convo);
    memory.set_plan(
        convo, "tidy the folder",
        json::array({{{"step", "list files"}, {"status", "done"}}, {{"step", "delete junk"}, {"status", "pending"}}}));

    std::vector<json> script;
    for (int i = 0; i < 6; i++)
        script.push_back(assistant_calling("test_read", json::object(), "c" + std::to_string(i)));
    ScriptedServer server(script);
    server.start();

    Harness harness(server.url(), "test-model", true);
    RunOptions options = test_options();
    options.max_iterations = 2;
    options.scratchpad_id = convo;
    harness.set_options(options);
    EventLog log;
    auto result = harness.run(json::array({user("keep going")}), log.sink());
    server.stop();

    check_eq(result.stop_reason, std::string("max_iterations"), "the run hit its budget");
    auto requests = server.requests();
    bool prompt_has_plan = false;
    if (!requests.empty() && requests[0].contains("messages") && !requests[0]["messages"].empty())
        prompt_has_plan = requests[0]["messages"][0].value("content", "").find("delete junk") != std::string::npos;
    check(prompt_has_plan, "the system prompt showed the conversation's plan");
    check(memory.get_plan(convo).is_object(), "the plan is still there for the next turn");

    // When the model finishes its turn the plan has served its purpose, even if it never marked
    // the steps done -- small models rarely do -- so a stale plan is not carried into the next turn.
    ScriptedServer finished({{{"role", "assistant"}, {"content", "All tidy."}}});
    finished.start();
    Harness h2(finished.url(), "test-model", true);
    h2.set_options(options);
    h2.run(json::array({user("thanks")}), log.sink());
    finished.stop();
    check(memory.get_plan(convo).is_null(), "a normal stop clears the plan even with steps still pending");
}

// ----------------------------------------------------------- context manager

static void test_context_keeps_system_prompt_and_recent_turns() {
    test("the context manager keeps the system prompt and drops the oldest turns");

    ContextManager context(1200, 256);
    json history = json::array();
    for (int i = 0; i < 40; i++) {
        history.push_back({{"role", i % 2 == 0 ? "user" : "assistant"},
                           {"content", std::string("message number ") + std::to_string(i) +
                                           " with enough text to take up a meaningful number of tokens"}});
    }

    json built = context.build("SYSTEM PROMPT", history);

    check(built.size() >= 2, "something was kept");
    check_eq(built[0].value("role", ""), std::string("system"), "the system prompt comes first");
    check(built[0].value("content", "").find("SYSTEM PROMPT") != std::string::npos, "it is the prompt we passed");
    check(context.stats().dropped_messages > 0, "older messages were dropped");
    check(built.size() < history.size(), "the transcript really did shrink");

    // The newest turn must always survive -- it is what the user just said.
    const std::string last = built[built.size() - 1].value("content", "");
    check(last.find("message number 39") != std::string::npos, "the most recent message was kept");
}

static void test_context_never_orphans_tool_messages() {
    test("tool results are never separated from the assistant turn that requested them");

    ContextManager context(900, 256);
    json history = json::array();
    for (int i = 0; i < 12; i++) {
        history.push_back(user(std::string("please do task ") + std::to_string(i) +
                               " which needs a reasonably long description to consume tokens"));
        history.push_back(
            {{"role", "assistant"},
             {"content", ""},
             {"tool_calls", json::array({tool_call("test_read", json::object(), "call_" + std::to_string(i))})}});
        history.push_back({{"role", "tool"},
                           {"tool_call_id", "call_" + std::to_string(i)},
                           {"name", "test_read"},
                           {"content", std::string("result for task ") + std::to_string(i)}});
    }

    json built = context.build("SYSTEM", history);

    // Walk what survived: a tool message is only valid if the message before it is an assistant
    // turn with tool_calls, or another tool message from the same batch.
    bool previous_allows_tool = false;
    bool orphan_found = false;
    for (const auto& message : built) {
        const std::string role = message.value("role", "");
        if (role == "tool" && !previous_allows_tool) {
            orphan_found = true;
            break;
        }
        previous_allows_tool =
            (role == "assistant" && message.contains("tool_calls")) || (role == "tool" && previous_allows_tool);
    }
    check(!orphan_found, "no tool message was left without its assistant turn");
    check(context.stats().dropped_messages > 0, "the history was actually trimmed");
}

static void test_context_truncates_huge_tool_results() {
    test("an oversized tool result is truncated instead of evicting the conversation");

    ContextManager context(4096, 256);
    json history = json::array();
    history.push_back(user("run the build"));
    history.push_back({{"role", "assistant"},
                       {"content", ""},
                       {"tool_calls", json::array({tool_call("test_read", json::object(), "call_0")})}});
    history.push_back(
        {{"role", "tool"}, {"tool_call_id", "call_0"}, {"name", "test_read"}, {"content", std::string(200000, 'x')}});

    json built = context.build("SYSTEM", history);

    check_eq(context.stats().truncated_results, 1, "the result was truncated");
    check_eq(built.size(), size_t(4), "every message survived");
    const std::string tool_content = built[built.size() - 1].value("content", "");
    check(tool_content.size() < 20000, "the tool output was cut down");
    check(tool_content.find("truncated by Delta") != std::string::npos, "the truncation is visible to the model");
}

static void test_context_emits_one_system_message_starting_on_a_user_turn() {
    test("compaction keeps a single system message and opens the window on a user turn");

    // Several chat templates (Gemma's among them) raise on a second system message or on a
    // window that starts with an assistant turn, so this is a hard requirement, not a nicety.
    ContextManager context(1200, 256);
    context.set_summarizer([](const nlohmann::json&) { return std::string("They discussed scheduling."); });

    json history = json::array();
    for (int i = 0; i < 40; i++) {
        history.push_back({{"role", i % 2 == 0 ? "user" : "assistant"},
                           {"content", std::string("turn ") + std::to_string(i) +
                                           " with plenty of words so the budget is exceeded quickly"}});
    }

    json built = context.build("SYSTEM PROMPT", history);

    int system_count = 0;
    for (const auto& message : built)
        if (message.value("role", "") == "system")
            system_count++;
    check_eq(system_count, 1, "there is exactly one system message");
    check_eq(built[0].value("role", ""), std::string("system"), "and it is first");
    check(built[0].value("content", "").find("They discussed scheduling") != std::string::npos,
          "the summary was folded into the system prompt");
    check(context.stats().summarized, "the summary was recorded in the stats");

    check(built.size() > 1, "some history survived");
    if (built.size() > 1)
        check_eq(built[1].value("role", ""), std::string("user"), "the history opens on a user turn");

    // And from there it must alternate.
    bool alternates = true;
    for (size_t i = 2; i < built.size(); i++) {
        if (built[i].value("role", "") == built[i - 1].value("role", ""))
            alternates = false;
    }
    check(alternates, "the kept turns alternate");
}

static void test_context_folds_client_system_messages() {
    test("a system message from the client is folded into the prompt, not treated as history");

    ContextManager context(2048, 256);
    json history = json::array({{{"role", "system"}, {"content", "Call the user Jovine."}}, user("hello")});

    json built = context.build("BASE PROMPT", history);

    check_eq(built[0].value("role", ""), std::string("system"), "there is one system message");
    check(built[0].value("content", "").find("BASE PROMPT") != std::string::npos, "the base prompt is there");
    check(built[0].value("content", "").find("Jovine") != std::string::npos, "so is the client's instruction");
    for (size_t i = 1; i < built.size(); i++)
        check(built[i].value("role", "") != "system", "no stray system message is left in the history");
}

// -------------------------------------------------------------- memory store

static void test_memory_store_roundtrip() {
    test("memories can be saved, searched, and forgotten");

    auto& memory = MemoryStore::instance();
    check(memory.ready(), "the store is initialised");

    const std::string id = memory.remember("Jovine prefers concise answers", "preference", "style", 3, "test");
    check(!id.empty(), "a memory was saved");

    auto found = memory.search("concise", 5);
    check(!found.empty(), "keyword search finds it");
    if (!found.empty())
        check_eq(found[0].content, std::string("Jovine prefers concise answers"), "the right memory came back");

    auto pinned = memory.pinned(5);
    bool in_pinned = false;
    for (const auto& m : pinned)
        if (m.id == id)
            in_pinned = true;
    check(in_pinned, "importance 3 makes it always-loaded");

    // Saving the same text again must update rather than duplicate.
    const std::string again = memory.remember("Jovine prefers concise answers", "preference", "style", 2, "test");
    check_eq(again, id, "an identical memory is de-duplicated");

    check(memory.forget(id), "it can be forgotten");
    check(!memory.forget(id), "forgetting it twice reports nothing to do");
}

static void test_policy_is_remembered() {
    test("an 'always allow' answer is remembered for later runs");

    auto& memory = MemoryStore::instance();
    memory.set_policy("test_destroy", "allow");
    check_eq(memory.get_policy("test_destroy"), std::string("allow"), "the decision was stored");

    ScriptedServer server(
        {assistant_calling("test_destroy", {{"id", "evt-5"}}), {{"role", "assistant"}, {"content", "Done."}}});
    server.start();

    const int before = g_destructive_spy.calls;
    Harness harness(server.url(), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("delete it")}), log.sink());
    server.stop();

    check_eq(log.count(EventType::ApprovalRequired), 0, "no approval was asked for the second time");
    check_eq(g_destructive_spy.calls - before, 1, "the tool ran straight away");
    check(result.success, "the run succeeded");

    memory.clear_policies();
    check_eq(memory.get_policy("test_destroy"), std::string(""), "policies can be cleared");
}

static void test_scratchpad_plan() {
    test("the run scratchpad stores and updates a plan");

    auto& memory = MemoryStore::instance();
    const std::string run = "run_test_plan";
    memory.set_plan(run, "tidy up",
                    json::array({{{"step", "one"}, {"status", "pending"}}, {{"step", "two"}, {"status", "pending"}}}));

    json plan = memory.get_plan(run);
    check(plan.is_object(), "the plan came back");
    check_eq(plan.value("goal", ""), std::string("tidy up"), "the goal was stored");
    check_eq(plan["steps"].size(), size_t(2), "both steps were stored");

    memory.clear_plan(run);
    check(memory.get_plan(run).is_null(), "clearing the plan removes it");
}

static void test_transport_failure_is_not_mistaken_for_schema_rejection() {
    test("a transport failure ends the run with an error instead of retrying without tools");

    // Nothing listens here, so every request fails at the transport level.
    httplib::Server placeholder;
    const int port = placeholder.bind_to_any_port("127.0.0.1");
    placeholder.stop();

    Harness harness("http://127.0.0.1:" + std::to_string(port), "test-model", true);
    harness.set_options(test_options());

    EventLog log;
    auto result = harness.run(json::array({user("hello")}), log.sink());

    check(!result.success, "the run failed");
    check_eq(result.stop_reason, std::string("error"), "with an error stop reason");
    json status = log.first(EventType::Status);
    check(status.is_null() || status.value("message", "").find("rejected the tool schemas") == std::string::npos,
          "the user was not told the model rejected the tool schemas");
}

static void test_schema_rejection_retries_without_tools() {
    test("an HTTP 400 for a request with tools is retried without them");

    httplib::Server server;
    std::mutex mutex;
    std::vector<bool> saw_tools;
    server.Get("/props", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(json{{"n_ctx", 4096}}.dump(), "application/json");
    });
    server.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body, nullptr, false);
        const bool with_tools = body.is_object() && body.contains("tools") && !body["tools"].empty();
        {
            std::lock_guard<std::mutex> lock(mutex);
            saw_tools.push_back(with_tools);
        }
        if (with_tools) {
            res.status = 400;
            res.set_content(json{{"error", {{"message", "tools are not supported by this template"}}}}.dump(),
                            "application/json");
            return;
        }
        json chunk = {
            {"object", "chat.completion.chunk"},
            {"choices",
             json::array({{{"index", 0}, {"delta", {{"content", "plain answer"}}}, {"finish_reason", "stop"}}})}};
        res.set_content("data: " + chunk.dump() + "\n\ndata: [DONE]\n\n", "text/event-stream");
    });
    const int port = server.bind_to_any_port("127.0.0.1");
    std::thread thread([&server] { server.listen_after_bind(); });
    server.wait_until_ready();

    Harness harness("http://127.0.0.1:" + std::to_string(port), "test-model", true);
    harness.set_options(test_options());
    EventLog log;
    auto result = harness.run(json::array({user("hello")}), log.sink());

    server.stop();
    thread.join();

    check(result.success, "the run succeeded on the retry");
    check_eq(result.content, std::string("plain answer"), "with the tool-less answer");
    check_eq(saw_tools.size(), size_t(2), "exactly one retry was made");
    if (saw_tools.size() == 2)
        check(saw_tools[0] && !saw_tools[1], "the retry dropped the tool schemas");
}

// ---------------------------------------------------------------------- main

int main() {
    std::cout << "Delta harness tests\n===================\n";

    // A scratch database, so tests never touch the user's real one.
    const std::string db_path = "/tmp/delta-harness-test.db";
    std::remove(db_path.c_str());
    if (!AgentDatabase::instance().init(db_path)) {
        std::cerr << "could not open the test database\n";
        return 1;
    }
    if (!MemoryStore::instance().init(AgentDatabase::instance().handle())) {
        std::cerr << "could not initialise the memory store\n";
        return 1;
    }
    register_test_tools();

    test_multi_step_loop();
    test_write_result_reaches_model();
    test_tool_failure_is_reported_to_model();
    test_chained_tools();
    test_destructive_tool_requires_approval();
    test_approval_endpoint_resumes_a_parked_run();
    test_approval_endpoint_rejects_bad_input();
    test_denied_approval_tells_the_model();
    test_approval_timeout_denies();
    test_blocking_mode_refuses_without_asking();
    test_iteration_budget();
    test_client_abort_stops_the_run();
    test_truncated_tool_call_is_not_executed();
    test_unparseable_arguments_are_rejected();
    test_client_abort_during_tool_result_stops_before_next_turn();
    test_transcript_delta_carries_tool_turns_into_the_next_run();
    test_scratchpad_survives_an_unfinished_run();
    test_transport_failure_is_not_mistaken_for_schema_rejection();
    test_schema_rejection_retries_without_tools();

    test_context_keeps_system_prompt_and_recent_turns();
    test_context_never_orphans_tool_messages();
    test_context_truncates_huge_tool_results();
    test_context_emits_one_system_message_starting_on_a_user_turn();
    test_context_folds_client_system_messages();

    test_memory_store_roundtrip();
    test_policy_is_remembered();
    test_scratchpad_plan();

    AgentDatabase::instance().close();
    std::remove(db_path.c_str());

    std::cout << "\n===================\n";
    std::cout << g_checks << " checks, " << g_failures << " failed\n";
    return g_failures == 0 ? 0 : 1;
}
