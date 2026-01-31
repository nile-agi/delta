/**
 * Delta CLI - Offline AI Assistant
 * Main entry point for the CLI application
 */

#include "delta_cli.h"
#include "update.h"
#include "commands.h"
#include "history.h"
#include <iostream>
#include <string>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #ifndef PATH_MAX
        #define PATH_MAX 260
    #endif
#else
    #include <unistd.h>
    #include <limits.h>
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
#endif
#include <algorithm>
#include <cctype>

using namespace delta;

// Command handlers
void print_help() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                      DELTA CLI v1.0.0                        ║
║              Offline AI Assistant — Delta CLI                ║
╚══════════════════════════════════════════════════════════════╝

USAGE:
    delta                    # Start interactive mode
    delta [OPTIONS] [PROMPT] # One-shot query or interactive mode
    delta pull <model-name>  # Download a model
    delta --server [-m <model>] [--port <N>] [--np <N>] [-c <N>]

COMMANDS:
    pull <model-name>       Download model from Hugging Face
                           Example: delta pull qwen2.5:0.5b
    remove <model-name>     Remove model from local cache
                           Example: delta remove qwen2.5:0.5b

OPTIONS:
    -h, --help              Show this help message
    -v, --version           Show version information
    -m, --model <MODEL>     Specify model (short name or full filename)
    -l, --list-models       List locally cached models
        --available         With -l, show available models to download
    -t, --tokens <N>        Max tokens to generate (default: 512)
    -T, --temperature <F>   Sampling temperature (default: 0.8)
    -c, --ctx-size <N>      Context size (default: 2048)
    -g, --gpu-layers <N>    Number of GPU layers (default: 0, use -1 for all)
    --multimodal            Enable multimodal mode (images + text)
    --interactive           Start interactive chat mode
    --server                Start Delta Server (OpenAI-compatible API)
        --port <N>              Server port (default: 8080)
         --np <N>                Max parallel requests (default: 4)
         --c <N>                 Max context size (default: from model registry, or model native)
         --models-dir <DIR>      Router mode: scan directory for .gguf (no -m; default: ~/.delta-cli/models)
         --embedding             Enable embedding endpoints
         --reranking             Enable reranking endpoints
         --md <model>            Draft model for speculative decoding
     --grammar-file <file>   Grammar file for output constraints
    --check-updates         Check for new versions
     --update                Update to latest version
     --no-color              Disable colored output

INTERACTIVE COMMANDS:
    /download <model>        Download a model
    /remove <model>          Remove a model (alias: /delete)
    /list                    List local models
    /available               List available models
    /use <model>             Switch to another model
    /clear-screen            Clear the terminal screen
    /help                    Show interactive commands

EXAMPLES:
    # Download a model
    delta pull qwen2.5:0.5b
    delta pull llama3.1:8b
    
    # Remove a model
    delta remove qwen2.5:0.5b
    delta -r llama3.1:8b
    
    # List models
    delta --list-models
    delta --list-models --available
    
    # Use a model (SHORT NAMES - much easier!)
    delta --model qwen2.5-0.5b "Explain quantum computing"
    delta --model llama3.1-8b --gpu-layers -1 "Write a poem"
    delta --model mistral-7b --interactive
    
    # Updates
    delta --check-updates
    delta --update
    delta --server

For more information, visit: https://github.com/nile-agi/delta
)" << std::endl;
}

void print_version() {
    std::cout << "Delta CLI v1.0.0" << std::endl;
    std::cout << "Professional offline AI assistant" << std::endl;
}

void interactive_mode(InferenceEngine& engine, InferenceConfig& config, ModelManager& model_mgr, const std::string& current_model) {
    // Initialize command system
    Commands::init();
    
    // Initialize history manager (default session is already enforced in constructor)
    auto& history_mgr = get_history_manager();
    
    // CRITICAL: Load existing history from disk for persistence (only once)
    if (!history_mgr.is_history_loaded()) {
        history_mgr.load_history_from_disk();
    }
    
    // Create session state
    InteractiveSession session;
    session.engine = &engine;
    session.config = &config;
    session.model_mgr = &model_mgr;
    session.current_model = current_model;
    session.max_tokens = 256;
    session.temperature = config.temperature;
    session.gpu_layers = config.n_gpu_layers;
    session.multimodal = config.multimodal;
    session.no_color = false;
    
    UI::init();
    UI::print_info("Interactive mode - Type 'exit' or 'quit' to end session");
    UI::print_info("Type '/help' for available commands");
    
    // Automatically launch web UI server with default model
    std::string model_path = model_mgr.get_model_path(current_model);
    if (!model_path.empty()) {
        int ctx_size = model_mgr.get_max_context_for_model(current_model);
        if (ctx_size <= 0 && config.n_ctx > 0) {
            ctx_size = config.n_ctx;
        }
        
        // Get name (with colon) for model alias in web UI
        // Priority: 1) Lookup by current_model, 2) Lookup by filename from model_path
        std::string model_alias;
        
        // First, try to get name from filename (most reliable since model_path is always correct)
        std::string filename = model_path;
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }
        std::string found_name = model_mgr.get_name_from_filename(filename);
        if (!found_name.empty()) {
            model_alias = found_name;  // Use name (e.g., "qwen3:0.6b") from filename lookup
        } else {
            // Fallback: try to lookup by current_model (might be short_name or registry name)
            std::string registry_name_for_alias = current_model;
            if (model_mgr.is_in_registry(registry_name_for_alias)) {
                auto entry = model_mgr.get_registry_entry(registry_name_for_alias);
                if (!entry.name.empty()) {
                    model_alias = entry.name;  // Use name (e.g., "qwen3:0.6b") instead of short_name
                }
            } else {
                // Try converting dash to colon format (e.g., "qwen3-0.6b" -> "qwen3:0.6b")
                size_t last_dash = registry_name_for_alias.find_last_of('-');
                if (last_dash != std::string::npos) {
                    std::string colon_name = registry_name_for_alias.substr(0, last_dash) + ":" + 
                                             registry_name_for_alias.substr(last_dash + 1);
                    if (model_mgr.is_in_registry(colon_name)) {
                        auto entry = model_mgr.get_registry_entry(colon_name);
                        if (!entry.name.empty()) {
                            model_alias = entry.name;  // Use name (e.g., "qwen3:0.6b") instead of short_name
                        }
                    }
                }
            }
            
            // Last resort: use short_name from filename
            if (model_alias.empty()) {
                model_alias = model_mgr.get_short_name_from_filename(filename);
            }
        }
        
        // Try to launch server - if it fails, it's okay (server might not be built)
        // launch_server_auto now waits for server to be ready before returning
        if (Commands::launch_server_auto(model_path, 8080, ctx_size, model_alias)) {
            // Server is confirmed listening, get the actual port used
            int actual_port = Commands::get_current_port();
            std::string url = "http://localhost:" + std::to_string(actual_port) + "/index.html";
            // Open browser now that server is confirmed ready
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (tools::Browser::open_url(url)) {
                UI::print_info("Browser opened automatically");
            }
        } else {
            UI::print_error("Server failed to start. Check the error messages above.");
        }
        // If launch fails, don't show error - server is optional
    } else {
        // Model not found - can't start server
    }
    
    // Show session info only if not default (to avoid duplicate messages)
    std::string current_session_name = history_mgr.get_current_session_name();
    if (!history_mgr.is_default_session_active()) {
        UI::print_info("Current session: " + current_session_name);
    }
    
    std::cout << std::endl;
    
    // Do not accumulate or inject prior turns into prompts; rely on clean llama context per turn
    std::vector<std::string> history;
    
    while (true) {
        UI::print_prompt();
        std::string input = UI::get_input();
        
        // Check if input stream is exhausted (e.g., when piping input)
        if (std::cin.eof()) {
            UI::print_info("Input stream ended. Exiting interactive mode.");
            // Save history before exiting
            history_mgr.save_current_session();
            break;
        }
        
        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t\n\r"));
        input.erase(input.find_last_not_of(" \t\n\r") + 1);
        
        if (input.empty()) continue;
        
        // Handle basic commands
        if (input == "exit" || input == "quit") {
            UI::print_info("Goodbye!");
            // Save history before exiting
            history_mgr.save_current_session();
            break;
        } else if (input == "help") {
            Commands::show_help();
            continue;
        }
        
        // Handle slash commands
        if (input[0] == '/') {
            // Remove the leading slash
            std::string command = input.substr(1);
            
            // Process command
            if (Commands::process_command(command, session)) {
                continue; // Command was handled, don't process as AI prompt
            }
        }
        
        // Check if model is loaded before processing text input
        if (!engine.is_loaded()) {
            // No model loaded - show helpful message
            UI::print_info("Interactive mode - Type 'exit' or 'quit' to end session");
            UI::print_info("Type '/help' for available commands");
            continue;
        }
        
        // Build prompt with only the current user input
        history.push_back(input);
        
        // Generate response using session settings
        try {
            std::cout << "\n";
            
            // Use clean prompt without modifications
            std::string simple_prompt = input;
            
            // Generate response with real-time streaming
            // Use very short max_tokens for concise responses
            int max_tokens = std::min(session.max_tokens, 50);
            std::string response = engine.generate(simple_prompt, max_tokens, true);
            
            // Clean up the response
            response.erase(0, response.find_first_not_of(" \t\n\r"));
            response.erase(response.find_last_not_of(" \t\n\r") + 1);
            
            // Basic cleanup - just trim whitespace
            // Let the model's natural response come through
            
            std::cout << "\n" << std::endl;
            
            // Save to history (no injection back into prompt state)
            history_mgr.add_entry(input, response, session.current_model);
        } catch (const std::exception& e) {
            UI::print_error(std::string("Error generating response: ") + e.what());
        }
    }
}

void list_models(ModelManager& model_mgr, bool show_available = false) {
    // Get friendly model list with new structured format
    auto models = model_mgr.get_friendly_model_list(show_available);
    
    if (models.empty()) {
        if (show_available) {
            UI::print_error("No models available in registry");
        } else {
            UI::print_info("No models found locally.");
            UI::print_info("Download a model with: delta pull <model-name>");
            UI::print_info("See available models: delta --list-models --available");
        }
        return;
    }
    
    if (show_available) {
        UI::print_border("Available Models to Download");
        UI::print_info("Use 'delta pull <model-name>' to download");
        std::cout << std::endl;
        
        for (const auto& model : models) {
            std::string status = model.installed ? "[✓ Installed]" : "[ Download  ]";
            
            std::cout << "  " << status << " " << model.name << std::endl;
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
    } else {
        UI::print_border("Locally Cached Models");
        
        for (const auto& model : models) {
            std::cout << "  • [ Installed ] " << model.name << std::endl;
            std::cout << "      " << model.display_name << " - " << model.description << std::endl;
            std::cout << "      Size: " << model.size_str << " | Quant: " << model.quantization << std::endl;
            std::cout << std::endl;
        }
        
        UI::print_info("Use 'delta --model " + models[0].name + "' to use a model");
        UI::print_info("Example with short name: delta --model " + models[0].name + " \"your prompt\"");
    }
}

// Progress bar for model downloads
void download_progress_callback(double progress, long long current, long long total) {
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
}

int main(int argc, char** argv) {
    // Parse command line arguments
    InferenceConfig config;
    std::string model_name = "";
    std::string prompt = "";
    std::string pull_model_name = "";
    std::string remove_model_name = "";
    bool interactive = false;
    bool show_help = false;
    bool show_version = false;
    bool show_models = false;
    bool show_available = false;
    bool start_server = false;
    bool check_updates = false;
    bool do_update = false;
    bool is_pull_command = false;
    bool is_remove_command = false;
    bool no_args = (argc == 1);  // No arguments provided
    int max_tokens = 256;
    int server_port = 8080;
    int max_parallel = 4;
    int max_context = 0;  // 0 = use model default from registry when launching server
    bool max_context_explicit = false;  // Track if --c was explicitly set
    std::string models_dir = "";  // Router mode: scan this dir for .gguf (no -m)
    // Server-only flags (parsed for compatibility; unused in CLI mode)
    bool enable_embedding = false;
    bool enable_reranking = false;
    std::string draft_model = "";
    std::string grammar_file = "";
    
    // Check for pull command first
    if (argc > 1 && std::string(argv[1]) == "pull") {
        is_pull_command = true;
        if (argc > 2) {
            pull_model_name = argv[2];
        }
    }
    
    // Check for remove command
    if (argc > 1 && (std::string(argv[1]) == "remove" || std::string(argv[1]) == "-r")) {
        is_remove_command = true;
        if (argc > 2) {
            remove_model_name = argv[2];
        }
    }
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            show_help = true;
        } else if (arg == "-v" || arg == "--version") {
            show_version = true;
        } else if (arg == "-l" || arg == "--list-models") {
            show_models = true;
        } else if (arg == "-a" || arg == "--available") {
            show_available = true;
        } else if (arg == "-i" || arg == "--interactive") {
            interactive = true;
        } else if (arg == "--server") {
            start_server = true;
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                server_port = std::stoi(argv[++i]);
                if (server_port < 1024 || server_port > 65535) {
                    UI::print_error("Port must be between 1024 and 65535");
                    return 1;
                }
            } catch (const std::exception&) {
                UI::print_error("Invalid port number: " + std::string(argv[i]));
                return 1;
            }
        } else if (arg == "--np" && i + 1 < argc) {
            try {
                max_parallel = std::stoi(argv[++i]);
                if (max_parallel < 1 || max_parallel > 16) {
                    UI::print_error("Max parallel must be between 1 and 16");
                    return 1;
                }
            } catch (const std::exception&) {
                UI::print_error("Invalid max parallel number: " + std::string(argv[i]));
                return 1;
            }
        } else if (arg == "--c" && i + 1 < argc) {
            try {
                max_context = std::stoi(argv[++i]);
                max_context_explicit = true;
                if (max_context != 0 && (max_context < 512 || max_context > 32768)) {
                    UI::print_error("Max context must be 0 (model default) or between 512 and 32768");
                    return 1;
                }
            } catch (const std::exception&) {
                UI::print_error("Invalid max context number: " + std::string(argv[i]));
                return 1;
            }
        } else if (arg == "--embedding") {
            enable_embedding = true;
        } else if (arg == "--reranking") {
            enable_reranking = true;
        } else if (arg == "--md" && i + 1 < argc) {
            draft_model = argv[++i];
        } else if (arg == "--grammar-file" && i + 1 < argc) {
            grammar_file = argv[++i];
        } else if (arg == "--models-dir" && i + 1 < argc) {
            models_dir = argv[++i];
        } else if (arg == "--check-updates") {
            check_updates = true;
        } else if (arg == "--update") {
            do_update = true;
        } else if (arg == "--multimodal") {
            config.multimodal = true;
        } else if (arg == "--no-color") {
            // Disable color (would set a global flag in UI class)
        } else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) {
                model_name = argv[++i];
            }
        } else if (arg == "-t" || arg == "--tokens") {
            if (i + 1 < argc) {
                max_tokens = std::atoi(argv[++i]);
            }
        } else if (arg == "-T" || arg == "--temperature") {
            if (i + 1 < argc) {
                config.temperature = std::atof(argv[++i]);
            }
        } else if (arg == "-c" || arg == "--ctx-size") {
            if (i + 1 < argc) {
                config.n_ctx = std::atoi(argv[++i]);
            }
        } else if (arg == "-g" || arg == "--gpu-layers") {
            if (i + 1 < argc) {
                config.n_gpu_layers = std::atoi(argv[++i]);
            }
        } else if (arg == "pull") {
            // Skip - already handled above
            continue;
        } else if (!arg.empty() && arg[0] == '-') {
            // Unknown flag - might be a typo
            if (!show_help && !show_version && !show_models && !interactive && !start_server && 
                !check_updates && !do_update && !config.multimodal) {
                // Suggest corrections for common typos
                UI::print_error("Unknown option: " + arg);
                
                // Suggest corrections
                if (arg == "--check-update") {
                    UI::print_info("Did you mean '--check-updates'?");
                } else if (arg == "--updates") {
                    UI::print_info("Did you mean '--check-updates' or '--update'?");
                } else if (arg == "--list-model") {
                    UI::print_info("Did you mean '--list-models' (with 's')?");
                } else if (arg == "-a") {
                    UI::print_info("Did you mean '--available'? (use with --list-models)");
                } else if (arg.find("--model") != std::string::npos && arg != "--model") {
                    UI::print_info("Did you mean '--model <MODEL_NAME>'?");
                } else {
                    UI::print_info("Run 'delta --help' to see available options");
                }
                return 1;
            }
        } else if (!arg.empty() && arg[0] != '-' && !is_pull_command) {
            // Treat as prompt (but not if it's the model name for pull)
            if (!prompt.empty()) prompt += " ";
            prompt += arg;
        }
    }
    
    // Handle pull command
    if (is_pull_command) {
        if (pull_model_name.empty()) {
            UI::print_error("Please specify a model name");
            UI::print_info("Usage: delta pull <model-name>");
            UI::print_info("Example: delta pull qwen2.5:0.5b");
            UI::print_info("See available models: delta --list-models --available");
            return 1;
        }
        
        UI::init();
        ModelManager model_mgr;
        model_mgr.set_progress_callback(download_progress_callback);
        
        bool success = model_mgr.pull_model(pull_model_name);
        return success ? 0 : 1;
    }
    
    // Handle remove command
    if (is_remove_command) {
        if (remove_model_name.empty()) {
            UI::print_error("Please specify a model name");
            UI::print_info("Usage: delta remove <model-name>");
            UI::print_info("Example: delta remove qwen2.5:0.5b");
            UI::print_info("See installed models: delta --list-models");
            return 1;
        }
        
        UI::init();
        ModelManager model_mgr;
        
        bool success = model_mgr.remove_model_with_confirmation(remove_model_name);
        return success ? 0 : 1;
    }
    
    // Handle help/version/list-models
    if (show_help) {
        print_help();
        return 0;
    }
    
    if (show_version) {
        print_version();
        return 0;
    }
    
    // Handle update commands
    if (check_updates) {
        UI::init();
        UpdateManager updater;
        bool has_update = updater.check_for_updates(true);
        return has_update ? 1 : 0;  // Exit code 1 if update available
    }
    
    if (do_update) {
        UI::init();
        UpdateManager updater;
        bool success = updater.perform_update();
        return success ? 0 : 1;
    }
    
    // Initialize UI
    UI::init();
    
    // Handle no-args case: Display banner first, then start interactive mode
    if (no_args) {
        UI::print_banner();
        std::cout << std::endl;
    }
    
    // Handle first-time authentication
    Auth auth;
    if (auth.is_first_run()) {
        auth.handle_first_run();
    }
    
    // Initialize model manager
    ModelManager model_mgr;
    
    if (show_models) {
        list_models(model_mgr, show_available);
        return 0;
    }
    
    // Handle server mode: spawn bundled delta-server (single-model with -m, or router mode with --models-dir)
    if (start_server) {
        bool use_router_mode = false;
        std::string model_path;
        std::string model_alias;
        int server_ctx = max_context;

        if (model_name.empty()) {
            // No -m: router mode with --models-dir (user-provided or default)
            if (models_dir.empty()) {
                models_dir = tools::FileOps::join_path(tools::FileOps::get_home_dir(), ".delta-cli");
                models_dir = tools::FileOps::join_path(models_dir, "models");
            }
            use_router_mode = true;
        } else if (!models_dir.empty()) {
            // User passed both -m and --models-dir: prefer single-model (-m)
            use_router_mode = false;
        }
        if (!use_router_mode) {
            // Single-model: resolve model
            if (model_name.empty()) {
                model_name = model_mgr.get_auto_selected_model();
            }
            if (!model_mgr.is_model_installed(model_name)) {
                UI::print_error("Model not found: " + model_name);
                UI::print_info("Use 'delta pull " + model_name + "' to download it");
                return 1;
            }
            model_path = model_mgr.get_model_path(model_name);
            if (model_path.empty()) {
                UI::print_error("Could not resolve model path for: " + model_name);
                return 1;
            }
            if (!max_context_explicit) {
                server_ctx = model_mgr.get_max_context_for_model(model_name);
            }
            // Resolve model alias for web UI
            std::string filename = model_path;
            size_t last_slash = filename.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                filename = filename.substr(last_slash + 1);
            }
            std::string found_name = model_mgr.get_name_from_filename(filename);
            if (!found_name.empty()) {
                model_alias = found_name;
            } else {
                std::string registry_name_for_alias = model_name;
                if (model_mgr.is_in_registry(registry_name_for_alias)) {
                    auto entry = model_mgr.get_registry_entry(registry_name_for_alias);
                    if (!entry.name.empty()) model_alias = entry.name;
                }
                if (model_alias.empty()) {
                    model_alias = model_mgr.get_short_name_from_filename(filename);
                }
            }
        }

        // Find delta-server binary with comprehensive cross-platform search
        std::string exe_dir = tools::FileOps::get_executable_dir();
        std::vector<std::string> server_candidates;
        
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
                server_candidates.push_back(dev_path);
                server_candidates.push_back("./" + dev_path);
            }
        }
        
        // Then check relative to executable directory
#ifdef _WIN32
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server.exe"));
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "build_windows/delta-server.exe"));
#elif defined(__APPLE__)
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server"));
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "build_macos/delta-server"));
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "build_ios/delta-server"));
#else
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server"));
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "build_linux/delta-server"));
        server_candidates.push_back(tools::FileOps::join_path(exe_dir, "build_android/delta-server"));
#endif
        
        // Check common installation locations
        std::string install_locations[] = {
            "/usr/local/bin/delta-server",
            "/usr/bin/delta-server",
            "/opt/delta-cli/bin/delta-server",
            tools::FileOps::join_path(tools::FileOps::get_home_dir(), ".local/bin/delta-server")
        };
        
        for (const auto& install_path : install_locations) {
            server_candidates.push_back(install_path);
#ifdef _WIN32
            server_candidates.push_back(install_path + ".exe");
#endif
        }
        
        // Check system PATH
        server_candidates.push_back("delta-server");
#ifdef _WIN32
        server_candidates.push_back("delta-server.exe");
#endif
        
        std::string server_bin = "";
        for (const auto &cand : server_candidates) {
            if (tools::FileOps::file_exists(cand)) { server_bin = cand; break; }
        }
        if (server_bin.empty()) {
            UI::print_error("delta-server binary not found. Build it first.");
#ifdef _WIN32
            UI::print_info("Run: installers/build_windows.bat");
#elif defined(__APPLE__)
            UI::print_info("Run: installers/build_macos.sh");
#else
            UI::print_info("Run: installers/build_linux.sh");
#endif
            return 1;
        }

        // Get the path to the web UI directory (use Delta web UI from public/ only)
        std::string public_path;
        std::string exe_parent = tools::FileOps::join_path(exe_dir, "..");
        std::string exe_grandparent = tools::FileOps::join_path(exe_parent, "..");
        
        // Check for Delta web UI in public/ directory (built from assets/)
        // Priority: Homebrew share directory, then relative to executable, then current directory
        std::vector<std::string> public_candidates = {
            // Homebrew installed location (priority)
            "/opt/homebrew/share/delta-cli/webui",
            "/usr/local/share/delta-cli/webui",
            tools::FileOps::join_path(exe_dir, "../../share/delta-cli/webui"),
            tools::FileOps::join_path(exe_dir, "../../../share/delta-cli/webui"),
            // macOS app bundle Resources directory (for DMG installs)
            // Executable is at Contents/MacOS/delta, web UI is at Contents/Resources/webui
            tools::FileOps::join_path(exe_dir, "../Resources/webui"),
            tools::FileOps::join_path(exe_dir, "../../Resources/webui"),
            // Relative to executable (Delta web UI from public/)
            tools::FileOps::join_path(exe_dir, "../public"),
            tools::FileOps::join_path(exe_dir, "../../public"),
            tools::FileOps::join_path(exe_grandparent, "public"),
            // Current directory
            "public",
            "./public",
            "../public"
        };
        
        for (const auto& candidate : public_candidates) {
            if (tools::FileOps::dir_exists(candidate)) {
                std::string index_file = tools::FileOps::join_path(candidate, "index.html.gz");
                if (tools::FileOps::file_exists(index_file) || 
                    tools::FileOps::file_exists(tools::FileOps::join_path(candidate, "index.html"))) {
                    public_path = candidate;
                    break;
                }
            }
        }
        
        // Convert relative path to absolute path
#ifdef _WIN32
        if (!public_path.empty() && !(public_path.length() >= 2 && public_path[1] == ':')) {
#else
        if (!public_path.empty() && public_path[0] != '/') {
#endif
#ifdef _WIN32
            char resolved_path[MAX_PATH];
            bool resolved = false;
            
            // Try _fullpath (Windows equivalent of realpath)
            if (_fullpath(resolved_path, public_path.c_str(), MAX_PATH) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
            
            if (!resolved) {
                char cwd[MAX_PATH];
                if (_getcwd(cwd, MAX_PATH) != nullptr) {
                    std::string full_path = tools::FileOps::join_path(std::string(cwd), public_path);
                    if (_fullpath(resolved_path, full_path.c_str(), MAX_PATH) != nullptr) {
                        public_path = std::string(resolved_path);
                        resolved = true;
                    }
                }
            }
            
            if (!resolved) {
                std::string exe_based_path = tools::FileOps::join_path(exe_dir, public_path);
                if (_fullpath(resolved_path, exe_based_path.c_str(), MAX_PATH) != nullptr) {
                    public_path = std::string(resolved_path);
                    resolved = true;
                }
            }
            
            if (!resolved) {
                std::string project_path = tools::FileOps::join_path(exe_grandparent, public_path);
                if (_fullpath(resolved_path, project_path.c_str(), MAX_PATH) != nullptr) {
                    public_path = std::string(resolved_path);
                }
            }
#else
            char resolved_path[PATH_MAX];
            bool resolved = false;
            
            if (realpath(public_path.c_str(), resolved_path) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
            
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
            
            if (!resolved) {
                std::string exe_based_path = tools::FileOps::join_path(exe_dir, public_path);
                if (realpath(exe_based_path.c_str(), resolved_path) != nullptr) {
                    public_path = std::string(resolved_path);
                    resolved = true;
                }
            }
            
            if (!resolved) {
                std::string project_path = tools::FileOps::join_path(exe_grandparent, public_path);
                if (realpath(project_path.c_str(), resolved_path) != nullptr) {
                    public_path = std::string(resolved_path);
                }
            }
#endif
        }

        // Build command (router mode: --models-dir only; single-model: -m model_path)
        std::stringstream cmd;
        cmd << server_bin;
        if (use_router_mode) {
            cmd << " --models-dir \"" << models_dir << "\"";
        } else {
            cmd << " -m \"" << model_path << "\"";
        }
        cmd << " --port " << server_port
            << " --parallel " << max_parallel;
        if (server_ctx > 0) {
            cmd << " -c " << server_ctx;
        }
        
        // Add --flash-attn flag for single-model only (router mode uses server defaults)
        if (!use_router_mode) {
            if (server_ctx > 16384) {
                cmd << " --flash-attn off";
                if (server_ctx > 32768) {
                    cmd << " --gpu-layers 0";
                }
            } else {
                cmd << " --flash-attn auto";
            }
        }
        
        // Add --jinja flag for gemma3 models (single-model only)
        if (!use_router_mode) {
        std::string model_name_lower = model_name;
        std::string model_alias_lower = model_alias;
        std::string model_path_lower = model_path;
        std::transform(model_name_lower.begin(), model_name_lower.end(), model_name_lower.begin(), ::tolower);
        std::transform(model_alias_lower.begin(), model_alias_lower.end(), model_alias_lower.begin(), ::tolower);
        std::transform(model_path_lower.begin(), model_path_lower.end(), model_path_lower.begin(), ::tolower);
        if (model_name_lower.find("gemma3") != std::string::npos ||
            model_alias_lower.find("gemma3") != std::string::npos ||
            model_path_lower.find("gemma3") != std::string::npos) {
            cmd << " --jinja";
        }
        }
        
        if (enable_embedding) cmd << " --embedding";
        if (enable_reranking) cmd << " --reranking";
        if (!draft_model.empty()) cmd << " --md \"" << draft_model << "\"";
        if (!grammar_file.empty()) cmd << " --grammar-file \"" << grammar_file << "\"";
        
        // Add --alias flag to use short_name instead of filename in web UI (single-model only)
        if (!use_router_mode && !model_alias.empty()) {
            cmd << " --alias \"" << model_alias << "\"";
        }
        
        // Add --path flag to use Delta web UI from public/ if found
        if (!public_path.empty()) {
            cmd << " --path \"" << public_path << "\"";
        }

        // Run server in background and silence stdout/stderr
        cmd << " >/dev/null 2>&1 &";
        int rc = std::system(cmd.str().c_str());
        if (rc == -1) {
            UI::print_error("Failed to start server process");
            return 1;
        }
        UI::print_success(use_router_mode ? "Delta Server started in background (router mode)" : "Delta Server started in background");
        std::string url = "http://localhost:" + std::to_string(server_port) + "/index.html";
        UI::print_info("Open: " + url);
        // Open browser after a short delay to ensure server is ready
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (tools::Browser::open_url(url)) {
            UI::print_info("Browser opened automatically");
        }
        return 0;
    }
    
    // Find and load model
    if (model_name.empty()) {
        // Try to get an auto-selected model
        model_name = model_mgr.get_auto_selected_model();
        
        // Check if any model is actually installed
        if (!model_mgr.is_model_installed(model_name)) {
            // No models installed - try to download default model
            if (no_args) {
                UI::print_info("No models installed. Downloading default model...");
                std::cout << std::endl;
            } else {
                UI::print_info("No models installed. Attempting to download default model...");
                std::cout << std::endl;
            }
            
            // Set progress callback for download
            model_mgr.set_progress_callback(download_progress_callback);
            bool success = model_mgr.ensure_default_model_installed(download_progress_callback);
            model_mgr.set_progress_callback(nullptr);
            
            if (!success) {
                UI::print_error("Failed to install default model");
                UI::print_info("This might be due to:");
                UI::print_info("  • No internet connection");
                UI::print_info("  • Network timeout");
                UI::print_info("  • Insufficient disk space");
                std::cout << std::endl;
                
                // Check if any other models are installed as fallback
                auto installed_models = model_mgr.get_friendly_model_list(false);
                if (!installed_models.empty()) {
                    UI::print_info("Found " + std::to_string(installed_models.size()) + " installed model(s) as fallback:");
                    for (const auto& model : installed_models) {
                        UI::print_info("  • " + model.name + " (" + model.size_str + ")");
                    }
                    std::cout << std::endl;
                    UI::print_info("Using fallback model: " + installed_models[0].name);
                    model_name = installed_models[0].name;
                } else {
                    UI::print_info("No models available. Please:");
                    UI::print_info("  1. Check your internet connection and try: delta pull " + ModelManager::get_default_model());
                    UI::print_info("  2. Or see available models: delta --list-models --available");
                    UI::print_info("  3. Or install manually from Hugging Face");
                    return 1;
                }
            }
            
            // Use the default model that was just downloaded
            model_name = model_mgr.get_default_model_short_name();
        }
        
        if (!no_args) {
            UI::print_info("Auto-selecting model: " + model_name);
        }
    }
    
    std::string model_path = model_mgr.get_model_path(model_name);
    if (model_path.empty()) {
        UI::print_error("Model not found: " + model_name);
        UI::print_error("Use --list-models to see available models");
        return 1;
    }
    
    config.model_path = model_path;
    
    // Initialize inference engine
    InferenceEngine engine;
    
    // For no-args case: don't load model, only allow commands
    // For other cases: load model normally
    if (no_args) {
        // Don't load model - interactive mode will only allow commands
        // Model will be loaded when user uses /use command
    } else {
        if (!interactive && !prompt.empty()) {
        UI::print_info("Loading model: " + model_name);
    }
    
    if (!engine.load_model(config)) {
        UI::print_error("Failed to load model");
        return 1;
        }
    }
    
    // Interactive mode
    if (interactive || prompt.empty() || no_args) {
        // For no-args case, start interactive mode directly without extra output
        if (no_args) {
            // Interactive mode will handle its own initialization
        } else if (prompt.empty()) {
            std::cout << std::endl;
        }
        interactive_mode(engine, config, model_mgr, model_name);
        return 0;
    }
    
    // Single prompt mode
    if (!no_args) {
        UI::print_banner();
        std::cout << "\n";
        UI::print_info("Generating response...");
        std::cout << "\n";
    }
    
    
    try {
        std::string response = engine.generate(prompt, max_tokens, true);
        std::cout << "\n" << std::endl;
        
        // Ensure response is displayed (fallback for non-streaming)
        if (response.empty()) {
            UI::print_warning("No response generated");
        }
    } catch (const std::exception& e) {
        UI::print_error(std::string("Error: ") + e.what());
        return 1;
    }
    
    return 0;
}