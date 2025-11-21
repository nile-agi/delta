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
#include "commands.h"
#include <cpp-httplib/httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>

using json = nlohmann::json;

namespace delta {

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
        server_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
            return;
        });
        
        // GET /api/models/available - List all available models
        server_->Get("/api/models/available", [this](const httplib::Request&, httplib::Response& res) {
            try {
                auto models = model_mgr_.get_friendly_model_list(true);
                json result = json::array();
                
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
                    result.push_back(model_json);
                }
                
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
                json result = json::array();
                
                for (const auto& model : models) {
                    json model_json = {
                        {"name", model.name},
                        {"display_name", model.display_name},
                        {"description", model.description},
                        {"size_str", model.size_str},
                        {"quantization", model.quantization},
                        {"size_bytes", model.size_bytes}
                    };
                    result.push_back(model_json);
                }
                
                res.set_content(result.dump(), "application/json");
            } catch (const std::exception& e) {
                json error = {{"error", {{"code", 500}, {"message", e.what()}}}};
                res.status = 500;
                res.set_content(error.dump(), "application/json");
            }
        });
        
        // POST /api/models/download - Download a model
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
                
                // Download model (synchronous for now)
                bool success = model_mgr_.pull_model(model_name);
                
                if (success) {
                    json result = {{"success", true}, {"message", "Model downloaded successfully"}};
                    res.set_content(result.dump(), "application/json");
                } else {
                    json error = {{"error", {{"code", 500}, {"message", "Failed to download model"}}}};
                    res.status = 500;
                    res.set_content(error.dump(), "application/json");
                }
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
                
                // Get model's max context from registry
                int ctx_size = 4096;  // Default fallback
                std::string registry_name = model_name;
                if (model_mgr_.is_in_registry(registry_name)) {
                    auto entry = model_mgr_.get_registry_entry(registry_name);
                    if (entry.max_context > 0) {
                        ctx_size = entry.max_context;
                    }
                } else {
                    // Try converting dash to colon format
                    size_t last_dash = registry_name.find_last_of('-');
                    if (last_dash != std::string::npos) {
                        std::string colon_name = registry_name.substr(0, last_dash) + ":" + 
                                                 registry_name.substr(last_dash + 1);
                        if (model_mgr_.is_in_registry(colon_name)) {
                            auto entry = model_mgr_.get_registry_entry(colon_name);
                            if (entry.max_context > 0) {
                                ctx_size = entry.max_context;
                            }
                        }
                    }
                }
                
                // Get model alias (name with colon) for web UI
                std::string model_alias;
                std::string found_name = model_mgr_.get_name_from_filename(model_path);
                if (!found_name.empty()) {
                    model_alias = found_name;
                } else if (model_mgr_.is_in_registry(registry_name)) {
                    auto entry = model_mgr_.get_registry_entry(registry_name);
                    if (!entry.name.empty()) {
                        model_alias = entry.name;
                    }
                } else {
                    // Try converting dash to colon format
                    size_t last_dash = registry_name.find_last_of('-');
                    if (last_dash != std::string::npos) {
                        std::string colon_name = registry_name.substr(0, last_dash) + ":" + 
                                                 registry_name.substr(last_dash + 1);
                        if (model_mgr_.is_in_registry(colon_name)) {
                            auto entry = model_mgr_.get_registry_entry(colon_name);
                            if (!entry.name.empty()) {
                                model_alias = entry.name;
                            }
                        }
                    }
                }
                
                // Restart the server with the new model
                bool server_restarted = Commands::launch_server_auto(model_path, 8080, ctx_size, model_alias);
                
                json result = {
                    {"success", true},
                    {"model_path", model_path},
                    {"model_alias", model_alias},
                    {"server_restarted", server_restarted},
                    {"message", server_restarted 
                        ? "Model switched successfully. Server is restarting with the new model."
                        : "Model switched successfully, but server restart failed. Please restart manually."}
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

