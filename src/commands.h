/**
 * Interactive Commands - Slash command system for Delta CLI
 */

#ifndef DELTA_COMMANDS_H
#define DELTA_COMMANDS_H

#include "delta_cli.h"
#include <string>
#include <vector>
#include <map>

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
    static bool launch_server_auto(const std::string& model_path, int port = 8080, int ctx_size = 4096);
    
    // Command handlers
    static bool handle_download(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_remove(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_list(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_updates(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_use(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_version(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_available(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_tokens(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_temperature(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_gpu_layers(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_multimodal(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_interactive(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_server(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_update(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_no_color(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_help(const std::vector<std::string>& args, InteractiveSession& session);
    
    // History and session management commands
    static bool handle_clear_screen(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_history(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_delete_history(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_new_session(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_list_sessions(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_switch_session(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_delete_session(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_active_session(const std::vector<std::string>& args, InteractiveSession& session);
    static bool handle_export_session(const std::vector<std::string>& args, InteractiveSession& session);
    
    // Utility functions
    static std::vector<std::string> parse_args(const std::string& input);
    static void show_help();
    static bool is_online_command(const std::string& command);
    static void show_offline_message(const std::string& command);
    
private:
    static std::map<std::string, CommandHandler> command_map_;
    static bool initialized_;
};

} // namespace delta

#endif // DELTA_COMMANDS_H
