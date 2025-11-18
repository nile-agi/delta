/**
 * Interactive Commands - Slash command system implementation
 */

#include "commands.h"
#include "update.h"
#include "history.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <fstream>
#include <map>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <vector>
#ifdef _WIN32
    #include <windows.h>
    #include <limits.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <limits.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
#endif

namespace delta {

// Static member initialization
std::map<std::string, CommandHandler> Commands::command_map_;
bool Commands::initialized_ = false;

// Launch server automatically (public method)
bool Commands::launch_server_auto(const std::string& model_path, int port, int ctx_size, const std::string& model_alias) {
    // Find llama-server binary in vendor/llama.cpp
    std::vector<std::string> server_candidates;
    std::string exe_dir = tools::FileOps::get_executable_dir();
    
    // When llama.cpp is built as subdirectory, llama-server is in the main build/bin/
    // Check relative to executable (most common case)
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "llama-server"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../llama-server"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "bin/llama-server"));
    
    // Check if executable is in a build directory - llama-server should be in same dir
    // For example: if delta is in build_macos/, llama-server is also in build_macos/
    if (exe_dir.find("build_") != std::string::npos) {
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "llama-server"));
    }
    
    // Check vendor directory - if llama.cpp was built standalone
    server_candidates.push_back("vendor/llama.cpp/build/bin/llama-server");
    server_candidates.push_back("./vendor/llama.cpp/build/bin/llama-server");
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "vendor/llama.cpp/build/bin/llama-server"));
    
    // Check if we're in build directory (relative paths from source)
    server_candidates.push_back("../vendor/llama.cpp/build/bin/llama-server");
    server_candidates.push_back("../../vendor/llama.cpp/build/bin/llama-server");
    
    // Check build directories (if running from build_macos, build_linux, etc.)
    // llama-server should be in the same build directory as delta
    std::string build_dirs_server[] = {"build_macos", "build_linux", "build_windows", "build_android", "build_ios"};
    for (const auto& build_dir : build_dirs_server) {
        server_candidates.push_back(tools::FileOps::join_path(build_dir, "llama-server"));
        server_candidates.push_back(tools::FileOps::join_path(build_dir, "bin/llama-server"));
        std::string rel_path = tools::FileOps::join_path("..", build_dir);
        server_candidates.push_back(tools::FileOps::join_path(rel_path, "llama-server"));
    }
    
    // Check if llama.cpp was built in a different location
    server_candidates.push_back("vendor/llama.cpp/bin/llama-server");
    server_candidates.push_back("./vendor/llama.cpp/bin/llama-server");
    
    // Check common installation locations first (system-installed)
    server_candidates.push_back("/opt/homebrew/bin/llama-server");  // Homebrew on Apple Silicon
    server_candidates.push_back("/usr/local/bin/llama-server");     // Homebrew on Intel Mac
    server_candidates.push_back("/usr/bin/llama-server");
    
    // Check system PATH (if llama-server was installed separately)
    server_candidates.push_back("llama-server");
    
    std::string server_bin;
    for (const auto& candidate : server_candidates) {
        if (tools::FileOps::file_exists(candidate)) {
            server_bin = candidate;
            break;
        }
    }
    
    if (server_bin.empty()) {
        // llama-server not found anywhere
        // Checked: build directories, vendor directory, system PATH, and common install locations
        // User needs to either:
        // 1. Install llama-server: brew install llama.cpp (macOS) - RECOMMENDED
        // 2. Build llama-server in vendor/llama.cpp/build
        return false;
    }
    
    // Verify model path exists
    if (!tools::FileOps::file_exists(model_path)) {
        // Model path is invalid
        return false;
    }
    
    // Get the path to the web UI directory (use original llama.cpp web UI)
    std::string public_path;
    
    // Get executable directory and project root
    std::string exe_parent = tools::FileOps::join_path(exe_dir, "..");
    std::string exe_grandparent = tools::FileOps::join_path(exe_parent, "..");
    
    // Check for Homebrew share directory first, then public/ (built web UI from assets/), then assets/ as fallback
    std::vector<std::string> public_candidates = {
        // Homebrew installed location (priority)
        "/opt/homebrew/share/delta-cli/webui",
        "/usr/local/share/delta-cli/webui",
        tools::FileOps::join_path(exe_dir, "../../share/delta-cli/webui"),
        tools::FileOps::join_path(exe_dir, "../../../share/delta-cli/webui"),
        // Relative to executable
        tools::FileOps::join_path(exe_dir, "public"),
        tools::FileOps::join_path(exe_dir, "../public"),
        tools::FileOps::join_path(exe_grandparent, "public"),
        // Current directory
        "public",
        "./public",
        "../public",
        // Assets directory (source, not built)
        tools::FileOps::join_path(exe_dir, "assets"),
        tools::FileOps::join_path(exe_dir, "../assets"),
        tools::FileOps::join_path(exe_grandparent, "assets"),
        "assets",
        "./assets",
        "../assets"
    };
    
    for (const auto& candidate : public_candidates) {
        if (tools::FileOps::dir_exists(candidate)) {
            // Check for index.html.gz first (preferred), then index.html
            std::string index_file_gz = tools::FileOps::join_path(candidate, "index.html.gz");
            std::string index_file = tools::FileOps::join_path(candidate, "index.html");
            if (tools::FileOps::file_exists(index_file_gz) || tools::FileOps::file_exists(index_file)) {
                public_path = candidate;
                // Convert to absolute path for reliability
                if (!public_path.empty() && public_path[0] != '/') {
                    char resolved[PATH_MAX];
                    if (realpath(public_path.c_str(), resolved) != nullptr) {
                        public_path = std::string(resolved);
                    } else {
                        // Try relative to current working directory
                        char cwd[PATH_MAX];
                        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                            std::string full_path = tools::FileOps::join_path(std::string(cwd), public_path);
                            if (realpath(full_path.c_str(), resolved) != nullptr) {
                                public_path = std::string(resolved);
                            }
                        }
                    }
                }
                break;
            }
        }
    }
    
    // Convert relative path to absolute path for llama-server
    if (!public_path.empty() && public_path[0] != '/') {
        // Try multiple strategies to get absolute path
        char resolved_path[PATH_MAX];
        bool resolved = false;
        
        // Strategy 1: Try realpath on the path directly (if it exists from current dir)
        if (realpath(public_path.c_str(), resolved_path) != nullptr) {
            public_path = std::string(resolved_path);
            resolved = true;
        }
        
        // Strategy 2: Try relative to current working directory
        if (!resolved) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                std::string full_path = tools::FileOps::join_path(std::string(cwd), public_path);
                if (realpath(full_path.c_str(), resolved_path) != nullptr) {
                    public_path = std::string(resolved_path);
                    resolved = true;
                }
            }
        }
        
        // Strategy 3: Try relative to executable directory
        if (!resolved) {
            std::string exe_based_path = tools::FileOps::join_path(exe_dir, public_path);
            if (realpath(exe_based_path.c_str(), resolved_path) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // Strategy 4: Try relative to project root (exe_grandparent)
        if (!resolved) {
            std::string project_path = tools::FileOps::join_path(exe_grandparent, public_path);
            if (realpath(project_path.c_str(), resolved_path) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // If all else fails, construct absolute path manually from exe_dir
        if (!resolved) {
            // Build absolute path from executable directory
            std::string abs_path = tools::FileOps::join_path(exe_dir, public_path);
            // Normalize the path (remove .. and .)
            char normalized[PATH_MAX];
            if (realpath(abs_path.c_str(), normalized) != nullptr) {
                public_path = std::string(normalized);
            } else {
                // Last resort: use exe_grandparent
                abs_path = tools::FileOps::join_path(exe_grandparent, public_path);
                if (realpath(abs_path.c_str(), normalized) != nullptr) {
                    public_path = std::string(normalized);
                }
            }
        }
    }
    
    // If not found, server will use default web UI (no --path flag)
    
    // Build command: llama-server -m "model_path" --port 8080 -c 4096 [--path public_dir] [--alias model_alias]
    std::stringstream cmd;
    cmd << server_bin
        << " -m \"" << model_path << "\""
        << " --port " << port
        << " -c " << ctx_size;
    
    // Add --flash-attn flag for all models
    cmd << " --flash-attn";
    
    // Add --jinja flag for gemma3 models
    // Check model_alias and model_path for gemma3 (case-insensitive)
    std::string model_alias_lower = model_alias;
    std::string model_path_lower = model_path;
    std::transform(model_alias_lower.begin(), model_alias_lower.end(), model_alias_lower.begin(), ::tolower);
    std::transform(model_path_lower.begin(), model_path_lower.end(), model_path_lower.begin(), ::tolower);
    if (model_alias_lower.find("gemma3") != std::string::npos || model_path_lower.find("gemma3") != std::string::npos) {
        cmd << " --jinja";
    }
    
    // Add --alias flag to use short_name instead of filename in web UI
    if (!model_alias.empty()) {
        cmd << " --alias \"" << model_alias << "\"";
    }
    
    // Add --path flag to use Delta web UI if found
    if (!public_path.empty()) {
        cmd << " --path \"" << public_path << "\"";
    }
    
    // Debug: Print the command being executed (can be removed in production)
    // std::cerr << "Executing: " << cmd.str() << std::endl;
    
    // Run in background
    // Note: We redirect stderr to a temp file to capture errors for debugging
#ifdef _WIN32
    std::string err_file = "nul";
    cmd << " 2>" << err_file;
    // On Windows, use START to run in background
    std::string full_cmd = "start /B " + cmd.str();
    int result = std::system(full_cmd.c_str());
#else
    // Create a temporary error log file to capture startup errors
    std::string err_file = "/tmp/delta-server-err-" + std::to_string(port) + ".log";
    // Remove old log file if it exists
    std::remove(err_file.c_str());
    cmd << " 2>" << err_file << " &";
    int result = std::system(cmd.str().c_str());
#endif
    
    // Wait for server to start and verify it's running
    // Give server time to initialize (especially for model loading)
    bool server_listening = false;
    for (int attempt = 0; attempt < 20; attempt++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Check if port is listening using socket connection
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock != INVALID_SOCKET) {
                sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(port);
                inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
                    server_listening = true;
                    closesocket(sock);
                } else {
                    closesocket(sock);
                }
            }
            WSACleanup();
        }
#else
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock >= 0) {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_aton("127.0.0.1", &addr.sin_addr);
            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                server_listening = true;
                close(sock);
            } else {
                close(sock);
            }
        }
#endif
        if (server_listening) {
            break;
        }
    }
    
    // Check error log for any startup errors
#ifndef _WIN32
    std::ifstream err_log(err_file);
    if (err_log.is_open()) {
        std::string line;
        std::vector<std::string> error_lines;
        while (std::getline(err_log, line)) {
            if (!line.empty()) {
                // Filter out informational messages
                std::string line_lower = line;
                std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);
                // Look for actual error messages (not just info)
                if (line_lower.find("error") != std::string::npos ||
                    line_lower.find("failed") != std::string::npos ||
                    line_lower.find("fatal") != std::string::npos ||
                    (line_lower.find("unknown") != std::string::npos && line_lower.find("option") != std::string::npos)) {
                    error_lines.push_back(line);
                }
            }
        }
        err_log.close();
        
        if (!error_lines.empty()) {
            // Show errors - these indicate the server failed to start
            UI::print_error("Server startup errors detected:");
            for (size_t i = 0; i < error_lines.size() && i < 5; i++) {
                std::cerr << "  " << error_lines[i] << std::endl;
            }
            std::cerr << "\nFull error log: " << err_file << std::endl;
            std::cerr << "\nTip: If you see 'unknown option' errors, your llama-server build may not support all flags." << std::endl;
            return false;
        }
    }
#endif
    
    if (result != 0) {
        UI::print_error("Failed to start server process");
        return false;
    }
    
    if (!server_listening) {
        UI::print_error("Server process started but port " + std::to_string(port) + " is not listening");
        UI::print_info("Check error log: " + err_file);
        UI::print_info("The server may still be loading the model. Try accessing http://localhost:" + std::to_string(port) + " in a few seconds.");
        // Don't return false here - server might still be starting
    }
    
    return true;
}

void Commands::init() {
    if (initialized_) return;
    
    // Register all commands
    command_map_["download"] = handle_download;
    command_map_["pull"] = handle_download;
    command_map_["remove"] = handle_remove;
    command_map_["delete"] = handle_remove;
    command_map_["list"] = handle_list;
    command_map_["list-models"] = handle_list;
    command_map_["models"] = handle_list;
    command_map_["list-local"] = handle_list;
    command_map_["use"] = handle_use;
    command_map_["available"] = handle_available;
    command_map_["list-available"] = handle_available;
    command_map_["clear-screen"] = handle_clear_screen;
    command_map_["help"] = handle_help;
    
    initialized_ = true;
}

bool Commands::process_command(const std::string& input, InteractiveSession& session) {
    // Parse command and arguments
    std::vector<std::string> args = parse_args(input);
    if (args.empty()) return false;
    
    std::string command = args[0];
    args.erase(args.begin()); // Remove command from args
    
    // Check if command exists
    auto it = command_map_.find(command);
    if (it == command_map_.end()) {
        UI::print_info("ℹ Type /help to see available commands");
        return true;
    }
    
    // Check if online command and we're offline
    if (is_online_command(command)) {
        // For now, we'll try the command and let it handle offline gracefully
        // In a real implementation, you'd check network connectivity here
    }
    
    // Execute command
    return it->second(args, session);
}

std::vector<std::string> Commands::parse_args(const std::string& input) {
    std::vector<std::string> args;
    std::istringstream iss(input);
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

void Commands::show_help() {
    std::cout << "\n" << UI::BRIGHT_GREEN << UI::BOLD << "Interactive Commands:" << UI::RESET << std::endl;
    std::cout << "  " << UI::GREEN << "/download <model>" << UI::RESET << "     - Download a model" << std::endl;
    std::cout << "  " << UI::GREEN << "/remove <model>" << UI::RESET << "       - Remove a model (alias: /delete)" << std::endl;
    std::cout << "  " << UI::GREEN << "/list" << UI::RESET << "                - List local models" << std::endl;
    std::cout << "  " << UI::GREEN << "/available" << UI::RESET << "            - List available models" << std::endl;
    std::cout << "  " << UI::GREEN << "/use <model>" << UI::RESET << "          - Switch to another model" << std::endl;
    std::cout << "  " << UI::GREEN << "/clear-screen" << UI::RESET << "         - Clear the terminal screen" << std::endl;
    std::cout << "  " << UI::GREEN << "/help" << UI::RESET << "                 - Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "  " << UI::YELLOW << "exit, quit" << UI::RESET << "            - Exit interactive mode" << std::endl;
    std::cout << std::endl;
}

bool Commands::is_online_command(const std::string& command) {
    return (command == "download" || command == "pull");
}

void Commands::show_offline_message(const std::string& command) {
    UI::print_error("Command /" + command + " requires internet connection");
    UI::print_info("Please check your connection and try again");
}

// Command implementations
bool Commands::handle_download(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_error("Please specify a model name");
        UI::print_info("Usage: /download <model-name>");
        UI::print_info("Example: /download qwen3:0.6b");
        return true;
    }
    
    std::string model_name = args[0];
    UI::print_info("Downloading model: " + model_name);
    
    // Set progress callback for download with visual progress bar
    session.model_mgr->set_progress_callback([](double progress, long long current, long long total) {
        // Calculate MB
        double current_mb = current / (1024.0 * 1024.0);
        double total_mb = total / (1024.0 * 1024.0);
        
        // Create progress bar
        int bar_width = 50;
        int pos = (int)(progress / 100.0 * bar_width);
        
        std::cout << "\r  [";
        for (int i = 0; i < bar_width; i++) {
            if (i < pos) std::cout << "█";
            else if (i == pos) std::cout << "▓";
            else std::cout << "░";
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << progress << "% ";
        std::cout << "(" << std::fixed << std::setprecision(1) << current_mb << " / ";
        std::cout << total_mb << " MB)";
        std::cout << std::flush;
    });
    
    bool success = session.model_mgr->pull_model(model_name);
    session.model_mgr->set_progress_callback(nullptr);
    
    if (success) {
        std::cout << std::endl;
        UI::print_info("✓ Model downloaded successfully!");
        UI::print_info("You can now use: /use " + model_name);
    } else {
        std::cout << std::endl;
        UI::print_error("✗ Download failed");
        UI::print_info("Check your internet connection and try again");
    }
    
    return true;
}

bool Commands::handle_list(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    // Get friendly model list (local models only)
    auto models = session.model_mgr->get_friendly_model_list(false);
    
    if (models.empty()) {
        UI::print_info("No models found locally.");
        UI::print_info("Download a model with: /download <model-name>");
        UI::print_info("See available models: /available");
        return true;
    }
    
    UI::print_border("Locally Cached Models");
    
    for (const auto& model : models) {
        std::string current_indicator = (model.name == session.current_model) ? " [CURRENT]" : "";
        std::cout << "  • " << model.name << current_indicator << std::endl;
        std::cout << "      " << model.display_name << " - " << model.description << std::endl;
        std::cout << "      Size: " << model.size_str << " | Quant: " << model.quantization << std::endl;
        std::cout << std::endl;
    }
    
    UI::print_info("Use '/use " + models[0].name + "' to switch to a model");
    return true;
}


bool Commands::handle_use(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_error("Please specify a model name");
        UI::print_info("Usage: /use <model-name>");
        UI::print_info("Example: /use qwen3:0.6b");
        return true;
    }
    
    std::string model_name = args[0];
    
    // Check if model is installed
    if (!session.model_mgr->is_model_installed(model_name)) {
        UI::print_error("Model not found: " + model_name);
        UI::print_info("Use /list to see available models");
        UI::print_info("Use /download to download a model");
        return true;
    }
    
    // Get model path
    std::string model_path = session.model_mgr->get_model_path(model_name);
    if (model_path.empty()) {
        UI::print_error("Could not find model path for: " + model_name);
        return true;
    }
    
    UI::print_info("Switching to model: " + model_name);
    UI::print_info("Loading model...");
    
    // Update config
    session.config->model_path = model_path;
    session.current_model = model_name;
    
    // Reload model
    if (!session.engine->load_model(*session.config)) {
        UI::print_error("Failed to load model: " + model_name);
        return true;
    }
    
    UI::print_info("✓ Model loaded successfully!");
    UI::print_info("Current model: " + session.current_model);
    
    // Automatically launch web UI server with the new model
    // Get model's max context from registry (use model's max_context as default)
    int ctx_size = 4096;  // Default fallback
    // Try to get max context from registry
    std::string registry_name = model_name;
    if (session.model_mgr->is_in_registry(registry_name)) {
        auto entry = session.model_mgr->get_registry_entry(registry_name);
        if (entry.max_context > 0) {
            ctx_size = entry.max_context;
        }
    } else {
        // Try converting dash to colon format
        size_t last_dash = registry_name.find_last_of('-');
        if (last_dash != std::string::npos) {
            std::string colon_name = registry_name.substr(0, last_dash) + ":" + 
                                     registry_name.substr(last_dash + 1);
            if (session.model_mgr->is_in_registry(colon_name)) {
                auto entry = session.model_mgr->get_registry_entry(colon_name);
                if (entry.max_context > 0) {
                    ctx_size = entry.max_context;
                }
            }
        }
    }
    // Fallback to config.n_ctx if model not in registry and config.n_ctx is set
    if (ctx_size <= 0 && session.config->n_ctx > 0) {
        ctx_size = session.config->n_ctx;
    }
    
    // Get short_name for model alias in web UI
    std::string model_alias;
    std::string registry_name_for_alias = model_name;
    if (session.model_mgr->is_in_registry(registry_name_for_alias)) {
        auto entry = session.model_mgr->get_registry_entry(registry_name_for_alias);
        if (!entry.short_name.empty()) {
            model_alias = entry.short_name;
        }
    } else {
        // Try converting dash to colon format
        size_t last_dash = registry_name_for_alias.find_last_of('-');
        if (last_dash != std::string::npos) {
            std::string colon_name = registry_name_for_alias.substr(0, last_dash) + ":" + 
                                     registry_name_for_alias.substr(last_dash + 1);
            if (session.model_mgr->is_in_registry(colon_name)) {
                auto entry = session.model_mgr->get_registry_entry(colon_name);
                if (!entry.short_name.empty()) {
                    model_alias = entry.short_name;
                }
            }
        }
    }
    
    // If still not found, try to get short_name from filename
    if (model_alias.empty()) {
        // Extract filename from model_path
        std::string filename = model_path;
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }
        model_alias = session.model_mgr->get_short_name_from_filename(filename);
    }
    
    if (Commands::launch_server_auto(model_path, 8080, ctx_size, model_alias)) {
        UI::print_success("Delta Server started in background");
        std::string url = "http://localhost:8080";
        UI::print_info("Open: " + url);
        // Open browser after a short delay to ensure server is ready
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (tools::Browser::open_url(url)) {
            UI::print_info("Browser opened automatically");
        }
    }
    // If launch fails, don't show error - server is optional
    
    return true;
}


bool Commands::handle_available(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    // Get friendly model list (all available models)
    auto models = session.model_mgr->get_friendly_model_list(true);
    
    if (models.empty()) {
        UI::print_error("No models available in registry");
        return true;
    }
    
    UI::print_border("Available Models to Download");
    UI::print_info("Use '/download <model-name>' to download");
    std::cout << std::endl;
    
    for (const auto& model : models) {
        std::string status = model.installed ? "[✓ Installed]" : "[ Download  ]";
        std::string current_indicator = (model.name == session.current_model) ? " [CURRENT]" : "";
        
        std::cout << "  " << status << " " << model.name << current_indicator << std::endl;
        std::cout << "      " << model.display_name << " - " << model.description << std::endl;
        std::cout << "      Size: " << model.size_str << " | Quant: " << model.quantization << std::endl;
        std::cout << std::endl;
    }
    
    int installed_count = 0;
    for (const auto& m : models) {
        if (m.installed) installed_count++;
    }
    
    UI::print_info("Total: " + std::to_string(models.size()) + " models available (" + 
                  std::to_string(installed_count) + " installed)");
    return true;
}






// Removed: handle_server - command removed for simplicity

// Removed: handle_update, handle_no_color - commands removed for simplicity

bool Commands::handle_help(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    (void)session; // Suppress unused parameter warning
    show_help();
    return true;
}

bool Commands::handle_remove(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_error("Please specify a model name");
        UI::print_info("Usage: /remove <model-name>");
        UI::print_info("Example: /remove qwen2.5:0.6b");
        UI::print_info("Use /list to see installed models");
        return true;
    }
    
    std::string model_name = args[0];
    
    // Check if model is currently in use
    if (!session.current_model.empty() && session.current_model == model_name) {
        UI::print_error("Cannot delete model '" + model_name + "' - it is currently in use");
        UI::print_info("Switch to another model first with /use <model-name>");
        return true;
    }
    
    // Perform deletion with confirmation
    bool success = session.model_mgr->remove_model_with_confirmation(model_name);
    
    if (success) {
        UI::print_success("Model '" + model_name + "' removed successfully");
    }
    
    return true;
}

// ============================================================================
// HISTORY AND SESSION MANAGEMENT COMMANDS
// ============================================================================

bool Commands::handle_clear_screen(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args;
    (void)session;
    
    UI::clear_screen();
    return true;
}

// Removed: handle_history, handle_delete_history, handle_new_session, handle_list_sessions,
//          handle_switch_session, handle_delete_session, handle_active_session, handle_export_session
//          - commands removed for simplicity

} // namespace delta
