/**
 * Model Management API Server
 * Provides HTTP endpoints for model management operations
 *
 * This server runs on port 8081 and provides REST API endpoints for:
 * - GET /api/models/available - List all available models
 * - GET /api/models/list - List installed models
 * - POST /api/models/download - Download a model
 * - DELETE /api/models/:name - Remove a model
 * - POST /api/models/use - Switch to a model
 */

#include "delta_cli.h"
#include "model_api_server.h"
#include "agent/agent_database.h"
#include "agent/tool_registry.h"
#include "agent/agent_loop.h"

#include "tools/hardware_monitor.h"

// Updated includes to point to the local vendor directory structure
#include "vendor/llama.cpp/vendor/cpp-httplib/httplib.h"
#include "api/note_routes.h"  
#include "vendor/json.hpp"

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <iomanip>
#include <future>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <sys/sysinfo.h>
#endif

using json = nlohmann::json;

// DHATS: Global hardware telemetry monitor
static delta::HardwareMonitor g_hardware_monitor;

namespace delta {

// OpenAI-compatible SSE frames for /v1/chat/completions streaming.
static json sse_content_chunk(const std::string& text) {
    json delta{{"content", text}};
    json choice{{"index", 0}, {"delta", delta}, {"finish_reason", nullptr}};
    return json{{"id", "chatcmpl-delta"}, {"object", "chat.completion.chunk"}, {"choices", json::array({choice})}};
}

static json sse_tool_calls_chunk(const json& tool_calls) {
    json delta{{"tool_calls", tool_calls}};
    json choice{{"index", 0}, {"delta", delta}, {"finish_reason", nullptr}};
    return json{{"id", "chatcmpl-delta"}, {"object", "chat.completion.chunk"}, {"choices", json::array({choice})}};
}

static json sse_finish_chunk() {
    json choice{{"index", 0}, {"delta", json::object()}, {"finish_reason", "stop"}};
    return json{{"id", "chatcmpl-delta"}, {"object", "chat.completion.chunk"}, {"choices", json::array({choice})}};
}

// Forward declaration for model switch callback
static ModelSwitchCallback* g_model_switch_callback = nullptr;
// Callback for unloading model / stopping llama-server
static ModelUnloadCallback* g_model_unload_callback = nullptr;

// Last known model path/alias (set when /api/models/use is called) for /api/props fallback
static std::string g_props_fallback_model_path;
static std::string g_props_fallback_model_alias;
static std::mutex g_props_fallback_mutex;

// Progress tracking structure
struct DownloadProgress {
  public:
    std::atomic<double> progress{0.0};
    std::atomic<long long> current_bytes{0};
    std::atomic<long long> total_bytes{0};
    std::atomic<bool> completed{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> cancelled{false};
    std::atomic<int> last_logged_step{-1};

    void set_error(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);
        error_message = message;
    }

    std::string get_error() {
        std::lock_guard<std::mutex> lock(mutex);
        return error_message;
    }

  private:
    std::string error_message;
    std::mutex mutex;
};

static std::mutex g_console_mutex;
static constexpr int LOG_STEP_PERCENT = 5;
static std::atomic<int> g_active_downloads{0};

struct ActiveDownloadCount {
    ActiveDownloadCount() { g_active_downloads.fetch_add(1); }
    ~ActiveDownloadCount() { g_active_downloads.fetch_sub(1); }
    ActiveDownloadCount(const ActiveDownloadCount&) = delete;
    ActiveDownloadCount& operator=(const ActiveDownloadCount&) = delete;
};

static void log_download_line(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_console_mutex);
    std::cout << line << std::endl;
}

static std::map<std::string, std::shared_ptr<DownloadProgress>> g_download_progress;
static std::mutex g_progress_mutex;
thread_local std::shared_ptr<DownloadProgress> g_current_progress = nullptr;
thread_local std::string g_current_model_name;

class ModelAPIServer {
  private:
    int port_;
    std::string webui_path_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    ModelManager model_mgr_;

    void write_props_fallback(httplib::Response& res) {
        std::string model_path;
        std::string model_alias;
        {
            std::lock_guard<std::mutex> lock(g_props_fallback_mutex);
            model_path = g_props_fallback_model_path;
            model_alias = g_props_fallback_model_alias;
        }
        json params = {{"n_predict", -1},
                       {"seed", -1},
                       {"temperature", 0.8},
                       {"dynatemp_range", 0.0},
                       {"dynatemp_exponent", 1.0},
                       {"top_k", 40},
                       {"top_p", 0.95},
                       {"min_p", 0.05},
                       {"top_n_sigma", 0.0},
                       {"xtc_probability", 0.0},
                       {"xtc_threshold", 0.0},
                       {"typ_p", 1.0},
                       {"repeat_last_n", 64},
                       {"repeat_penalty", 1.1},
                       {"presence_penalty", 0.0},
                       {"frequency_penalty", 0.0},
                       {"dry_multiplier", 1.0},
                       {"dry_base", 1.0},
                       {"dry_allowed_length", 0},
                       {"dry_penalty_last_n", 0},
                       {"dry_sequence_breakers", json::array()},
                       {"mirostat", 0},
                       {"mirostat_tau", 5.0},
                       {"mirostat_eta", 0.1},
                       {"stop", json::array()},
                       {"max_tokens", 512},
                       {"n_keep", 0},
                       {"n_discard", 0},
                       {"ignore_eos", false},
                       {"stream", true},
                       {"logit_bias", json::array()},
                       {"n_probs", 0},
                       {"min_keep", 0},
                       {"grammar", ""},
                       {"grammar_lazy", false},
                       {"grammar_triggers", json::array()},
                       {"preserved_tokens", json::array()},
                       {"chat_format", ""},
                       {"reasoning_format", ""},
                       {"reasoning_in_content", false},
                       {"thinking_forced_open", false},
                       {"samplers", json::array()},
                       {"speculative.n_max", 0},
                       {"speculative.n_min", 0},
                       {"speculative.p_min", 0.0},
                       {"timings_per_token", false},
                       {"post_sampling_probs", false},
                       {"lora", json::array()}};
        json default_gen = {{"id", 0},
                            {"id_task", 0},
                            {"n_ctx", 0},
                            {"speculative", false},
                            {"is_processing", false},
                            {"params", params},
                            {"prompt", ""},
                            {"next_token",
                             {{"has_next_token", false},
                              {"has_new_line", false},
                              {"n_remain", 0},
                              {"n_decoded", 0},
                              {"stopping_word", ""}}}};
        json fallback = {{"default_generation_settings", default_gen},
                         {"total_slots", 1},
                         {"model_path", model_path},
                         {"model_alias", model_alias},
                         {"modalities", {{"vision", false}, {"audio", false}}},
                         {"chat_template", ""},
                         {"bos_token", ""},
                         {"eos_token", ""},
                         {"build_info", "delta-cli"}};
        res.set_content(fallback.dump(), "application/json");
    }

    void setup_routes() {
        server_->set_default_headers({{"Access-Control-Allow-Origin", "*"},
                                      {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
                                      {"Access-Control-Allow-Headers", "Content-Type, Authorization"}});

        server_->Options(".*", [](const httplib::Request&, httplib::Response&) { return; });

        server_->Get("/props", [this](const httplib::Request&, httplib::Response& res) { write_props_fallback(res); });

        server_->Get("/api/props", [this](const httplib::Request&, httplib::Response& res) {
            try {
                int llama_port = port_ - 1;
                httplib::Client cli("127.0.0.1", llama_port);
                cli.set_connection_timeout(2, 0);
                cli.set_read_timeout(2, 0);
                auto proxy_res = cli.Get("/props");
                if (proxy_res && proxy_res->status == 200) {
                    res.set_content(proxy_res->body, "application/json");
                    return;
                }
            } catch (...) {
            }
            write_props_fallback(res);
        });

        server_->Get("/api/models/available", [this](const httplib::Request&, httplib::Response& res) {
            try {
                auto models = model_mgr_.get_friendly_model_list(true);
                json models_array = json::array();

                for (const auto& model : models) {
                    json model_json = {{"name", model.name},
                                       {"display_name", model.display_name},
                                       {"description", model.description},
                                       {"size_str", model.size_str},
                                       {"quantization", model.quantization},
                                       {"size_bytes", model.size_bytes},
                                       {"installed", model.installed},
                                       {"supports_tools", model.supports_tools}};
                    models_array.push_back(model_json);
                }

                json result = {{"models", models_array}};
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Get("/api/models/list", [this](const httplib::Request&, httplib::Response& res) {
            try {
                auto models = model_mgr_.get_friendly_model_list(false);
                json models_array = json::array();

                for (const auto& model : models) {
                    json model_json = {{"name", model.name},
                                       {"display_name", model.display_name},
                                       {"description", model.description},
                                       {"size_str", model.size_str},
                                       {"quantization", model.quantization},
                                       {"size_bytes", model.size_bytes},
                                       {"supports_tools", model.supports_tools}};
                    models_array.push_back(model_json);
                }

                json result = {{"models", models_array}};
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Get(R"(/api/models/download/progress/(.+))", [](const httplib::Request& req, httplib::Response& res) {
            try {
                std::string model_name = req.matches[1];

                std::lock_guard<std::mutex> lock(g_progress_mutex);
                auto it = g_download_progress.find(model_name);

                if (it == g_download_progress.end()) {
                    json result = {{"progress", 0.0},
                                   {"current_bytes", 0},
                                   {"total_bytes", 0},
                                   {"completed", false},
                                   {"failed", false}};
                    res.set_content(result.dump(), "application/json");
                    return;
                }

                auto& prog = it->second;
                json result = {{"progress", prog->progress.load()},       {"current_bytes", prog->current_bytes.load()},
                               {"total_bytes", prog->total_bytes.load()}, {"completed", prog->completed.load()},
                               {"failed", prog->failed.load()},           {"cancelled", prog->cancelled.load()}};

                if (prog->failed.load()) {
                    result["error_message"] = prog->get_error();
                }

                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Get("/api/models/downloads", [](const httplib::Request&, httplib::Response& res) {
            try {
                json downloads = json::array();
                {
                    std::lock_guard<std::mutex> lock(g_progress_mutex);
                    for (const auto& entry : g_download_progress) {
                        const auto& prog = entry.second;
                        if (prog->completed.load() || prog->failed.load()) {
                            continue;
                        }
                        downloads.push_back({{"model", entry.first},
                                             {"progress", prog->progress.load()},
                                             {"current_bytes", prog->current_bytes.load()},
                                             {"total_bytes", prog->total_bytes.load()},
                                             {"completed", false},
                                             {"failed", false}});
                    }
                }

                json result = {{"downloads", downloads}};
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Post("/api/models/download", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                std::string model_name = body.value("model", "");

                if (model_name.empty()) {
                    json error = {{"error", {{"code", 400}, {"message", "Model name is required"}}}};
                    res.status = 400;
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(g_progress_mutex);
                    auto it = g_download_progress.find(model_name);
                    if (it != g_download_progress.end() && !it->second->completed.load() &&
                        !it->second->failed.load()) {
                        json error = {{"error", {{"code", 409}, {"message", "Download already in progress"}}}};
                        res.status = 409;
                        res.set_content(error.dump(), "application/json");
                        return;
                    }
                }

                auto progress = std::make_shared<DownloadProgress>();
                {
                    std::lock_guard<std::mutex> lock(g_progress_mutex);
                    g_download_progress[model_name] = progress;
                }

                std::thread download_thread([this, model_name, progress]() {
                    ActiveDownloadCount active_downloads;
                    (void)active_downloads;
                    try {
#ifdef _WIN32
                        SetConsoleOutputCP(65001);
                        SetConsoleCP(65001);
#endif
                        g_current_progress = progress;
                        g_current_model_name = model_name;

                        static auto progress_cb = [](double prog, long long current, long long total) {
                            if (!g_current_progress)
                                return;

                            g_current_progress->progress.store(prog);
                            g_current_progress->current_bytes.store(current);
                            g_current_progress->total_bytes.store(total);

                            const double current_mb = current / (1024.0 * 1024.0);
                            const double total_mb = total / (1024.0 * 1024.0);

                            if (g_active_downloads.load() > 1) {
                                const int step = (int)(prog / LOG_STEP_PERCENT);
                                int previous = g_current_progress->last_logged_step.load();
                                if (step <= previous)
                                    return;
                                if (!g_current_progress->last_logged_step.compare_exchange_strong(previous, step)) {
                                    return;
                                }

                                std::ostringstream line;
                                line << "[Download " << g_current_model_name << "] " << std::fixed
                                     << std::setprecision(1) << prog << "% (" << current_mb << " / " << total_mb
                                     << " MB)";
                                log_download_line(line.str());
                                return;
                            }

                            const int bar_width = 50;
                            const int pos = (int)(prog / 100.0 * bar_width);

                            std::ostringstream bar;
                            bar << "\r[Download " << g_current_model_name << "] [";
#ifdef _WIN32
                            for (int i = 0; i < bar_width; i++) {
                                if (i < pos)
                                    bar << "#";
                                else if (i == pos)
                                    bar << ">";
                                else
                                    bar << "-";
                            }
#else
                            for (int i = 0; i < bar_width; i++) {
                                if (i < pos) bar << "█";
                                else if (i == pos) bar << "▓";
                                else bar << "░";
                            }
#endif
                            bar << "] " << std::fixed << std::setprecision(1) << prog << "% ";
                            bar << "(" << std::fixed << std::setprecision(1) << current_mb << " / ";
                            bar << total_mb << " MB)";

                            std::lock_guard<std::mutex> lock(g_console_mutex);
                            std::cout << bar.str() << std::flush;
                        };

                        model_mgr_.set_progress_callback(progress_cb);

                        bool success = model_mgr_.pull_model(model_name);

                        model_mgr_.set_progress_callback(nullptr);
                        g_current_progress = nullptr;

                        if (success) {
                            progress->completed.store(true);
                            progress->progress.store(100.0);
                            std::cout << std::endl;
#ifdef _WIN32
                            std::cout << "[Download " << model_name << "] [OK] Download completed successfully!"
                                      << std::endl;
#else
                            std::cout << "[Download " << model_name << "] ✓ Download completed successfully!" << std::endl;
#endif
                        } else {
                            const bool was_cancelled = progress->cancelled.load();
                            const std::string reason = was_cancelled ? "Download cancelled" : "Download failed";
                            progress->failed.store(true);
                            progress->set_error(reason);
                            log_download_line("[Download " + model_name + "] " + reason);
                        }
                    } catch (const std::exception& e) {
                        progress->failed.store(true);
                        progress->set_error(e.what());
                        model_mgr_.set_progress_callback(nullptr);
                        g_current_progress = nullptr;
                        std::cout << std::endl;
                        std::cout << "[Download " << model_name << "] Error: " << e.what() << std::endl;
                    }
                });
                download_thread.detach();

                json result = {{"success", true}, {"message", "Download started"}, {"model", model_name}};
                res.set_content(result.dump(), "application/json");
            } catch (const json::parse_error& e) {
                json error = {{"error", {{"code", 400}, {"message", "Invalid JSON in request body"}}}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Post("/api/models/download/cancel", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                std::string model_name = body.value("model", "");

                if (model_name.empty()) {
                    json error = {{"error", {{"code", 400}, {"message", "Model name is required"}}}};
                    res.status = 400;
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                model_mgr_.cancel_download(model_name);

                {
                    std::lock_guard<std::mutex> lock(g_progress_mutex);
                    auto it = g_download_progress.find(model_name);
                    if (it != g_download_progress.end() && !it->second->completed.load()) {
                        it->second->cancelled.store(true);
                        it->second->failed.store(true);
                        it->second->set_error("Download cancelled");
                    }
                }

                json result = {{"success", true}, {"message", "Download cancellation requested"}};
                res.set_content(result.dump(), "application/json");
            } catch (const json::parse_error&) {
                json error = {{"error", {{"code", 400}, {"message", "Invalid JSON in request body"}}}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Delete(R"(/api/models/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                std::string model_name = req.matches[1];

                if (model_name.empty()) {
                    json error = {{"error", {{"code", 400}, {"message", "Model name is required"}}}};
                    res.status = 400;
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                bool success = model_mgr_.remove_model(model_name);

                if (success) {
                    json result = {{"success", true}, {"message", "Model removed successfully"}};
                    res.set_content(result.dump(), "application/json");
                } else {
                    json error = {{"error", {{"code", 500}, {"message", "Failed to remove model"}}}};
                    res.status = 500;
                    res.set_content(error.dump(), "application/json");
                }
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Post("/api/models/use", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                std::string model_name = body.value("model", "");
                int ctx_override = body.value("ctx_size", 0);

                if (model_name.empty()) {
                    json error = {{"error", {{"code", 400}, {"message", "Model name is required"}}}};
                    res.status = 400;
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                if (!model_mgr_.is_model_installed(model_name)) {
                    json error = {{"error", {{"code", 404}, {"message", "Model not found"}}}};
                    res.status = 404;
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                std::string model_path = model_mgr_.get_model_path(model_name);
                if (model_path.empty()) {
                    json error = {{"error", {{"code", 500}, {"message", "Could not get model path"}}}};
                    res.status = 500;
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                if (ctx_override > 0) {
                    model_mgr_.set_max_context_override(model_name, ctx_override);
                }
                int ctx_size = model_mgr_.get_max_context_for_model(model_name);
                std::string model_alias = model_name;
                {
                    std::filesystem::path mp(model_path);
                    std::string stem = mp.stem().string();
                    if (!stem.empty()) {
                        model_alias = stem;
                    }
                }

                bool model_loaded = false;
                {
                    std::lock_guard<std::mutex> lock(g_props_fallback_mutex);
                    g_props_fallback_model_path = model_path;
                    g_props_fallback_model_alias = model_alias;
                }

                if (g_model_switch_callback) {
                    bool likely_ui_only = (port_ == 8080);

                    if (likely_ui_only) {
                        std::thread migration_thread([model_path, model_name, ctx_size, model_alias]() {
                            try {
#ifdef _WIN32
                                SetConsoleOutputCP(65001);
                                SetConsoleCP(65001);
#endif
                                std::this_thread::sleep_for(std::chrono::milliseconds(300));

                                stop_model_api_server();

                                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                                if (g_model_switch_callback) {
                                    (*g_model_switch_callback)(model_path, model_name, ctx_size, model_alias);
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "[ERROR] Error in migration thread: " << e.what() << std::endl;
                            }
                        });
                        migration_thread.detach();

                        model_loaded = false;
                    } else {
                        try {
                            model_loaded = (*g_model_switch_callback)(model_path, model_name, ctx_size, model_alias);
                        } catch (const std::exception& e) {
                            std::cerr << "[ERROR] Error switching model: " << e.what() << std::endl;
                        }
                    }
                }

                json result = {
                    {"success", true},
                    {"model_path", model_path},
                    {"model_name", model_name},
                    {"model_alias", model_alias},
                    {"ctx_size", ctx_size},
                    {"loaded", model_loaded},
                    {"message", model_loaded
                                    ? "Model loaded successfully! The server is now using " + model_alias + "."
                                    : (model_loaded == false && port_ == 8080
                                           ? "Model migration in progress. The server is switching to full mode. This "
                                             "may take a few seconds."
                                           : "Model selected. The model path will be sent in API requests. Note: "
                                             "llama-server uses the model loaded at startup. To actually use this "
                                             "model, restart the server with: ./delta-server -m \"" +
                                                 model_path + "\" --port 8080")}};

                res.set_content(result.dump(), "application/json");
            } catch (const json::parse_error& e) {
                json error = {{"error", {{"code", 400}, {"message", "Invalid JSON in request body"}}}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Post("/v1/chat/completions", [this](const httplib::Request& req, httplib::Response& res) {
            int llama_port = port_ - 1;
            {
                httplib::Client probe("127.0.0.1", llama_port);
                probe.set_connection_timeout(1, 0);
                auto check = probe.Get("/props");
                if (!check || check->status != 200) {
                    json err = {{"error",
                                 {{"message", "No model loaded. Please select a model from the dropdown first."},
                                  {"type", "server_error"},
                                  {"code", "no_model_loaded"}}}};
                    res.status = 503;
                    res.set_content(err.dump(), "application/json");
                    return;
                }
            }

            try {
                json body = json::parse(req.body);
                if (!body.is_object()) {
                    json err = {
                        {"error",
                         {{"message", "Request body must be a JSON object, got: " + std::string(body.type_name())},
                          {"type", "invalid_request_error"}}}};
                    res.status = 400;
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                json messages = body.contains("messages") ? body["messages"] : json::array();
                if (!messages.is_array()) {
                    json err = {{"error",
                                 {{"message", "messages must be an array, got: " + std::string(messages.type_name())},
                                  {"type", "invalid_request_error"}}}};
                    res.status = 400;
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                for (size_t i = 0; i < messages.size(); i++) {
                    if (!messages[i].is_object()) {
                        json err = {
                            {"error",
                             {{"message", "messages[" + std::to_string(i) + "] must be an object, got: " +
                                              std::string(messages[i].type_name()) + " = " + messages[i].dump()},
                              {"type", "invalid_request_error"}}}};
                        res.status = 400;
                        res.set_content(err.dump(), "application/json");
                        return;
                    }
                }
                bool stream = body.value("stream", false);
                std::string model_name = body.value("model", std::string("default"));

                std::string llama_url = "http://127.0.0.1:" + std::to_string(llama_port);
                bool model_supports_tools = false;
                auto reg = model_mgr_.get_registry_entry(model_name);
                std::string llama_model_name = model_name;
                if (!reg.name.empty()) {
                    model_supports_tools = reg.supports_tools;
                    if (!reg.filename.empty()) {
                        auto dot = reg.filename.rfind('.');
                        llama_model_name = (dot != std::string::npos) ? reg.filename.substr(0, dot) : reg.filename;
                    }
                }
                std::cerr << "[delta-server] agent loop: model=" << model_name << " -> llama_alias=" << llama_model_name
                          << ", supports_tools=" << (model_supports_tools ? "true" : "false")
                          << ", msgs=" << messages.size() << std::endl;

                if (stream) {
                    struct StreamJob {
                        std::string llama_url, llama_model;
                        bool supports_tools = false;
                        json messages;
                        bool started = false;
                    };
                    auto job = std::make_shared<StreamJob>();
                    job->llama_url = llama_url;
                    job->llama_model = llama_model_name;
                    job->supports_tools = model_supports_tools;
                    job->messages = messages;

                    res.set_header("Cache-Control", "no-cache");
                    res.set_header("Connection", "keep-alive");
                    res.set_header("X-Accel-Buffering", "no");
                    res.set_chunked_content_provider(
                        "text/event-stream", [job](size_t, httplib::DataSink& sink) -> bool {
                            if (job->started) {
                                sink.done();
                                return true;
                            }
                            job->started = true;

                            auto emit = [&sink](const json& chunk) -> bool {
                                const std::string frame = "data: " + chunk.dump() + "\n\n";
                                return sink.write(frame.data(), frame.size());
                            };

                            try {
                                agent::AgentLoop loop(job->llama_url, job->llama_model, job->supports_tools);
                                auto result = loop.process(job->messages, [&](const std::string& delta) -> bool {
                                    if (delta.empty())
                                        return true;
                                    if (!sink.is_writable())
                                        return false;
                                    return emit(sse_content_chunk(delta));
                                });

                                if (result.client_aborted) {
                                    sink.done();
                                    return true;
                                }

                                if (!result.success) {
                                    std::cerr << "[delta-server] stream error: " << result.error << std::endl;
                                    emit(sse_content_chunk(result.error));
                                } else if (result.streamed_chars == 0) {
                                    std::istringstream iss(result.content);
                                    std::string line;
                                    bool first_line = true;
                                    while (std::getline(iss, line)) {
                                        emit(sse_content_chunk(first_line ? line : "\n" + line));
                                        first_line = false;
                                    }
                                }
                                if (!result.tool_calls.empty())
                                    emit(sse_tool_calls_chunk(result.tool_calls));
                            } catch (const std::exception& e) {
                                std::cerr << "[delta-server] exception in stream provider: " << e.what() << std::endl;
                                emit(sse_content_chunk(std::string("Error: ") + e.what()));
                            }

                            emit(sse_finish_chunk());
                            static const std::string done = "data: [DONE]\n\n";
                            sink.write(done.data(), done.size());
                            sink.done();
                            return true;
                        });
                } else {
                    agent::AgentLoop loop(llama_url, llama_model_name, model_supports_tools);
                    auto result = loop.process(messages);

                    if (!result.success) {
                        json err = {{"error", {{"message", result.error}, {"type", "server_error"}}}};
                        res.status = 500;
                        res.set_content(err.dump(), "application/json");
                        return;
                    }

                    json message = {{"role", "assistant"}, {"content", result.content}};
                    if (!result.tool_calls.empty())
                        message["tool_calls"] = result.tool_calls;
                                        json response = {{"id", "chatcmpl-delta"},
                                     {"object", "chat.completion"},
                                     {"choices", {{{"index", 0}, {"message", message}, {"finish_reason", "stop"}}}},
                                     {"usage", {{"prompt_tokens", 0}, {"completion_tokens", 0}, {"total_tokens", 0}}}};
                    if (result.tool_calls_made > 0) {
                        response["tool_calls_made"] = result.tool_calls_made;
                    }
                    res.set_content(response.dump(), "application/json");
                }
            } catch (const json::parse_error&) {
                json err = {
                    {"error", {{"message", "Invalid JSON in request body"}, {"type", "invalid_request_error"}}}};
                res.status = 400;
                res.set_content(err.dump(), "application/json");
            } catch (const nlohmann::json::type_error& e) {
                std::cerr << "[delta-server] JSON type error in agent loop: " << e.what() << std::endl;
                std::cerr << "[delta-server] Request body was: " << req.body.substr(0, 2000) << std::endl;
                json err = {{"error", {{"message", std::string(e.what())}, {"type", "json_type_error"}}}};
                res.status = 500;
                res.set_content(err.dump(), "application/json");
            } catch (const std::exception& e) {
                std::cerr << "[delta-server] Exception in agent loop: " << e.what() << std::endl;
                json err = {{"error", {{"message", e.what()}, {"type", "server_error"}}}};
                res.status = 500;
                res.set_content(err.dump(), "application/json");
            }
        });
        server_->Get("/v1/models", [](const httplib::Request&, httplib::Response& res) {
            json out = {{"object", "list"}, {"data", json::array()}};
            res.set_content(out.dump(), "application/json");
        });

        server_->Post("/api/models/unload", [](const httplib::Request&, httplib::Response& res) {
            try {
                if (g_model_unload_callback) {
                    (*g_model_unload_callback)();
                }
                json result = {{"success", true}, {"message", "Model unloaded and server stopped."}};
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Get("/api/system/ram", [](const httplib::Request&, httplib::Response& res) {
            try {
                long long total_ram_bytes = 0;

#ifdef _WIN32
                MEMORYSTATUSEX memInfo;
                memInfo.dwLength = sizeof(MEMORYSTATUSEX);
                if (GlobalMemoryStatusEx(&memInfo)) {
                    total_ram_bytes = memInfo.ullTotalPhys;
                }
#elif defined(__APPLE__)
                int64_t memsize = 0;
                size_t len = sizeof(memsize);
                int mib[2] = {CTL_HW, HW_MEMSIZE};
                if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) {
                    total_ram_bytes = memsize;
                }
#else
                struct sysinfo info;
                if (sysinfo(&info) == 0) {
                    total_ram_bytes = info.totalram * info.mem_unit;
                }
#endif

                long long total_ram_gb = (total_ram_bytes + (1024LL * 1024 * 1024 - 1)) / (1024LL * 1024 * 1024);

                json result = {{"total_ram_gb", total_ram_gb}, {"total_ram_bytes", total_ram_bytes}};
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });

        server_->Get("/api/agent/tools", [](const httplib::Request&, httplib::Response& res) {
            auto& registry = agent::ToolRegistry::instance();
            json result = {{"tools", registry.get_tools_array()}, {"tool_names", registry.get_tool_names()}};
            res.set_content(result.dump(), "application/json");
        });

        server_->Get("/api/agent/events", [](const httplib::Request& req, httplib::Response& res) {
            auto& db = agent::AgentDatabase::instance();
            std::string start = req.has_param("start") ? req.get_param_value("start") : "";
            std::string end = req.has_param("end") ? req.get_param_value("end") : "";
            int limit = 50;
            if (req.has_param("limit")) {
                try {
                    limit = std::stoi(req.get_param_value("limit"));
                } catch (...) {
                }
            }
            std::string type = req.has_param("type") ? req.get_param_value("type") : "";
            std::string status = req.has_param("status") ? req.get_param_value("status") : "";
            std::string priority = req.has_param("priority") ? req.get_param_value("priority") : "";
            std::string tags = req.has_param("tags") ? req.get_param_value("tags") : "";
            auto events = db.list_events(start, end, limit, type, status, priority, tags);
            json result = {{"events", json::array()}, {"count", events.size()}};
            for (auto& e : events)
                result["events"].push_back(e);
            res.set_content(result.dump(), "application/json");
        });

        server_->Post("/api/agent/events", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto& db = agent::AgentDatabase::instance();
                json body = json::parse(req.body);
                std::string id = db.create_event(body);
                if (id.empty()) {
                    res.status = 400;
                    res.set_content(json({{"error", "Failed to create event"}}).dump(), "application/json");
                    return;
                }
                auto event = db.get_event(id);
                res.set_content(event.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(json({{"error", e.what()}}).dump(), "application/json");
            }
        });

        server_->Put(R"(/api/agent/events/(.+))", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto& db = agent::AgentDatabase::instance();
                std::string id = req.matches[1];
                json body = json::parse(req.body);
                if (!db.update_event(id, body)) {
                    res.status = 404;
                    res.set_content(json({{"error", "Event not found"}}).dump(), "application/json");
                    return;
                }
                auto event = db.get_event(id);
                res.set_content(event.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(json({{"error", e.what()}}).dump(), "application/json");
            }
        });

        server_->Delete(R"(/api/agent/events/(.+))", [](const httplib::Request& req, httplib::Response& res) {
            auto& db = agent::AgentDatabase::instance();
            std::string id = req.matches[1];
            if (!db.delete_event(id)) {
                res.status = 404;
                res.set_content(json({{"error", "Event not found"}}).dump(), "application/json");
                return;
            }
            res.set_content(json({{"deleted", true}}).dump(), "application/json");
        });

        server_->Get("/api/agent/reminders/pending", [](const httplib::Request&, httplib::Response& res) {
            auto& db = agent::AgentDatabase::instance();
            auto reminders = db.get_upcoming_reminders();
            for (auto& r : reminders) {
                db.mark_reminded(r.value("id", ""));
            }
            json result = {{"reminders", reminders}, {"count", reminders.size()}};
            res.set_content(result.dump(), "application/json");
        });

        server_->Get("/api/notes", [](const httplib::Request& req, httplib::Response& res) {
            auto& db = agent::AgentDatabase::instance();
            std::string search = req.has_param("search") ? req.get_param_value("search") : "";
            std::string folder = req.has_param("folder") ? req.get_param_value("folder") : "";
            std::string tags   = req.has_param("tags")   ? req.get_param_value("tags")   : "";
            int limit = 50;
            if (req.has_param("limit")) {
                try { limit = std::stoi(req.get_param_value("limit")); } catch (...) {}
            }
            bool pinned_only = req.has_param("pinned_only") && req.get_param_value("pinned_only") == "true";

            auto notes = db.list_notes(folder, search, tags, limit, pinned_only);
            json result = {{"notes", json::array()}, {"count", notes.size()}};
            for (auto& n : notes) result["notes"].push_back(n);
            res.set_content(result.dump(), "application/json");
        });

        server_->Post("/api/notes", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto& db = agent::AgentDatabase::instance();
                json body = json::parse(req.body);
                if (!body.contains("title") || !body.contains("content")) {
                    res.status = 400;
                    res.set_content(json({{"error", "title and content are required"}}).dump(), "application/json");
                    return;
                }
                std::string id = db.create_note(body);
                if (id.empty()) {
                    res.status = 500;
                    res.set_content(json({{"error", "Failed to create note"}}).dump(), "application/json");
                    return;
                }
                res.set_content(db.get_note(id).dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(json({{"error", e.what()}}).dump(), "application/json");
            }
        });

        server_->Get(R"(/api/notes/(.+))", [](const httplib::Request& req, httplib::Response& res) {
            auto& db = agent::AgentDatabase::instance();
            std::string id = req.matches[1];
            auto note = db.get_note(id);
            if (note.is_null()) {
                res.status = 404;
                res.set_content(json({{"error", "Note not found"}}).dump(), "application/json");
                return;
            }
            res.set_content(note.dump(), "application/json");
        });

        server_->Put(R"(/api/notes/(.+))", [](const httplib::Request& req, httplib::Response& res) {
            try {
                auto& db = agent::AgentDatabase::instance();
                std::string id = req.matches[1];
                json body = json::parse(req.body);
                if (!db.update_note(id, body)) {
                    res.status = 404;
                    res.set_content(json({{"error", "Note not found"}}).dump(), "application/json");
                    return;
                }
                res.set_content(db.get_note(id).dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(json({{"error", e.what()}}).dump(), "application/json");
            }
        });

        server_->Delete(R"(/api/notes/(.+))", [](const httplib::Request& req, httplib::Response& res) {
            auto& db = agent::AgentDatabase::instance();
            std::string id = req.matches[1];
            if (!db.delete_note(id)) {
                res.status = 404;
                res.set_content(json({{"error", "Note not found"}}).dump(), "application/json");
                return;
            }
            res.set_content(json({{"deleted", true}}).dump(), "application/json");
        });

        // =====================================================================
        // DHATS: Hardware Telemetry SSE Stream
        // =====================================================================
        server_->Get("/api/v1/hardware/stream", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("X-Accel-Buffering", "no");
            // res.set_header("Access-Control-Allow-Origin", "*");

            res.set_chunked_content_provider(
                "text/event-stream",
                [](size_t, httplib::DataSink& sink) -> bool {
                    auto metrics = g_hardware_monitor.get_metrics();

                    json gpus_array = json::array();
                    for (const auto& gpu : metrics.gpus) {
                        gpus_array.push_back({
                            {"name", gpu.name},
                            {"backend", gpu.backend},
                            {"vram_used_gb", gpu.vram_used_gb},
                            {"vram_total_gb", gpu.vram_total_gb},
                            {"gpu_util_pct", gpu.gpu_util_pct},
                            {"temp_c", gpu.temp_c},
                            {"power_w", gpu.power_w}
                        });
                    }

                    json payload = {
                        {"system_ram_used_gb", metrics.system_ram_used_gb},
                        {"system_ram_total_gb", metrics.system_ram_total_gb},
                        {"gpus", gpus_array},
                        {"rpc_node_count", metrics.rpc_node_count},
                        {"timestamp_ms", metrics.timestamp_ms},
                        {"has_gpu", g_hardware_monitor.has_gpu()},
                        {"primary_backend", g_hardware_monitor.get_primary_backend()}
                    };

                    std::string frame = "data: " + payload.dump() + "\n\n";
                    sink.write(frame.c_str(), frame.size());

                    // Poll interval: 500ms
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    return true;
                }
            );
        });

        // DHATS: One-shot snapshot (for polling clients that don't use SSE)
        server_->Get("/api/v1/hardware/snapshot", [](const httplib::Request&, httplib::Response&) {
            auto metrics = g_hardware_monitor.get_metrics();

            json gpus_array = json::array();
            for (const auto& gpu : metrics.gpus) {
                gpus_array.push_back({
                    {"name", gpu.name},
                    {"backend", gpu.backend},
                    {"vram_used_gb", gpu.vram_used_gb},
                    {"vram_total_gb", gpu.vram_total_gb},
                    {"gpu_util_pct", gpu.gpu_util_pct},
                    {"temp_c", gpu.temp_c},
                    {"power_w", gpu.power_w}
                });
            }

            json result = {
                {"system_ram_used_gb", metrics.system_ram_used_gb},
                {"system_ram_total_gb", metrics.system_ram_total_gb},
                {"gpus", gpus_array},
                {"has_gpu", g_hardware_monitor.has_gpu()},
                {"primary_backend", g_hardware_monitor.get_primary_backend()}
            };

            // res.set_content(result.dump(), "application/json");
        });

        // DHATS: Auto-offload calculation endpoint
        server_->Post("/api/v1/hardware/auto-ngl", [](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                long long model_size = body.value("model_size_bytes", 0LL);
                int n_layers = body.value("n_layers", 32);
                int ctx_size = body.value("ctx_size", 4096);

                int ngl = g_hardware_monitor.calculate_auto_ngl(model_size, n_layers, ctx_size);

                json result = {
                    {"recommended_ngl", ngl},
                    {"has_gpu", g_hardware_monitor.has_gpu()},
                    {"primary_backend", g_hardware_monitor.get_primary_backend()},
                    {"model_size_gb", (double)model_size / (1024.0 * 1024.0 * 1024.0)}
                };
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 400}, {"message", e.what()}}}};
                res.status = 400;
                res.set_content(error.dump(), "application/json");
            }
        });

                // ====================================================================
        // DHATS: Real-time hardware telemetry (SSE stream + one-shot snapshot)
        // ====================================================================
        server_->Get("/api/v1/hardware/stream", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection", "keep-alive");
            res.set_header("X-Accel-Buffering", "no");
            res.set_header("Access-Control-Allow-Origin", "*");

            res.set_chunked_content_provider(
                "text/event-stream",
                [](size_t, httplib::DataSink& sink) -> bool {
                    auto m = g_hardware_monitor.get_metrics();

                    json gpus = json::array();
                    for (const auto& g : m.gpus) {
                        gpus.push_back({{"name", g.name},
                                        {"backend", g.backend},
                                        {"vram_used_gb", g.vram_used_gb},
                                        {"vram_total_gb", g.vram_total_gb},
                                        {"gpu_util_pct", g.gpu_util_pct},
                                        {"temp_c", g.temp_c},
                                        {"power_w", g.power_w}});
                    }

                    json payload = {{"system_ram_used_gb", m.system_ram_used_gb},
                                    {"system_ram_total_gb", m.system_ram_total_gb},
                                    {"cpu_util_pct", m.cpu_util_pct},
                                    {"gpus", gpus},
                                    {"rpc_node_count", m.rpc_node_count},
                                    {"timestamp_ms", m.timestamp_ms},
                                    {"has_gpu", g_hardware_monitor.has_gpu()},
                                    {"primary_backend", g_hardware_monitor.get_primary_backend()}};

                    std::string frame = "data: " + payload.dump() + "\n\n";
                    sink.write(frame.c_str(), frame.size());
                    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 2 Hz telemetry
                    return true; // keep the stream open
                });
        });

        server_->Get("/api/v1/hardware/snapshot", [](const httplib::Request&, httplib::Response& res) {
            auto m = g_hardware_monitor.get_metrics();
            json gpus = json::array();
            for (const auto& g : m.gpus) {
                gpus.push_back({{"name", g.name},
                                {"backend", g.backend},
                                {"vram_used_gb", g.vram_used_gb},
                                {"vram_total_gb", g.vram_total_gb},
                                {"gpu_util_pct", g.gpu_util_pct},
                                {"temp_c", g.temp_c},
                                {"power_w", g.power_w}});
            }
            json payload = {{"system_ram_used_gb", m.system_ram_used_gb},
                            {"system_ram_total_gb", m.system_ram_total_gb},
                            {"cpu_util_pct", m.cpu_util_pct},
                            {"gpus", gpus},
                            {"has_gpu", g_hardware_monitor.has_gpu()},
                            {"primary_backend", g_hardware_monitor.get_primary_backend()}};
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content(payload.dump(), "application/json");
        });

        if (!webui_path_.empty() && tools::FileOps::dir_exists(webui_path_)) {
            server_->set_mount_point("/", webui_path_);
        }
    }

    void server_loop() {
        if (!server_->bind_to_port("127.0.0.1", port_)) {
            std::cerr << "Failed to bind model API server to port " << port_ << std::endl;
            return;
        }

        server_->listen_after_bind();
    }

  public:
    ModelAPIServer(int port = 8081, const std::string& webui_path = "")
        : port_(port), webui_path_(webui_path), running_(false) {
        server_ = std::make_unique<httplib::Server>();
        if (!agent::AgentDatabase::instance().init()) {
            std::cerr << "[WARNING] Failed to initialize agent database — tools will not persist data" << std::endl;
        }
        agent::register_all_tools();
        setup_routes();
    }

    void start() {
        running_ = true;
        server_thread_ = std::thread(&ModelAPIServer::server_loop, this);
    }

    void stop() {
        running_ = false;
        if (server_) {
            server_->stop();
        }
        if (server_thread_.joinable()) {
            if (server_thread_.get_id() == std::this_thread::get_id()) {
                server_thread_.detach();
            } else {
                auto future = std::async(std::launch::async, [this]() {
                    if (server_thread_.joinable()) {
                        server_thread_.join();
                    }
                });
                if (future.wait_for(std::chrono::seconds(2)) == std::future_status::timeout) {
                    server_thread_.detach();
                }
            }
        }
    }

    ~ModelAPIServer() { stop(); }
};

static std::unique_ptr<ModelAPIServer> g_model_api_server;

void set_model_switch_callback(ModelSwitchCallback callback) {
    static ModelSwitchCallback stored_callback = callback;
    g_model_switch_callback = &stored_callback;
}

void set_model_unload_callback(ModelUnloadCallback callback) {
    static ModelUnloadCallback stored_callback = callback;
    g_model_unload_callback = &stored_callback;
}

void start_model_api_server(int port) {
    if (!g_model_api_server) {
        g_model_api_server = std::make_unique<ModelAPIServer>(port, "");
        g_model_api_server->start();
    }
}

void start_model_api_server(int port, const std::string& webui_path) {
    stop_model_api_server();
    g_model_api_server = std::make_unique<ModelAPIServer>(port, webui_path);
    g_model_api_server->start();
}

void stop_model_api_server() {
    if (g_model_api_server) {
        g_model_api_server->stop();
        g_model_api_server.reset();
    }
}

// delta::HardwareMonitor g_hw_monitor;

// void setup_hardware_routes(httplib::Server& svr) {
//     svr.Get("/api/v1/hardware/stream", [](const httplib::Request& req, httplib::Response& res) {
//         res.set_header("Content-Type", "text/event-stream");
//         res.set_header("Cache-Control", "no-cache");
//         res.set_header("Connection", "keep-alive");
        
//         res.set_chunked_content_provider(
//             "text/event-stream",
//             [](size_t offset, httplib::DataSink& sink) {
//                 // 1. Gather metrics (NVML for NVIDIA, Metal for Mac, ROCm for AMD)
//                 nlohmann::json telemetry = {
//                     {"system_ram_used_gb", get_system_ram_used()},
//                     {"gpus", get_gpu_metrics_array()}
//                 };
                
//                 // 2. Format as SSE
//                 std::string data = "data: " + telemetry.dump() + "\n\n";
//                 sink.write(data.c_str(), data.size());
                
//                 // 3. Throttle stream to 1Hz
//                 std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//                 return true; // Keep connection alive
//             }
//         );
//     });
// }

} // namespace delta