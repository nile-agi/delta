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
#ifdef _WIN32
    #include <windows.h>
    #include <limits.h>
#else
    #include <unistd.h>
    #include <limits.h>
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
    
    // Check for assets/ directory first (custom web UI), then llama.cpp web UI as fallback
    std::vector<std::string> public_candidates = {
        "assets",
        "./assets",
        "../assets",
        tools::FileOps::join_path(exe_dir, "assets"),
        tools::FileOps::join_path(exe_dir, "../assets"),
        tools::FileOps::join_path(exe_grandparent, "assets"),
        "vendor/llama.cpp/tools/server/public",
        "./vendor/llama.cpp/tools/server/public",
        "../vendor/llama.cpp/tools/server/public",
        tools::FileOps::join_path(exe_dir, "vendor/llama.cpp/tools/server/public"),
        tools::FileOps::join_path(exe_dir, "../vendor/llama.cpp/tools/server/public"),
        tools::FileOps::join_path(exe_grandparent, "vendor/llama.cpp/tools/server/public"),
        "vendor/llama.cpp/tools/server/webui",
        "./vendor/llama.cpp/tools/server/webui",
        "../vendor/llama.cpp/tools/server/webui",
        tools::FileOps::join_path(exe_dir, "vendor/llama.cpp/tools/server/webui"),
        tools::FileOps::join_path(exe_dir, "../vendor/llama.cpp/tools/server/webui"),
        tools::FileOps::join_path(exe_grandparent, "vendor/llama.cpp/tools/server/webui")
    };
    
    for (const auto& candidate : public_candidates) {
        if (tools::FileOps::dir_exists(candidate)) {
            std::string index_file = tools::FileOps::join_path(candidate, "index.html");
            if (tools::FileOps::file_exists(index_file)) {
                public_path = candidate;
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
    
    // Add --alias flag to use short_name instead of filename in web UI
    if (!model_alias.empty()) {
        cmd << " --alias \"" << model_alias << "\"";
    }
    
    // Add --path flag to use llama.cpp web UI if found
    if (!public_path.empty()) {
        cmd << " --path \"" << public_path << "\"";
    }
    
    // Run in background
#ifdef _WIN32
    cmd << " >nul 2>&1";
    // On Windows, use START to run in background
    std::string full_cmd = "start /B " + cmd.str();
    int result = std::system(full_cmd.c_str());
#else
    // Suppress output - server runs in background
    cmd << " >/dev/null 2>&1 &";
    int result = std::system(cmd.str().c_str());
#endif
    
    // Small delay to let server start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    return (result == 0);
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
    command_map_["updates"] = handle_updates;
    command_map_["check-updates"] = handle_updates;
    command_map_["use"] = handle_use;
    command_map_["version"] = handle_version;
    command_map_["available"] = handle_available;
    command_map_["list-available"] = handle_available;
    command_map_["tokens"] = handle_tokens;
    command_map_["temperature"] = handle_temperature;
    command_map_["gpu-layers"] = handle_gpu_layers;
    command_map_["multimodal"] = handle_multimodal;
    command_map_["interactive"] = handle_interactive;
    command_map_["server"] = handle_server;
    command_map_["update"] = handle_update;
    command_map_["no-color"] = handle_no_color;
    command_map_["help"] = handle_help;
    
    // History and session management commands
    command_map_["clear-screen"] = handle_clear_screen;
    command_map_["history"] = handle_history;
    command_map_["delete-history"] = handle_delete_history;
    command_map_["new-session"] = handle_new_session;
    command_map_["list-sessions"] = handle_list_sessions;
    command_map_["switch-session"] = handle_switch_session;
    command_map_["delete-session"] = handle_delete_session;
    command_map_["active-session"] = handle_active_session;
    command_map_["status"] = handle_active_session;
    command_map_["export-session"] = handle_export_session;
    
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
        UI::print_error("Unknown command: /" + command);
        UI::print_info("Type /help to see available commands");
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
    std::cout << "  " << UI::GREEN << "/tokens <N>" << UI::RESET << "           - Set max tokens (default: 512)" << std::endl;
    std::cout << "  " << UI::GREEN << "/temperature <F>" << UI::RESET << "       - Set temperature (default: 0.8)" << std::endl;
    std::cout << "  " << UI::GREEN << "/gpu-layers <N>" << UI::RESET << "        - Set GPU layers (default: 0, -1 for all)" << std::endl;
    std::cout << "  " << UI::GREEN << "/multimodal" << UI::RESET << "            - Toggle multimodal mode" << std::endl;
    std::cout << "  " << UI::GREEN << "/server" << UI::RESET << " [model] [--port N] [--ctx-size N] - Start web dashboard" << std::endl;
    std::cout << "  " << UI::GREEN << "/updates" << UI::RESET << "               - Check for updates" << std::endl;
    std::cout << "  " << UI::GREEN << "/version" << UI::RESET << "               - Show version info" << std::endl;
    std::cout << "  " << UI::GREEN << "/no-color" << UI::RESET << "              - Toggle colored output" << std::endl;
    std::cout << "  " << UI::GREEN << "/help" << UI::RESET << "                 - Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "  " << UI::BRIGHT_GREEN << UI::BOLD << "History & Session Management:" << UI::RESET << std::endl;
    std::cout << "  " << UI::GREEN << "/clear-screen" << UI::RESET << "         - Clear the terminal screen" << std::endl;
    std::cout << "  " << UI::GREEN << "/history" << UI::RESET << "              - Show conversation history" << std::endl;
    std::cout << "  " << UI::GREEN << "/delete-history <all|id|day|week|year> [date]" << UI::RESET << std::endl;
    std::cout << "                                - Delete history entries" << std::endl;
    std::cout << "  " << UI::GREEN << "/new-session <name>" << UI::RESET << "    - Create a new chat session" << std::endl;
    std::cout << "  " << UI::GREEN << "/list-sessions" << UI::RESET << "         - List all saved sessions" << std::endl;
    std::cout << "  " << UI::GREEN << "/switch-session <name>" << UI::RESET << " - Switch to another session" << std::endl;
    std::cout << "  " << UI::GREEN << "/delete-session <name>" << UI::RESET << " - Delete a session" << std::endl;
    std::cout << "  " << UI::GREEN << "/active-session" << UI::RESET << "        - Show current session info (alias: /status)" << std::endl;
    std::cout << "  " << UI::GREEN << "/export-session <name> <format>" << UI::RESET << " - Export session to JSON/CSV" << std::endl;
    std::cout << std::endl;
    std::cout << "  " << UI::YELLOW << "exit, quit" << UI::RESET << "            - Exit interactive mode" << std::endl;
    std::cout << "  " << UI::YELLOW << "clear" << UI::RESET << "                - Clear conversation history" << std::endl;
    std::cout << std::endl;
}

bool Commands::is_online_command(const std::string& command) {
    return (command == "download" || command == "pull" || 
            command == "updates" || command == "check-updates" || 
            command == "update");
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

bool Commands::handle_updates(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    (void)session; // Suppress unused parameter warning
    UI::print_info("Checking for updates...");
    
    UpdateManager updater;
    bool has_update = updater.check_for_updates(true);
    
    if (has_update) {
        UI::print_info("✓ Update available! Use /update to install");
    } else {
        UI::print_info("✓ You're running the latest version");
    }
    
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

bool Commands::handle_version(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    (void)session; // Suppress unused parameter warning
    UI::print_info("Delta CLI v1.0.0");
    UI::print_info("Built with llama.cpp");
    UI::print_info("Current model: " + (session.current_model.empty() ? "None" : session.current_model));
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

bool Commands::handle_tokens(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_info("Current max tokens: " + std::to_string(session.max_tokens));
        UI::print_info("Usage: /tokens <number>");
        return true;
    }
    
    try {
        int tokens = std::stoi(args[0]);
        if (tokens <= 0) {
            UI::print_error("Tokens must be a positive number");
            return true;
        }
        
        session.max_tokens = tokens;
        UI::print_info("✓ Max tokens set to: " + std::to_string(tokens));
    } catch (const std::exception& e) {
        UI::print_error("Invalid number: " + args[0]);
    }
    
    return true;
}

bool Commands::handle_temperature(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_info("Current temperature: " + std::to_string(session.temperature));
        UI::print_info("Usage: /temperature <number>");
        return true;
    }
    
    try {
        double temp = std::stod(args[0]);
        if (temp < 0.0 || temp > 2.0) {
            UI::print_error("Temperature must be between 0.0 and 2.0");
            return true;
        }
        
        session.temperature = temp;
        session.config->temperature = temp;
        UI::print_info("✓ Temperature set to: " + std::to_string(temp));
    } catch (const std::exception& e) {
        UI::print_error("Invalid number: " + args[0]);
    }
    
    return true;
}

bool Commands::handle_gpu_layers(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_info("Current GPU layers: " + std::to_string(session.gpu_layers));
        UI::print_info("Usage: /gpu-layers <number> (-1 for all)");
        return true;
    }
    
    try {
        int layers = std::stoi(args[0]);
        if (layers < -1) {
            UI::print_error("GPU layers must be -1 or greater");
            return true;
        }
        
        session.gpu_layers = layers;
        session.config->n_gpu_layers = layers;
        
        if (layers != session.gpu_layers) {
            UI::print_info("✓ GPU layers set to: " + std::to_string(layers));
            UI::print_info("Note: Model will need to be reloaded for changes to take effect");
            UI::print_info("Use /use <model-name> to reload the current model");
        } else {
            UI::print_info("✓ GPU layers set to: " + std::to_string(layers));
        }
    } catch (const std::exception& e) {
        UI::print_error("Invalid number: " + args[0]);
    }
    
    return true;
}

bool Commands::handle_multimodal(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    session.multimodal = !session.multimodal;
    session.config->multimodal = session.multimodal;
    
    if (session.multimodal) {
        UI::print_info("✓ Multimodal mode enabled");
        UI::print_info("You can now provide images along with text prompts");
    } else {
        UI::print_info("✓ Multimodal mode disabled");
    }
    
    return true;
}

bool Commands::handle_interactive(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    (void)session; // Suppress unused parameter warning
    UI::print_info("You're already in interactive mode!");
    UI::print_info("Type /help to see available commands");
    return true;
}

bool Commands::handle_server(const std::vector<std::string>& args, InteractiveSession& session) {
    // Parse: /server [model] [--port N] [--ctx-size N] [-c N]
    std::string model_name = session.current_model;
    int port = 8080;
    int ctx_size = -1;  // -1 means "use model default"
    bool ctx_size_explicit = false;  // Track if user explicitly set context size
    
    // Use session config if available and valid (allows override)
    if (session.config && session.config->n_ctx > 0) {
        ctx_size = session.config->n_ctx;
        ctx_size_explicit = true;
    }
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--port" && i + 1 < args.size()) {
            try { port = std::stoi(args[i+1]); } catch (...) { port = 8080; }
            ++i; // skip value
        } else if ((args[i] == "--ctx-size" || args[i] == "-c") && i + 1 < args.size()) {
            try { 
                ctx_size = std::stoi(args[i+1]);
                ctx_size_explicit = true;
                if (ctx_size < 512 || ctx_size > 131072) {
                    UI::print_error("Context size must be between 512 and 131072 (128K)");
                    return true;
                }
            } catch (...) { 
                UI::print_error("Invalid context size: " + args[i+1]);
                return true;
            }
            ++i; // skip value
        } else if (!args[i].empty() && args[i][0] != '-') {
            model_name = args[i];
        }
    }

    // Resolve model
    if (model_name.empty()) {
        model_name = session.model_mgr->get_auto_selected_model();
    }
    if (!session.model_mgr->is_model_installed(model_name)) {
        UI::print_error("Model not found: " + model_name);
        UI::print_info("Use /download " + model_name + " to install it");
        return true;
    }
    std::string model_path = session.model_mgr->get_model_path(model_name);
    if (model_path.empty()) {
        UI::print_error("Could not resolve model path for: " + model_name);
        return true;
    }
    
    // Get model's max context from registry if not explicitly set
    if (!ctx_size_explicit) {
        // Try to resolve model name to registry name (handle both "qwen3:0.6b" and "qwen3-0.6b")
        std::string registry_name = model_name;
        // Check if it's already in registry format
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
        // Fallback to default if still not set
        if (ctx_size <= 0) {
            ctx_size = 4096;  // Default fallback
        }
    }

    // Locate delta-server with comprehensive cross-platform search
    std::string exe_dir = tools::FileOps::get_executable_dir();
    std::vector<std::string> candidates;
    
    // First, check if we're in a development environment (build directories exist)
    std::string dev_candidates[] = {
        "build_macos/delta-server",
        "build_linux/delta-server", 
        "build_windows/delta-server.exe",
        "build_android/delta-server",
        "build_ios/delta-server"
    };
    
    for (const auto& dev_path : dev_candidates) {
        if (tools::FileOps::file_exists(dev_path)) {
            candidates.push_back(dev_path);
            candidates.push_back("./" + dev_path);
        }
    }
    
    // Then check relative to executable directory
#ifdef _WIN32
    candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server.exe"));
    candidates.push_back(tools::FileOps::join_path(exe_dir, "build_windows/delta-server.exe"));
#elif defined(__APPLE__)
    candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server"));
    candidates.push_back(tools::FileOps::join_path(exe_dir, "build_macos/delta-server"));
    candidates.push_back(tools::FileOps::join_path(exe_dir, "build_ios/delta-server"));
#else
    candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server"));
    candidates.push_back(tools::FileOps::join_path(exe_dir, "build_linux/delta-server"));
    candidates.push_back(tools::FileOps::join_path(exe_dir, "build_android/delta-server"));
#endif
    
    // Check common installation locations
    std::string install_locations[] = {
        "/usr/local/bin/delta-server",
        "/usr/bin/delta-server",
        "/opt/delta-cli/bin/delta-server",
        tools::FileOps::join_path(tools::FileOps::get_home_dir(), ".local/bin/delta-server")
    };
    
    for (const auto& install_path : install_locations) {
        candidates.push_back(install_path);
#ifdef _WIN32
        candidates.push_back(install_path + ".exe");
#endif
    }
    
    // Check system PATH
    candidates.push_back("delta-server");
#ifdef _WIN32
    candidates.push_back("delta-server.exe");
#endif
    
    std::string server_bin;
    for (const auto &c : candidates) {
        if (tools::FileOps::file_exists(c)) { server_bin = c; break; }
    }
    if (server_bin.empty()) {
#ifdef _WIN32
        UI::print_error("delta-server binary not found. Build it first (installers/build_windows.bat)");
#elif defined(__APPLE__)
        UI::print_error("delta-server binary not found. Build it first (installers/build_macos.sh)");
#else
        UI::print_error("delta-server binary not found. Build it first (installers/build_linux.sh)");
#endif
        return true;
    }

    // Get the path to the web UI directory (use original llama.cpp web UI)
    std::string public_path_server;
    std::string exe_dir_server = tools::FileOps::get_executable_dir();
    std::string exe_parent_server = tools::FileOps::join_path(exe_dir_server, "..");
    std::string exe_grandparent_server = tools::FileOps::join_path(exe_parent_server, "..");
    
    // Check for assets/ directory first (custom web UI), then llama.cpp web UI as fallback
    std::vector<std::string> public_candidates_server = {
        "assets",
        "./assets",
        "../assets",
        tools::FileOps::join_path(exe_dir_server, "assets"),
        tools::FileOps::join_path(exe_dir_server, "../assets"),
        tools::FileOps::join_path(exe_grandparent_server, "assets"),
        "vendor/llama.cpp/tools/server/public",
        "./vendor/llama.cpp/tools/server/public",
        "../vendor/llama.cpp/tools/server/public",
        tools::FileOps::join_path(exe_dir_server, "vendor/llama.cpp/tools/server/public"),
        tools::FileOps::join_path(exe_dir_server, "../vendor/llama.cpp/tools/server/public"),
        tools::FileOps::join_path(exe_grandparent_server, "vendor/llama.cpp/tools/server/public"),
        "vendor/llama.cpp/tools/server/webui",
        "./vendor/llama.cpp/tools/server/webui",
        "../vendor/llama.cpp/tools/server/webui",
        tools::FileOps::join_path(exe_dir_server, "vendor/llama.cpp/tools/server/webui"),
        tools::FileOps::join_path(exe_dir_server, "../vendor/llama.cpp/tools/server/webui"),
        tools::FileOps::join_path(exe_grandparent_server, "vendor/llama.cpp/tools/server/webui")
    };
    
    for (const auto& candidate : public_candidates_server) {
        if (tools::FileOps::dir_exists(candidate)) {
            std::string index_file = tools::FileOps::join_path(candidate, "index.html");
            if (tools::FileOps::file_exists(index_file)) {
                public_path_server = candidate;
                break;
            }
        }
    }
    
    // Convert relative path to absolute path for llama-server
    if (!public_path_server.empty() && public_path_server[0] != '/') {
        // Try multiple strategies to get absolute path
        char resolved_path[PATH_MAX];
        bool resolved = false;
        
        // Strategy 1: Try realpath on the path directly (if it exists from current dir)
        if (realpath(public_path_server.c_str(), resolved_path) != nullptr) {
            public_path_server = std::string(resolved_path);
            resolved = true;
        }
        
        // Strategy 2: Try relative to current working directory
        if (!resolved) {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                std::string full_path = tools::FileOps::join_path(std::string(cwd), public_path_server);
                if (realpath(full_path.c_str(), resolved_path) != nullptr) {
                    public_path_server = std::string(resolved_path);
                    resolved = true;
                }
            }
        }
        
        // Strategy 3: Try relative to executable directory
        if (!resolved) {
            std::string exe_based_path = tools::FileOps::join_path(exe_dir_server, public_path_server);
            if (realpath(exe_based_path.c_str(), resolved_path) != nullptr) {
                public_path_server = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // Strategy 4: Try relative to project root (exe_grandparent_server)
        if (!resolved) {
            std::string project_path = tools::FileOps::join_path(exe_grandparent_server, public_path_server);
            if (realpath(project_path.c_str(), resolved_path) != nullptr) {
                public_path_server = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // If all else fails, construct absolute path manually from exe_dir
        if (!resolved) {
            // Build absolute path from executable directory
            std::string abs_path = tools::FileOps::join_path(exe_dir_server, public_path_server);
            // Normalize the path (remove .. and .)
            char normalized[PATH_MAX];
            if (realpath(abs_path.c_str(), normalized) != nullptr) {
                public_path_server = std::string(normalized);
            } else {
                // Last resort: use exe_grandparent_server
                abs_path = tools::FileOps::join_path(exe_grandparent_server, public_path_server);
                if (realpath(abs_path.c_str(), normalized) != nullptr) {
                    public_path_server = std::string(normalized);
                }
            }
        }
    }
    
    // If not found, server will use default web UI (no --path flag)

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
    
    // Build command
    std::stringstream cmd;
    cmd << server_bin
        << " -m \"" << model_path << "\""
        << " --port " << port
        << " --parallel 4"  // default parallel
        << " -c " << ctx_size;
    
    // Add --alias flag to use short_name instead of filename in web UI
    if (!model_alias.empty()) {
        cmd << " --alias \"" << model_alias << "\"";
    }
    
    // Add --path flag to use llama.cpp web UI if found
    if (!public_path_server.empty()) {
        cmd << " --path \"" << public_path_server << "\"";
    }

    // Run server in background; silence stdout/stderr
    cmd << " >/dev/null 2>&1 &";
    std::system(cmd.str().c_str());
    UI::print_success("Delta Server started in background");
    std::string url = "http://localhost:" + std::to_string(port);
    UI::print_info("Open: " + url);
    UI::print_info("Context size: " + std::to_string(ctx_size));
    // Open browser after a short delay to ensure server is ready
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    if (tools::Browser::open_url(url)) {
        UI::print_info("Browser opened automatically");
    }
    return true;
}

bool Commands::handle_update(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    (void)session; // Suppress unused parameter warning
    UI::print_info("Updating Delta CLI...");
    
    UpdateManager updater;
    bool success = updater.perform_update();
    
    if (success) {
        UI::print_info("✓ Update completed successfully!");
        UI::print_info("Please restart Delta CLI to use the new version");
    } else {
        UI::print_error("✗ Update failed");
        UI::print_info("Please check your internet connection and try again");
    }
    
    return true;
}

bool Commands::handle_no_color(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args; // Suppress unused parameter warning
    session.no_color = !session.no_color;
    
    if (session.no_color) {
        UI::print_info("✓ Colored output disabled");
    } else {
        UI::print_info("✓ Colored output enabled");
    }
    
    return true;
}

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

bool Commands::handle_history(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args;
    (void)session;
    
    auto& history_mgr = get_history_manager();
    auto history = history_mgr.get_history();
    
    if (history.empty()) {
        UI::print_info("No conversation history found");
        return true;
    }
    
    UI::print_border("CONVERSATION HISTORY");
    
    // Display conversation history
    
    // Group entries by timestamp to pair user/assistant messages
    std::map<std::string, std::vector<HistoryEntry>> grouped_entries;
    for (const auto& entry : history) {
        if (!entry.timestamp.empty()) {
            grouped_entries[entry.timestamp].push_back(entry);
        }
    }
    
    // Display grouped entries
    for (const auto& group : grouped_entries) {
        std::string user_msg = "";
        std::string ai_resp = "";
        std::string model = "";
        
        for (const auto& entry : group.second) {
            if (!entry.user_message.empty()) {
                user_msg = entry.user_message;
            }
            if (!entry.ai_response.empty()) {
                ai_resp = entry.ai_response;
            }
            if (!entry.model_used.empty()) {
                model = entry.model_used;
            }
        }
        
        // Only display if we have actual content
        if (!user_msg.empty() || !ai_resp.empty()) {
            UI::print_history_entry(group.first, user_msg, ai_resp, model);
        }
    }
    
    return true;
}

bool Commands::handle_delete_history(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)session;
    
    if (args.empty()) {
        UI::print_error("Please specify what to delete");
        UI::print_info("Usage: /delete-history <all|id|day|week|year> [value]");
        UI::print_info("Examples:");
        UI::print_info("  /delete-history all");
        UI::print_info("  /delete-history 20250101_123456");
        UI::print_info("  /delete-history day 2025-01-23");
        return true;
    }
    
    auto& history_mgr = get_history_manager();
    std::string action = args[0];
    
    if (action == "all") {
        UI::print_warning("This will delete ALL conversation history!");
        std::cout << "Are you sure? (y/N): " << std::flush;
        
        std::string response;
        std::getline(std::cin, response);
        
        if (response == "y" || response == "Y" || response == "yes") {
            history_mgr.clear_history();
            UI::print_success("All conversation history deleted");
        } else {
            UI::print_info("Deletion cancelled");
        }
    } else if (action == "day" || action == "week" || action == "year") {
        if (args.size() < 2) {
            UI::print_error("Please specify a date for " + action + " deletion");
            return true;
        }
        
        std::string date = args[1];
        UI::print_warning("This will delete history from " + action + " " + date);
        std::cout << "Are you sure? (y/N): " << std::flush;
        
        std::string response;
        std::getline(std::cin, response);
        
        if (response == "y" || response == "Y" || response == "yes") {
            history_mgr.delete_history_by_date(action, date);
            UI::print_success("History deleted for " + action + " " + date);
        } else {
            UI::print_info("Deletion cancelled");
        }
    } else {
        // Assume it's an entry ID
        if (history_mgr.delete_history_entry(action)) {
            UI::print_success("History entry deleted");
        } else {
            UI::print_error("History entry not found");
        }
    }
    
    return true;
}

bool Commands::handle_new_session(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_error("Please specify a session name");
        UI::print_info("Usage: /new-session <name>");
        UI::print_info("Example: /new-session \"Project Ideas\"");
        return true;
    }
    
    std::string session_name = args[0];
    auto& history_mgr = get_history_manager();
    
    if (history_mgr.create_session(session_name, session.current_model)) {
        UI::print_success("Session '" + session_name + "' created");
        UI::print_info("Switched to session: " + session_name);
    } else {
        UI::print_error("Failed to create session '" + session_name + "'");
        UI::print_info("Session may already exist");
    }
    
    return true;
}

bool Commands::handle_list_sessions(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args;
    (void)session;
    
    auto& history_mgr = get_history_manager();
    auto sessions = history_mgr.list_sessions();
    
    if (sessions.empty()) {
        UI::print_info("No sessions found");
        return true;
    }
    
    UI::print_border("SAVED SESSIONS");
    for (const auto& session_name : sessions) {
        UI::print_info("• " + session_name);
    }
    
    std::string current = history_mgr.get_current_session();
    if (!current.empty()) {
        UI::print_info("Current session: " + current);
    }
    
    return true;
}

bool Commands::handle_switch_session(const std::vector<std::string>& args, InteractiveSession& session) {
    if (args.empty()) {
        UI::print_error("Please specify a session name");
        UI::print_info("Usage: /switch-session <name>");
        UI::print_info("Use 'default' to switch to the default session");
        UI::print_info("Use /list-sessions to see available sessions");
        return true;
    }
    
    std::string session_name = args[0];
    auto& history_mgr = get_history_manager();
    
    // Handle special case for default session
    if (session_name == "default") {
        if (history_mgr.switch_session("default")) {
            UI::print_success("Switched to default session");
            UI::print_info("All new conversations will be saved to the default session");
        } else {
            // Create default session if it doesn't exist
            if (history_mgr.create_session("default", session.current_model)) {
                UI::print_success("Created and switched to default session");
            } else {
                UI::print_error("Failed to create default session");
            }
        }
    } else {
        if (history_mgr.switch_session(session_name)) {
            UI::print_success("Switched to session: " + session_name);
            UI::print_info("Note: Delta will revert to default session on next startup");
        } else {
            UI::print_error("Session '" + session_name + "' not found");
            UI::print_info("Use /list-sessions to see available sessions");
        }
    }
    
    return true;
}

bool Commands::handle_delete_session(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)session;
    
    if (args.empty()) {
        UI::print_error("Please specify a session name");
        UI::print_info("Usage: /delete-session <name>");
        return true;
    }
    
    std::string session_name = args[0];
    auto& history_mgr = get_history_manager();
    
    if (session_name == history_mgr.get_current_session()) {
        UI::print_error("Cannot delete current session");
        UI::print_info("Switch to another session first");
        return true;
    }
    
    UI::print_warning("This will permanently delete session '" + session_name + "'");
    std::cout << "Are you sure? (y/N): " << std::flush;
    
    std::string response;
    std::getline(std::cin, response);
    
    if (response == "y" || response == "Y" || response == "yes") {
        if (history_mgr.delete_session(session_name)) {
            UI::print_success("Session '" + session_name + "' deleted");
        } else {
            UI::print_error("Failed to delete session '" + session_name + "'");
        }
    } else {
        UI::print_info("Deletion cancelled");
    }
    
    return true;
}

bool Commands::handle_active_session(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)args;
    (void)session;
    
    auto& history_mgr = get_history_manager();
    std::string session_info = history_mgr.get_session_info();
    
    UI::print_info("╔══════════════════════════════════════════════════════════════╗");
    UI::print_info("║                    ACTIVE SESSION INFO                       ║");
    UI::print_info("╚══════════════════════════════════════════════════════════════╝");
    
    // Split session info into lines and display
    std::istringstream iss(session_info);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            UI::print_info(line);
        }
    }
    
    return true;
}

bool Commands::handle_export_session(const std::vector<std::string>& args, InteractiveSession& session) {
    (void)session;
    
    if (args.empty()) {
        UI::print_error("Please specify a session name and format");
        UI::print_info("Usage: /export-session <session-name> <format>");
        UI::print_info("Formats: json, csv");
        UI::print_info("Example: /export-session formula-one json");
        return true;
    }
    
    if (args.size() < 2) {
        UI::print_error("Please specify both session name and format");
        UI::print_info("Usage: /export-session <session-name> <format>");
        UI::print_info("Formats: json, csv");
        return true;
    }
    
    std::string session_name = args[0];
    std::string format = args[1];
    
    // Validate format
    if (format != "json" && format != "csv") {
        UI::print_error("Invalid format. Use 'json' or 'csv'");
        return true;
    }
    
    auto& history_mgr = get_history_manager();
    
    // Check if session exists
    auto sessions = history_mgr.list_sessions();
    bool session_exists = false;
    for (const auto& s : sessions) {
        if (s == session_name) {
            session_exists = true;
            break;
        }
    }
    
    if (!session_exists) {
        UI::print_error("Session '" + session_name + "' not found");
        UI::print_info("Use /list-sessions to see available sessions");
        return true;
    }
    
    // Switch to the session temporarily to get its history
    std::string current_session = history_mgr.get_current_session_name();
    if (!history_mgr.switch_session(session_name)) {
        UI::print_error("Failed to switch to session '" + session_name + "'");
        return true;
    }
    
    auto history = history_mgr.get_history();
    
    // Generate filename
    std::string filename = session_name + "_export." + format;
    
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            UI::print_error("Failed to create export file: " + filename);
            return true;
        }
        
        if (format == "json") {
            // Export as JSON
            file << "[\n";
            bool first_entry = true;
            for (const auto& entry : history) {
                if (!first_entry) file << ",\n";
                
                // Export user message if exists
                if (!entry.user_message.empty()) {
                    file << "  {\n";
                    file << "    \"timestamp\": \"" << entry.timestamp << "\",\n";
                    file << "    \"role\": \"user\",\n";
                    file << "    \"content\": \"" << entry.user_message << "\",\n";
                    file << "    \"model\": \"" << entry.model_used << "\"\n";
                    file << "  }";
                    first_entry = false;
                }
                
                // Export AI response if exists
                if (!entry.ai_response.empty()) {
                    if (!first_entry) file << ",\n";
                    file << "  {\n";
                    file << "    \"timestamp\": \"" << entry.timestamp << "\",\n";
                    file << "    \"role\": \"assistant\",\n";
                    file << "    \"content\": \"" << entry.ai_response << "\",\n";
                    file << "    \"model\": \"" << entry.model_used << "\"\n";
                    file << "  }";
                    first_entry = false;
                }
            }
            file << "\n]\n";
        } else if (format == "csv") {
            // Export as CSV
            file << "timestamp,role,content,model\n";
            for (const auto& entry : history) {
                // Export user message if exists
                if (!entry.user_message.empty()) {
                    file << "\"" << entry.timestamp << "\",\"user\",\"" 
                         << entry.user_message << "\",\"" << entry.model_used << "\"\n";
                }
                
                // Export AI response if exists
                if (!entry.ai_response.empty()) {
                    file << "\"" << entry.timestamp << "\",\"assistant\",\"" 
                         << entry.ai_response << "\",\"" << entry.model_used << "\"\n";
                }
            }
        }
        
        file.close();
        
        UI::print_success("Session '" + session_name + "' exported to " + filename);
        UI::print_info("Exported " + std::to_string(history.size()) + " entries");
        
    } catch (const std::exception& e) {
        UI::print_error("Export failed: " + std::string(e.what()));
    }
    
    // Switch back to original session
    history_mgr.switch_session(current_session);
    
    return true;
}

} // namespace delta
