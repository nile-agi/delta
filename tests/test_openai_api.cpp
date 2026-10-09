/**
 * Tests for the OpenAI-compatible API: who may call it, the saved settings, the Developer routes
 * and the proxy to llama-server (a stand-in here).
 *
 *   cmake --build build --target delta-openai-tests && ./build/delta-openai-tests
 */

#include "api/openai_api.h"

#include <cpp-httplib/httplib.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace delta::api;

static int g_failures = 0;
static int g_checks = 0;

static void check(bool condition, const std::string& what) {
    g_checks++;
    std::cout << (condition ? "    ok   " : "  FAIL   ") << what << "\n";
    if (!condition)
        g_failures++;
}

static void section(const std::string& name) {
    std::cout << "\n" << name << "\n";
}

// An httplib server on a free local port, running until destroyed.
struct TestServer {
    httplib::Server server;
    std::thread thread;
    int port = 0;

    void start() {
        port = server.bind_to_any_port("127.0.0.1");
        thread = std::thread([this] { server.listen_after_bind(); });
        for (int i = 0; i < 200 && !server.is_running(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ~TestServer() {
        server.stop();
        if (thread.joinable())
            thread.join();
    }
};

static void gate_tests() {
    section("who may call the API");
    const std::string key = "delta-secret";
    check(check_request("POST", "/v1/chat/completions", "127.0.0.1", "", "", "").status == 0,
          "this computer, no key set: allowed");
    check(check_request("POST", "/v1/chat/completions", "192.168.1.20", "", "", "").status == 0,
          "another device, no key set: allowed (only reachable when shared)");
    check(check_request("GET", "/api/v1/system/resource-hogs", "192.168.1.20", "", "", "").status == 403,
          "another device never reaches Delta's own routes");
    check(check_request("POST", "/v1/agent/approve", "192.168.1.20", "Bearer " + key, "", key).status == 403,
          "not even with the key");
    check(check_request("GET", "/api/models/list", "127.0.0.1", "", "", key).status == 0,
          "Delta's own routes on this computer need no key");

    auto missing = check_request("POST", "/v1/chat/completions", "127.0.0.1", "", "", key);
    check(missing.status == 401 && missing.body["error"]["code"] == "invalid_api_key", "a missing key is a 401");
    check(check_request("POST", "/v1/chat/completions", "127.0.0.1", "Bearer wrong", "", key).status == 401,
          "a wrong key is a 401");
    check(check_request("POST", "/v1/chat/completions", "10.0.0.5", "Bearer " + key, "", key).status == 0,
          "the right Bearer key is let in");
    check(check_request("GET", "/v1/models", "10.0.0.5", "bearer " + key, "", key).status == 0,
          "\"bearer\" in any case");
    check(check_request("GET", "/v1/models/qwen", "10.0.0.5", "", key, key).status == 0, "X-Api-Key works too");
    check(check_request("OPTIONS", "/v1/models", "10.0.0.5", "", "", key).status == 0, "CORS preflight passes");

    check(is_loopback("127.0.0.1") && is_loopback("::1") && is_loopback("::ffff:127.0.0.1"), "loopback addresses");
    check(!is_loopback("192.168.1.2") && !is_loopback("0.0.0.0") && !is_loopback(""), "everything else is not");

    section("trusted origins");
    check(trusted_origin(""), "no Origin (curl, SDKs, the app's own requests)");
    check(trusted_origin("http://localhost:8080") && trusted_origin("http://127.0.0.1:5173"), "local pages");
    check(trusted_origin("tauri://localhost") && trusted_origin("http://tauri.localhost"), "the desktop app");
    check(!trusted_origin("https://evil.example") && !trusted_origin("http://localhost.evil.example"), "other sites");

    section("Delta's chat versus a plain OpenAI call");
    check(!is_delta_request(json{{"model", "m"}, {"messages", json::array()}, {"tools", json::array()}}),
          "an SDK request with its own tools is plain");
    check(is_delta_request(json{{"conversation_id", "c1"}}), "conversation_id marks Delta's chat");
    check(is_delta_request(json{{"use_tools", false}}), "use_tools too");
    check(is_delta_request(json{{"use_calendar_tools", true}}), "and the per-category switches");
    check(!is_delta_request(json{{"use_x", true}}), "other use_ keys do not");
}

static void settings_tests() {
    section("saved settings");
    const fs::path dir = fs::temp_directory_path() / "delta-openai-tests";
    fs::remove_all(dir);
    const std::string path = (dir / "api_server.json").string();

    ApiAccess fresh(path);
    fresh.load();
    check(!fresh.saved().network && fresh.api_key().empty() && fresh.host() == "127.0.0.1",
          "defaults: this computer only, no key");

    check(fresh.save({true, "delta-abc"}), "saves");
    ApiAccess reloaded(path);
    reloaded.load();
    check(reloaded.saved().network && reloaded.api_key() == "delta-abc" && reloaded.host() == "0.0.0.0",
          "reloads network access and the key");
#ifndef _WIN32
    check((fs::status(path).permissions() & (fs::perms::group_all | fs::perms::others_all)) == fs::perms::none,
          "the file is readable by its owner only");
#endif

    std::ofstream(path) << R"({"network": true, "api_key": ""})";
    ApiAccess keyless(path);
    keyless.load();
    check(!keyless.saved().network && keyless.host() == "127.0.0.1", "network access without a key is ignored");

    std::ofstream(path) << "not json";
    ApiAccess damaged(path);
    damaged.load();
    check(!damaged.saved().network && damaged.api_key().empty(), "a damaged file falls back to defaults");

    reloaded.set_overrides("0.0.0.0", "delta-flag");
    check(reloaded.host() == "0.0.0.0" && reloaded.api_key() == "delta-flag" && reloaded.overridden(),
          "command-line flags win for this run");

    const std::string a = generate_api_key(), b = generate_api_key();
    check(a.rfind("delta-", 0) == 0 && a.size() == 46 && a != b, "generated keys are long and different");
    check(mask_api_key(a).find(a.substr(10, 20)) == std::string::npos && mask_api_key(a).size() < a.size(),
          "a masked key hides the middle");
    fs::remove_all(dir);
}

static void developer_route_tests() {
    section("Developer settings routes");
    ApiAccess access("");
    int rebinds = 0;
    TestServer api;
    register_developer_routes(api.server, {&access, [] { return 8081; }, [&rebinds] { rebinds++; }});
    api.start();
    httplib::Client cli("127.0.0.1", api.port);

    auto got = cli.Get("/api/developer/server");
    json state = got ? json::parse(got->body) : json();
    check(got && got->status == 200 && state["local_url"] == "http://127.0.0.1:8081/v1" && !state["network"] &&
              !state["has_key"],
          "reports the local address");

    auto on = cli.Put("/api/developer/server", R"({"network": true})", "application/json");
    state = on ? json::parse(on->body) : json();
    check(on && on->status == 200 && state["network"] && state["has_key"] && access.host() == "0.0.0.0",
          "turning on network access creates a key");
    check(rebinds == 1, "and moves the listener");
    const std::string key = state.value("api_key", "");

    auto removing = cli.Put("/api/developer/server", R"({"key": "remove"})", "application/json");
    check(removing && removing->status == 400 && access.api_key() == key, "the key stays while shared");

    auto masked = cli.Get("/api/developer/server", httplib::Headers{{"Origin", "https://evil.example"}});
    check(masked && json::parse(masked->body).value("api_key", "") != key, "other sites only see it masked");
    auto shown = cli.Get("/api/developer/server", httplib::Headers{{"Origin", "http://localhost:8080"}});
    check(shown && json::parse(shown->body).value("api_key", "") == key, "Delta's own page sees it");

    auto evil = cli.Put("/api/developer/server", httplib::Headers{{"Origin", "https://evil.example"}},
                        R"({"network": false})", "application/json");
    check(evil && evil->status == 403 && access.host() == "0.0.0.0", "other sites cannot change anything");

    auto off = cli.Put("/api/developer/server", R"({"network": false, "key": "remove"})", "application/json");
    check(off && off->status == 200 && access.host() == "127.0.0.1" && access.api_key().empty() && rebinds == 2,
          "turning it off and removing the key");
}

static void proxy_tests() {
    section("plain OpenAI calls reach llama-server");
    TestServer llama;
    json last_body;
    llama.server.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        last_body = json::parse(req.body);
        if (last_body.value("model", "") == "missing") {
            res.status = 400;
            res.set_content(
                R"({"error":{"code":400,"message":"model 'missing' not found","type":"invalid_request_error"}})",
                "application/json");
            return;
        }
        if (last_body.value("stream", false)) {
            res.set_content("data: {\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}\n\n"
                            "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"}}]}\n\n"
                            "data: [DONE]\n\n",
                            "text/event-stream");
            return;
        }
        json call = {
            {"id", "call_1"}, {"type", "function"}, {"function", {{"name", "get_weather"}, {"arguments", "{}"}}}};
        res.set_content(json{{"object", "chat.completion"},
                             {"choices",
                              {{{"index", 0},
                                {"message", {{"role", "assistant"}, {"tool_calls", {call}}}},
                                {"finish_reason", "tool_calls"}}}},
                             {"usage", {{"prompt_tokens", 12}, {"completion_tokens", 3}, {"total_tokens", 15}}}}
                            .dump(),
                        "application/json");
    });
    llama.start();

    TestServer api;
    api.server.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        proxy_openai(res, "/v1/chat/completions", json::parse(req.body), "127.0.0.1", llama.port);
    });
    api.start();
    httplib::Client cli("127.0.0.1", api.port);

    json request = {{"model", "qwen"},
                    {"messages", {{{"role", "user"}, {"content", "weather?"}}}},
                    {"tools", {{{"type", "function"}, {"function", {{"name", "get_weather"}}}}}},
                    {"stop", {"\n\n"}}};
    auto plain = cli.Post("/v1/chat/completions", request.dump(), "application/json");
    json reply = plain ? json::parse(plain->body) : json();
    check(plain && plain->status == 200, "a plain request succeeds");
    check(last_body["tools"] == request["tools"] && last_body["stop"] == request["stop"],
          "the client's tools and stop words reach the model");
    check(reply["choices"][0]["finish_reason"] == "tool_calls" && reply["choices"][0]["message"].contains("tool_calls"),
          "tool_calls come back for the client to run");
    check(reply["usage"]["total_tokens"] == 15, "with real usage");

    request["stream"] = true;
    std::string streamed;
    auto st = cli.Post("/v1/chat/completions", request.dump(), "application/json");
    if (st)
        streamed = st->body;
    check(st && st->status == 200 && st->get_header_value("Content-Type").find("text/event-stream") == 0,
          "a stream is an event stream");
    check(streamed.find("\"role\":\"assistant\"") != std::string::npos &&
              streamed.find("\"content\":\"Hi\"") != std::string::npos &&
              streamed.find("data: [DONE]") != std::string::npos,
          "every chunk and [DONE] arrive as sent");

    request["model"] = "missing";
    request["stream"] = false;
    auto bad = cli.Post("/v1/chat/completions", request.dump(), "application/json");
    check(bad && bad->status == 400 && json::parse(bad->body)["error"]["message"] == "model 'missing' not found",
          "llama-server's error keeps its status and message");
    request["stream"] = true;
    auto bad_stream = cli.Post("/v1/chat/completions", request.dump(), "application/json");
    check(bad_stream && bad_stream->body.rfind("data: {\"error\"", 0) == 0,
          "in a stream the error arrives as an event an SDK raises");

    TestServer down;
    down.server.Post("/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
        proxy_openai(res, "/v1/chat/completions", json::parse(req.body), "127.0.0.1", 1);
    });
    down.start();
    httplib::Client down_cli("127.0.0.1", down.port);
    auto none = down_cli.Post("/v1/chat/completions", R"({"model":"m","messages":[]})", "application/json");
    check(none && none->status == 502 && json::parse(none->body)["error"]["code"] == "model_unavailable",
          "no model server is a clean 502");
}

int main() {
    gate_tests();
    settings_tests();
    developer_route_tests();
    proxy_tests();
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
