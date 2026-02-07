#ifndef DELTA_CLI_H
#define DELTA_CLI_H

#include <string>
#include <vector>
#include <map>
#include <list>

// Forward declarations for llama.cpp types
struct llama_model;
struct llama_context;
struct llama_sampler;

namespace delta {

// ============================================================================
// UI Module - Retro green terminal styling
// ============================================================================
class UI {
public:
    static void init();
    static void print_banner();
    static void print_prompt();
    static void print_response(const std::string& text);
    static void print_error(const std::string& error);
    static void print_info(const std::string& info);
    static void print_warning(const std::string& warning);
    static void print_success(const std::string& success);
    static void print_border(const std::string& title = "");
    static void clear_line();
    static std::string get_input();
    
    // Internationalization support
    static std::string format_size(long long bytes);
    static std::string format_number(long long number);
    static void print_utf8(const std::string& text);
    static void print_multilingual_info(const std::string& key, const std::string& value);
    static void print_multilingual_welcome();
    static std::string get_system_language();
    
    // History and session display
    static void clear_screen();
    static void print_history_entry(const std::string& timestamp, const std::string& user_msg, 
                                   const std::string& ai_resp, const std::string& model);
    static void print_session_info(const std::string& name, const std::string& created_at, 
                                  const std::string& last_accessed, int entry_count);
    
    // Enhanced logo and banner functionality
    static void print_responsive_banner();
    static void print_delta_logo_ascii();
    static int get_terminal_width();
    static bool has_color_support();
    static void print_compact_logo();
    static void print_full_logo();
    
    
public:
    static constexpr const char* GREEN = "\033[32m";
    static constexpr const char* BRIGHT_GREEN = "\033[92m";
    static constexpr const char* RED = "\033[31m";
    static constexpr const char* YELLOW = "\033[33m";
    static constexpr const char* RESET = "\033[0m";
    static constexpr const char* BOLD = "\033[1m";
    
    // Delta logo color scheme - exact match from image
    static constexpr const char* DELTA_BLUE = "\033[38;2;0;31;63m";   // #001F3F - Deep blue from image
    static constexpr const char* DELTA_RED = "\033[38;2;255;65;54m";  // #FF4136 - Red dot from image
};

// ============================================================================
// Authentication Module - Optional one-time telemetry
// ============================================================================
class Auth {
public:
    Auth();
    ~Auth();
    
    // Check if user has completed first-time setup
    bool is_first_run();
    
    // Prompt user for optional telemetry and send data if accepted
    void handle_first_run();
    
    // Send install data to tracking server (fails silently if offline)
    bool send_install_data(const std::string& uuid, const std::string& platform);
    
    // Get or generate device UUID
    std::string get_device_uuid();
    
    // Get current platform string
    static std::string get_platform();
    
private:
    std::string config_path_;
    std::string uuid_;
    bool load_config();
    void save_config();
};

// ============================================================================
// Models Module - Model management and downloads
// ============================================================================

// Model registry entry
struct ModelRegistry {
    std::string name;           // e.g., "qwen2.5:0.5b" (registry key with colon)
    std::string short_name;     // e.g., "qwen2.5-0.5b" (user-friendly CLI name)
    std::string repo_id;        // e.g., "Qwen/Qwen2-0.5B-Instruct-GGUF"
    std::string filename;       // e.g., "qwen2.5-0.5b-instruct-q4_k_m.gguf"
    std::string quantization;   // e.g., "Q4_K_M"
    long long size_bytes;       // Approximate size
    std::string description;    // Description for UI
    std::string display_name;   // e.g., "Qwen 2.5 0.5B" (for friendly output)
    int max_context;            // Maximum usable context size for llama-server (-c parameter)
};

class ModelManager {
public:
    ModelManager();
    ~ModelManager();
    
    // List available models (cached locally)
    std::vector<std::string> list_models();
    
    // Check if model exists locally
    bool has_model(const std::string& model_name);
    
    // Get model path
    std::string get_model_path(const std::string& model_name);
    
    // Add model to cache
    bool add_model(const std::string& model_name, const std::string& file_path);
    
    // Remove model from cache
    bool remove_model(const std::string& model_name);
    
    // Remove model with confirmation prompt
    bool remove_model_with_confirmation(const std::string& model_name);
    
    // Get model info (size, quantization, etc.)
    std::map<std::string, std::string> get_model_info(const std::string& model_name);
    
    // ===== NEW: Download functionality =====
    
    // Download model from Hugging Face
    // model_name format: "qwen3:0.6b" or "llama3.2:1b"
    // Returns true on success, false on failure
    bool pull_model(const std::string& model_name, 
                   const std::string& quantization = "Q4_K_M");
    
    // Get available models from registry (not yet downloaded)
    std::vector<ModelRegistry> get_registry_models();
    
    // Get model registry entry by name
    ModelRegistry get_registry_entry(const std::string& model_name);
    
    // Check if model exists in registry
    bool is_in_registry(const std::string& model_name);
    
    // Get max context size for llama-server (-c). Returns user override, else registry max_context, or 0 (use model default).
    int get_max_context_for_model(const std::string& model_name);

    // Set user's context size override for a model (persisted to ~/.delta-cli/model_context_overrides.json).
    void set_max_context_override(const std::string& model_name, int ctx);
    
    // Resolve short name to full GGUF filename
    std::string resolve_model_name(const std::string& input_name);
    
    // Get short_name from filename by looking up in registry
    std::string get_short_name_from_filename(const std::string& filename);
    
    // Get name (with colon) from filename by looking up in registry
    std::string get_name_from_filename(const std::string& filename);
    
    // Check if model is installed locally
    bool is_model_installed(const std::string& model_name);
    
    // Get friendly model info for display
    struct ModelInfo {
        std::string name;
        std::string display_name;
        std::string description;
        std::string size_str;
        std::string quantization;
        long long size_bytes;
        bool installed;
    };
    std::vector<ModelInfo> get_friendly_model_list(bool include_available = false);
    
    // Download progress callback
    typedef void (*ProgressCallback)(double progress, long long current, long long total);
    void set_progress_callback(ProgressCallback callback);
    
    // ===== DEFAULT MODEL SUPPORT =====
    
    // Get the default model name (registry format: "qwen3:0.6b")
    static std::string get_default_model();
    
    // Get default model short name (CLI format: "qwen3-0.6b")
    std::string get_default_model_short_name() const;
    
    // Ensure default model is installed (auto-download if needed)
    // Returns true if installed/downloaded successfully
    bool ensure_default_model_installed(ProgressCallback progress = nullptr);
    
    // Get best available model (default if installed, otherwise first available)
    std::string get_auto_selected_model();
    
private:
    std::string models_dir_;
    ProgressCallback progress_callback_;
    
    // Default model constant
    static const std::string DEFAULT_MODEL_NAME;
    
    void ensure_models_dir();
    void init_model_registry();
    /** Resolve model name to registry map key (by exact key or by entry.name for catalog names). Returns empty if not found. */
    std::string get_registry_key_for_name(const std::string& model_name) const;
    std::map<std::string, ModelRegistry> model_registry_;

    // Per-model context overrides (user choice from UI), persisted to file
    std::map<std::string, int> context_overrides_;
    std::string context_overrides_path_;
    void load_context_overrides();
    void save_context_overrides();
    
    // HTTP download helper using libcurl
    bool download_file(const std::string& url, 
                      const std::string& dest_path,
                      ProgressCallback progress = nullptr);
    
    // Construct Hugging Face URL
    std::string get_hf_url(const std::string& repo_id, const std::string& filename);
};

// ============================================================================
// Inference Module - llama.cpp integration
// ============================================================================
struct InferenceConfig {
    std::string model_path;
    int n_ctx = 0;           // context size
    int n_batch = 512;          // batch size
    int n_threads = 4;          // CPU threads
    int n_gpu_layers = 0;       // GPU layers (0 = CPU only)
    float temperature = 0.8f;
    float top_p = 0.95f;
    int top_k = 40;
    int repeat_last_n = 64;
    float repeat_penalty = 1.1f;
    bool use_mmap = true;
    bool use_mlock = false;
    bool multimodal = false;    // Enable image inputs
};

class InferenceEngine {
public:
    InferenceEngine();
    ~InferenceEngine();
    
    // Load model with configuration
    bool load_model(const InferenceConfig& config);
    
    // Unload current model
    void unload_model();
    
    // Check if model is loaded
    bool is_loaded() const { return model_ != nullptr && ctx_ != nullptr; }
    
    // Generate text response
    std::string generate(const std::string& prompt, 
                        int max_tokens = 512,
                        bool stream = true);
    
    // Generate with multimodal input (text + images)
    std::string generate_multimodal(const std::string& prompt,
                                    const std::vector<std::string>& image_paths,
                                    int max_tokens = 512,
                                    bool stream = true);
    
    // Get model info
    std::string get_model_name() const;
    size_t get_model_size() const;
    int get_context_size() const;
    
    // Tokenize text
    std::vector<int> tokenize(const std::string& text, bool add_bos = true);
    
    // Detokenize
    std::string detokenize(const std::vector<int>& tokens);
    
private:
    llama_model* model_;
    llama_context* ctx_;
    llama_sampler* sampler_;
    InferenceConfig config_;
    
    void setup_sampler();
    std::string generate_internal(const std::vector<int>& tokens, 
                                  int max_tokens, 
                                  bool stream);
};

// ============================================================================
// Tools Module - Extended capabilities
// ============================================================================
namespace tools {

// Dependency protocol - execute shell commands safely
class DepProtocol {
public:
    struct Result {
        int exit_code;
        std::string output;
        std::string error;
        bool success;
    };
    
    static Result execute(const std::string& command, 
                         const std::vector<std::string>& args = {},
                         const std::string& working_dir = "");
};

// File operations
class FileOps {
public:
    static std::string read_file(const std::string& path);
    static bool write_file(const std::string& path, const std::string& content);
    static bool file_exists(const std::string& path);
    static bool dir_exists(const std::string& path);
    static bool create_dir(const std::string& path);
    static std::vector<std::string> list_dir(const std::string& path);
    /** Return full path of first .gguf file in directory, or "" if none. For server compatibility when --models-dir is not supported. */
    static std::string first_gguf_in_dir(const std::string& path);
    /** Resolve path to absolute so llama-server can find the model regardless of cwd. Returns empty if path is empty or resolution fails. */
    static std::string absolute_path(const std::string& path);
    static std::string get_home_dir();
    static std::string join_path(const std::string& a, const std::string& b);
    static std::string get_executable_dir();
};

// Shell integration
class Shell {
public:
    static std::string get_shell();
    static std::string expand_path(const std::string& path);
    static std::map<std::string, std::string> get_env();
};

// Browser utilities
class Browser {
public:
    // Open URL in default browser (portable across platforms)
    static bool open_url(const std::string& url);
};

} // namespace tools

// ============================================================================
// HISTORY AND SESSION MANAGEMENT
// ============================================================================

// Forward declarations
struct HistoryEntry;
struct Session;
class HistoryManager;

// Global history manager access
HistoryManager& get_history_manager();
void cleanup_history_manager();

// Server functionality is provided by llama-server from llama.cpp

} // namespace delta

#endif // DELTA_CLI_H

