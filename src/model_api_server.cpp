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
#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>
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

using json = nlohmann::json;

namespace delta {

// Forward declaration for model switch callback
static ModelSwitchCallback* g_model_switch_callback = nullptr;

// Progress tracking structure
struct DownloadProgress {
    std::atomic<double> progress{0.0};
    std::atomic<long long> current_bytes{0};
    std::atomic<long long> total_bytes{0};
    std::atomic<bool> completed{false};
    std::atomic<bool> failed{false};
    std::string error_message;
    std::mutex mutex;
};

// Global progress map (model_name -> progress)
static std::map<std::string, std::shared_ptr<DownloadProgress>> g_download_progress;
static std::mutex g_progress_mutex;
// Thread-local storage for current download progress
thread_local std::shared_ptr<DownloadProgress> g_current_progress = nullptr;
thread_local std::string g_current_model_name;

class ModelAPIServer {
private:
    int port_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    ModelManager model_mgr_;
    
    void setup_routes() {
        // CORS headers
        server_->set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"}
        });
        
        // Handle OPTIONS (CORS preflight)
        server_->Options(".*", [](const httplib::Request&, httplib::Response&) {
            return;
        });
        
        // GET /api/models/available - List all available models
        server_->Get("/api/models/available", [this](const httplib::Request&, httplib::Response& res) {
            try {
                auto models = model_mgr_.get_friendly_model_list(true);
                json models_array = json::array();
                
                for (const auto& model : models) {
                    json model_json = {
                        {"name", model.name},
                        {"display_name", model.display_name},
                        {"description", model.description},
                        {"size_str", model.size_str},
                        {"quantization", model.quantization},
                        {"size_bytes", model.size_bytes},
                        {"installed", model.installed}
                    };
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
        
        // GET /api/models/list - List installed models
        server_->Get("/api/models/list", [this](const httplib::Request&, httplib::Response& res) {
            try {
                auto models = model_mgr_.get_friendly_model_list(false);
                json models_array = json::array();
                
                for (const auto& model : models) {
                    json model_json = {
                        {"name", model.name},
                        {"display_name", model.display_name},
                        {"description", model.description},
                        {"size_str", model.size_str},
                        {"quantization", model.quantization},
                        {"size_bytes", model.size_bytes}
                    };
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
        
        // GET /api/models/download/progress/:model - Get download progress
        server_->Get(R"(/api/models/download/progress/(.+))", [](const httplib::Request& req, httplib::Response& res) {
            try {
                std::string model_name = req.matches[1];
                
                std::lock_guard<std::mutex> lock(g_progress_mutex);
                auto it = g_download_progress.find(model_name);
                
                if (it == g_download_progress.end()) {
                    json result = {
                        {"progress", 0.0},
                        {"current_bytes", 0},
                        {"total_bytes", 0},
                        {"completed", false},
                        {"failed", false}
                    };
                    res.set_content(result.dump(), "application/json");
                    return;
                }
                
                auto& prog = it->second;
                json result = {
                    {"progress", prog->progress.load()},
                    {"current_bytes", prog->current_bytes.load()},
                    {"total_bytes", prog->total_bytes.load()},
                    {"completed", prog->completed.load()},
                    {"failed", prog->failed.load()}
                };
                
                if (prog->failed.load()) {
                    result["error_message"] = prog->error_message;
                }
                
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });
        
        // POST /api/models/download - Download a model (async)
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
                
                // Check if download is already in progress
                {
                    std::lock_guard<std::mutex> lock(g_progress_mutex);
                    auto it = g_download_progress.find(model_name);
                    if (it != g_download_progress.end() && !it->second->completed.load() && !it->second->failed.load()) {
                        json error = {{"error", {{"code", 409}, {"message", "Download already in progress"}}}};
                        res.status = 409;
                        res.set_content(error.dump(), "application/json");
                        return;
                    }
                }
                
                // Create progress tracker
                auto progress = std::make_shared<DownloadProgress>();
                {
                    std::lock_guard<std::mutex> lock(g_progress_mutex);
                    g_download_progress[model_name] = progress;
                }
                
                // Start download in background thread
                std::thread download_thread([this, model_name, progress]() {
                    try {
                        // Set thread-local variables for callback
                        g_current_progress = progress;
                        g_current_model_name = model_name;
                        
                        // Static progress callback function
                        static auto progress_cb = [](double prog, long long current, long long total) {
                            if (g_current_progress) {
                                g_current_progress->progress.store(prog);
                                g_current_progress->current_bytes.store(current);
                                g_current_progress->total_bytes.store(total);
                                
                                // Terminal progress output
                                double current_mb = current / (1024.0 * 1024.0);
                                double total_mb = total / (1024.0 * 1024.0);
                                
                                int bar_width = 50;
                                int pos = (int)(prog / 100.0 * bar_width);
                                
                                std::cout << "\r[Download " << g_current_model_name << "] [";
                                for (int i = 0; i < bar_width; i++) {
                                    if (i < pos) std::cout << "█";
                                    else if (i == pos) std::cout << "▓";
                                    else std::cout << "░";
                                }
                                std::cout << "] " << std::fixed << std::setprecision(1) << prog << "% ";
                                std::cout << "(" << std::fixed << std::setprecision(1) << current_mb << " / ";
                                std::cout << total_mb << " MB)";
                                std::cout << std::flush;
                            }
                        };
                        
                        // Set up progress callback for terminal output
                        model_mgr_.set_progress_callback(progress_cb);
                        
                        // Download model
                        bool success = model_mgr_.pull_model(model_name);
                        
                        // Clear progress callback and thread-local
                        model_mgr_.set_progress_callback(nullptr);
                        g_current_progress = nullptr;
                        
                        if (success) {
                            progress->completed.store(true);
                            progress->progress.store(100.0);
                            std::cout << std::endl;
                            std::cout << "[Download " << model_name << "] ✓ Download completed successfully!" << std::endl;
                        } else {
                            progress->failed.store(true);
                            progress->error_message = "Download failed";
                            std::cout << std::endl;
                            std::cout << "[Download " << model_name << "] ✗ Download failed" << std::endl;
                        }
                    } catch (const std::exception& e) {
                        progress->failed.store(true);
                        progress->error_message = e.what();
                        model_mgr_.set_progress_callback(nullptr);
                        g_current_progress = nullptr;
                        std::cout << std::endl;
                        std::cout << "[Download " << model_name << "] ✗ Error: " << e.what() << std::endl;
                    }
                });
                download_thread.detach();
                
                // Return immediately
                json result = {
                    {"success", true},
                    {"message", "Download started"},
                    {"model", model_name}
                };
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
        
        // DELETE /api/models/:name - Remove a model
        server_->Delete(R"(/api/models/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                std::string model_name = req.matches[1];
                
                if (model_name.empty()) {
                    json error = {{"error", {{"code", 400}, {"message", "Model name is required"}}}};
                    res.status = 400;
                    res.set_content(error.dump(), "application/json");
                    return;
                }
                
                // Remove model (without confirmation for API)
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
        
        // POST /api/models/use - Switch to a model
        server_->Post("/api/models/use", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                std::string model_name = body.value("model", "");
                
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
                
                // Get model's max context and alias from registry
                int ctx_size = 4096; // Default
                std::string model_alias = model_name;
                if (model_mgr_.is_in_registry(model_name)) {
                    auto entry = model_mgr_.get_registry_entry(model_name);
                    if (entry.max_context > 0) {
                        ctx_size = entry.max_context;
                    }
                    if (!entry.name.empty()) {
                        model_alias = entry.name;
                    }
                }
                
                // Try to actually switch the model if callback is set
                bool model_loaded = false;
                std::cerr << "[DEBUG] /api/models/use: model_name=" << model_name 
                          << ", model_path=" << model_path 
                          << ", ctx_size=" << ctx_size 
                          << ", model_alias=" << model_alias << std::endl;
                std::cerr << "[DEBUG] g_model_switch_callback is " << (g_model_switch_callback ? "set" : "null") << std::endl;
                if (g_model_switch_callback) {
                    try {
                        std::cerr << "[DEBUG] Calling model switch callback..." << std::endl;
                        model_loaded = (*g_model_switch_callback)(model_path, model_name, ctx_size, model_alias);
                        std::cerr << "[DEBUG] Model switch callback returned: " << (model_loaded ? "true" : "false") << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "[ERROR] Error switching model: " << e.what() << std::endl;
                    }
                } else {
                    std::cerr << "[WARNING] Model switch callback is not set!" << std::endl;
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
                        : "Model selected. The model path will be sent in API requests. Note: llama-server uses the model loaded at startup. To actually use this model, restart the server with: ./delta-server -m \"" + model_path + "\" --port 8080"}
                };
                
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
    }
    
    void server_loop() {
        if (!server_->bind_to_port("127.0.0.1", port_)) {
            std::cerr << "Failed to bind model API server to port " << port_ << std::endl;
            return;
        }
        
        std::cout << "Model Management API server running on http://127.0.0.1:" << port_ << std::endl;
        server_->listen_after_bind();
    }
    
public:
    ModelAPIServer(int port = 8081) : port_(port), running_(false) {
        server_ = std::make_unique<httplib::Server>();
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
            server_thread_.join();
        }
    }
    
    ~ModelAPIServer() {
        stop();
    }
};

// Global server instance
static std::unique_ptr<ModelAPIServer> g_model_api_server;

void set_model_switch_callback(ModelSwitchCallback callback) {
    static ModelSwitchCallback stored_callback = callback;
    g_model_switch_callback = &stored_callback;
}

void start_model_api_server(int port) {
    if (!g_model_api_server) {
        g_model_api_server = std::make_unique<ModelAPIServer>(port);
        g_model_api_server->start();
    }
}

void stop_model_api_server() {
    if (g_model_api_server) {
        g_model_api_server->stop();
        g_model_api_server.reset();
    }
}

} // namespace delta

