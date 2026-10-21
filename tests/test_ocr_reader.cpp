/**
 * Tests for the text reader that reads scanned PDF pages: which model a machine gets, cleaning
 * the model's output, the /api/ocr routes, and reading a page from a stand-in llama-server.
 *
 *   cmake --build build --target delta-ocr-tests && ./build/delta-ocr-tests
 */

#include "agent/ocr_reader.h"
#include "api/ocr_routes.h"
#include "cpu_level.h"

#include <cpp-httplib/httplib.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace delta::agent;

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

static fs::path scratch(const std::string& name) {
    const fs::path dir = fs::temp_directory_path() / ("delta-ocr-test-" + name);
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

static void write_file(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}

template <typename F> static bool wait_until(F done) {
    for (int i = 0; i < 400; i++) {
        if (done())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

static void model_choice_tests() {
    section("which reader a machine gets");
    const auto none = [](const OcrModel&) { return false; };
    check(pick_ocr_model(8, none).id == "glm-ocr", "8 GB: GLM-OCR");
    check(pick_ocr_model(16, none).id == "unlimited-ocr", "16 GB: Unlimited-OCR");
    check(pick_ocr_model(0, none).id == "glm-ocr", "memory unknown: the small one");
    check(pick_ocr_model(64, [](const OcrModel& m) { return m.id == "glm-ocr"; }).id == "glm-ocr",
          "an installed reader is kept, never downloaded twice");
    for (const auto& m : ocr_models())
        check(!m.model_file.empty() && !m.mmproj_file.empty() && m.size_bytes > 0,
              m.display_name + ": model, projector and size set");
}

static void cleanup_tests() {
    section("cleaning what the reader returns");
    check(clean_ocr_text("<|ref|>title<|/ref|><|det|>[[10, 20, 300, 40]]<|/det|>\nWater Report\n\n\n\nPage text") ==
              "Water Report\n\nPage text",
          "layout tags and extra blank lines are removed");
    check(clean_ocr_text("```markdown\n# Heading\n\n| a | b |\n```") == "# Heading\n\n| a | b |",
          "a code fence around the whole page is removed");
    check(clean_ocr_text("  \n<|end_of_sentence|>  ") == "", "a page with nothing on it is empty");
    check(clean_ocr_text("Total: 38 boreholes") == "Total: 38 boreholes", "plain text is kept as it is");
}

static void request_tests() {
    section("what is sent to the reader");
    const auto& glm = ocr_models().front();
    const json body = ocr_request(glm, "data:image/png;base64,AAAA");
    const auto& content = body["messages"][0]["content"];
    check(content[0]["type"] == "image_url" && content[0]["image_url"]["url"] == "data:image/png;base64,AAAA",
          "the page image goes first");
    check(content[1]["text"] == glm.prompt, "then the model's own prompt");
    check(body["temperature"] == 0 && body["stream"] == false, "no sampling, no streaming");

    const auto args = ocr_server_args(glm, "/tmp/ocr/glm-ocr", 5123);
    const auto has = [&](const std::string& a, const std::string& b) {
        for (size_t i = 0; i + 1 < args.size(); i++)
            if (args[i] == a && args[i + 1] == b)
                return true;
        return false;
    };
    check(has("--mmproj", (fs::path("/tmp/ocr/glm-ocr") / glm.mmproj_file).string()), "the projector is loaded");
    check(has("--port", "5123") && has("--host", "127.0.0.1"), "listens on this computer only");
    const auto& unlimited = ocr_models().back();
    const auto u = ocr_server_args(unlimited, "/tmp/ocr/u", 1);
    check(std::find(u.begin(), u.end(), "deepseek-ocr") != u.end(), "Unlimited-OCR uses its DeepSeek-OCR template");
}

static void binary_tests() {
    section("finding llama-server");
    const fs::path dir = scratch("bin");
#ifdef _WIN32
    const std::string ext = ".exe";
#else
    const std::string ext;
#endif
    write_file(dir / ("delta-server" + ext), "x");
    const std::string found_none = find_llama_server(dir.string());
    check(found_none.find("delta-server") == std::string::npos, "delta-server is never used as the reader");
    write_file(dir / ("llama-server" + ext), "x");
    check(fs::path(find_llama_server(dir.string())) == dir / ("llama-server" + ext),
          "the llama-server next to Delta is used");
    fs::remove_all(dir);
}

static void cpu_level_tests() {
    section("picking the AVX2 llama-server");
    const fs::path dir = scratch("cpu");
#ifdef _WIN32
    const std::string ext = ".exe";
#else
    const std::string ext;
#endif
    const std::string base = (dir / ("llama-server" + ext)).string();
    write_file(base, "x");
    check(delta::fastest_llama_server(base, true) == base, "no AVX2 build shipped: the baseline is used");
    write_file(dir / ("llama-server-v3" + ext), "x");
    check(fs::path(delta::fastest_llama_server(base, true)) == dir / ("llama-server-v3" + ext),
          "a CPU with AVX2 gets the fast build");
    check(delta::fastest_llama_server(base, false) == base, "an older CPU keeps the baseline");
    const std::string dev = (dir / ("llama-server-x86_64-unknown-linux-gnu" + ext)).string();
    write_file(dev, "x");
    write_file(dir / ("llama-server-v3-x86_64-unknown-linux-gnu" + ext), "x");
    check(fs::path(delta::fastest_llama_server(dev, true)) == dir / ("llama-server-v3-x86_64-unknown-linux-gnu" + ext),
          "Tauri's target-suffixed names map to their AVX2 build");
    check(delta::is_v3_llama_server("llama-server-v3") && !delta::is_v3_llama_server("llama-server-x86_64"),
          "the AVX2 build is told apart from the baseline");
    fs::remove_all(dir);

    const fs::path only_v3 = scratch("cpu-only-v3");
    write_file(only_v3 / ("llama-server-v3-x86_64-unknown-linux-gnu" + ext), "x");
    check(find_llama_server(only_v3.string()).find("llama-server-v3") == std::string::npos,
          "the AVX2 build is never picked as the baseline");
    fs::remove_all(only_v3);
}

static void route_tests() {
    section("downloading the reader");
    auto& reader = OcrReader::instance();
    const fs::path root = scratch("root");
    write_file(root / "glm-ocr" / "stale.gguf.tmp", "partial");

    std::atomic<bool> release{false};
    std::atomic<bool> cancel_called{false};
    OcrReader::Hooks hooks;
    hooks.download = [&](const std::string& url, const std::string& dest,
                         const std::function<void(long long, long long)>& progress) {
        if (url.find("https://huggingface.co/ggml-org/GLM-OCR-GGUF/resolve/main/") != 0)
            return false;
        progress(1000, 2000);
        if (!wait_until([&] { return release.load() || cancel_called.load(); }) || cancel_called)
            return false;
        write_file(dest, std::string(64, 'g'));
        return true;
    };
    hooks.cancel = [&] { cancel_called = true; };
    reader.configure(root.string(), "", 8, hooks);
    check(!fs::exists(root / "glm-ocr" / "stale.gguf.tmp"), "partial files from an earlier run are removed");

    TestServer api;
    delta::api::register_ocr_routes(api.server);
    api.start();
    httplib::Client cli("127.0.0.1", api.port);

    auto status = json::parse(cli.Get("/api/ocr")->body);
    check(status["model"]["id"] == "glm-ocr" && status["installed"] == false && status["downloading"] == false,
          "status names the reader and says it is not downloaded");
    auto page = cli.Post("/api/ocr/page", R"({"image":"AAAA"})", "application/json");
    check(page && page->status == 409, "reading a page before the download: 409");

    auto started = cli.Post("/api/ocr/download", "", "application/json");
    check(started && started->status == 200, "download starts");
    check(wait_until([&] { return reader.status()["current_bytes"] == 1000; }), "progress is reported");
    auto again = cli.Post("/api/ocr/download", "", "application/json");
    check(again && again->status == 409, "a second download while one runs: 409");
    cli.Post("/api/ocr/download/cancel", "", "application/json");
    check(wait_until([&] { return reader.status()["downloading"] == false; }), "cancel stops it");
    status = reader.status();
    check(status["installed"] == false && status.value("error", "") == "Download cancelled", "and says so");

    cancel_called = false;
    release = true;
    cli.Post("/api/ocr/download", "", "application/json");
    check(wait_until([&] { return reader.installed(); }), "a second try downloads model and projector");
    status = json::parse(cli.Get("/api/ocr")->body);
    check(status["installed"] == true && status["progress"] == 100.0 && !status.contains("error"),
          "status: installed, 100%");
    check(fs::exists(root / "glm-ocr" / "GLM-OCR-Q8_0.gguf") &&
              fs::exists(root / "glm-ocr" / "mmproj-GLM-OCR-Q8_0.gguf"),
          "files are kept in their own folder, apart from chat models");
    auto done = cli.Post("/api/ocr/download", "", "application/json");
    check(done && done->status == 200 && json::parse(done->body)["installed"] == true,
          "download when installed: nothing to do");

    section("reading a page");
    TestServer llama;
    json seen;
    llama.server.Post("/v1/chat/completions", [&](const httplib::Request& req, httplib::Response& res) {
        seen = json::parse(req.body);
        json reply = {{"choices", json::array({{{"message",
                                                 {{"role", "assistant"},
                                                  {"content", "<|ref|>text<|/ref|><|det|>[[1,2,3,4]]<|/det|>\n38 "
                                                              "boreholes serve 9,400 households."}}}}})}};
        res.set_content(reply.dump(), "application/json");
    });
    llama.start();
    reader.use_server_for_tests(llama.port);

    page = cli.Post("/api/ocr/page", R"({"image":"AAAA"})", "application/json");
    check(page && page->status == 200, "a page is read");
    check(page && json::parse(page->body)["text"] == "38 boreholes serve 9,400 households.",
          "its text comes back without layout tags");
    check(seen["messages"][0]["content"][0]["image_url"]["url"] == "data:image/png;base64,AAAA",
          "bare base64 is sent as a PNG data URL");
    page = cli.Post("/api/ocr/page", R"({"image":""})", "application/json");
    check(page && page->status == 400, "no image: 400");
    page = cli.Post("/api/ocr/page", "not json", "application/json");
    check(page && page->status == 400, "bad JSON: 400");

    llama.server.stop();
    page = cli.Post("/api/ocr/page", R"({"image":"data:image/png;base64,AAAA"})", "application/json");
    check(page && page->status == 503, "the reader not answering: 503");
    fs::remove_all(root);
}

int main() {
    model_choice_tests();
    cleanup_tests();
    request_tests();
    binary_tests();
    cpu_level_tests();
    route_tests();
    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}
