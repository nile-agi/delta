/**
 * Interactive Commands - Slash command system for Delta CLI
 */

#ifndef DELTA_COMMANDS_H
#define DELTA_COMMANDS_H

#include "delta_cli.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

#if defined(_WIN32) || defined(_MSC_VER)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <windows.h>
    typedef DWORD process_id_t;
#else
    #include <unistd.h>
    typedef pid_t process_id_t;
#endif

namespace delta {

// Forward declarations
struct InteractiveSession;

// Command handler function type
typedef bool (*CommandHandler)(const std::vector<std::string>& args, InteractiveSession& session);

// Session state for interactive mode
struct InteractiveSession {
    InferenceEngine* engine;
    InferenceConfig* config;
    ModelManager* model_mgr;
    std::string current_model;
    int max_tokens;
    double temperature;
    int gpu_layers;
    bool multimodal;
    bool no_color;
    
    InteractiveSession() : engine(nullptr), config(nullptr), model_mgr(nullptr),
                          current_model(""), max_tokens(512), temperature(0.8),
                          gpu_layers(0), multimodal(false), no_color(false) {}
};

class Commands {
public:
    // Initialize command system
    static void init();
    
    // Process slash command
    static bool process_command(const std::string& input, InteractiveSession& session);
    
    // Launch server automatically (for auto-start on delta launch)
    static bool launch_server_auto(const std::string& model_path, int port = 8080, int ctx_size = 4096, const std::string& model_alias = "");
    
    // Check if a port is available
    static bool is_port_available(int port);
    
    // Find an available port (tries default, then fallback ports)
    static int find_available_port(int preferred_port = 8080);
    
    // Restart llama-server with new model (for model switching)
    static bool restart_llama_server(const std::string& model_path, const std::string& model_name, int ctx_size, const std::string& model_alias);
    
    // Stop llama-server
    static void stop_llama_server();
    
    // Command handlers
    static bool handle_download(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_remove(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_list(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_use(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_available(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_clear_screen(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_help(const std::vector<std::string>& args, InteractiveSession& session);
    
    // Utility functions
    static std::vector<std::string> parse_args(const std::string& input);
    static void show_help();
    static bool is_online_command(const std::string& command);
    static void show_offline_message(const std::string& command);
    
private:
    static std::map<std::string, CommandHandler> command_map_;
    static bool initialized_;
    
    // Process management for llama-server
    static process_id_t llama_server_pid_;
    static std::string current_model_path_;
    static int current_port_;
    static std::mutex server_mutex_;
    
    // Helper to build llama-server command
    static std::string build_llama_server_cmd(const std::string& server_bin, const std::string& model_path, 
                                               int port, int ctx_size, const std::string& model_alias, 
                                               const std::string& public_path);
};

} // namespace delta

#endif // DELTA_COMMANDS_H
