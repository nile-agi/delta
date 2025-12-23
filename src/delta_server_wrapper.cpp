/**
 * Delta CLI Server Wrapper
 * Uses Delta web UI from public/ directory (built from assets/)
 */

#include "delta_cli.h"
#include "model_api_server.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <limits.h>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>
#include <mutex>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <libgen.h>
#include <unistd.h>
#else
#include <unistd.h>
#include <libgen.h>
#endif

namespace delta {

class DeltaServerWrapper {
private:
    std::string llama_server_path_;
    std::string model_path_;
    int port_;
    int max_parallel_;
    int max_context_;
    bool enable_embedding_;
    bool enable_reranking_;
    std::string draft_model_;
    std::string grammar_file_;
    
    // Process management for delta-server
    std::thread llama_server_thread_;
    std::atomic<bool> llama_server_running_;
    std::atomic<bool> should_stop_;
    pid_t llama_server_pid_;
    std::mutex llama_server_mutex_;

public:
    DeltaServerWrapper() 
        : port_(8080), max_parallel_(4), max_context_(16384), 
          enable_embedding_(false), enable_reranking_(false),
          llama_server_running_(false), should_stop_(false), llama_server_pid_(0) {}

    bool find_llama_server() {
        std::vector<std::string> possible_paths = {
            "delta-server",
            "./delta-server",
            "/opt/homebrew/bin/delta-server",
            "/usr/local/bin/delta-server",
            "/usr/bin/delta-server"
        };

        for (const auto& path : possible_paths) {
            if (std::filesystem::exists(path)) {
                llama_server_path_ = path;
                return true;
            }
        }
        return false;
    }

    void set_model_path(const std::string& path) {
        model_path_ = path;
    }

    void set_port(int port) {
        port_ = port;
    }

    void set_max_parallel(int np) {
        max_parallel_ = np;
    }

    void set_max_context(int ctx) {
        max_context_ = ctx;
    }

    void set_embedding(bool enable) {
        enable_embedding_ = enable;
    }

    void set_reranking(bool enable) {
        enable_reranking_ = enable;
    }

    void set_draft_model(const std::string& model) {
        draft_model_ = model;
    }

    void set_grammar_file(const std::string& file) {
        grammar_file_ = file;
    }

    std::string find_webui_path() {
        // Find the Delta web UI directory (from public/ only, not llama.cpp web UI)
        std::vector<std::string> candidates;
        
        // Get current executable directory
        std::string exe_path;
#ifdef _WIN32
        char exe_buf[MAX_PATH];
        GetModuleFileNameA(NULL, exe_buf, MAX_PATH);
        exe_path = exe_buf;
        size_t last_slash = exe_path.find_last_of("\\/");
        if (last_slash != std::string::npos) {
            exe_path = exe_path.substr(0, last_slash);
        }
#elif defined(__APPLE__)
        char exe_buf[PATH_MAX];
        uint32_t size = sizeof(exe_buf);
        if (_NSGetExecutablePath(exe_buf, &size) == 0) {
            char resolved[PATH_MAX];
            if (realpath(exe_buf, resolved) != nullptr) {
                exe_path = std::string(dirname(resolved));
            }
        }
#else
        char exe_buf[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (len != -1) {
            exe_buf[len] = '\0';
            exe_path = std::string(dirname(exe_buf));
        }
#endif
        
        // Build candidate paths - check Homebrew share directory first, then public/ (built Delta web UI from assets/)
        // Only use Delta web UI from public/, never fall back to llama.cpp web UI or assets/ source
        // Homebrew installs web UI to share/delta-cli/webui relative to the prefix
        if (!exe_path.empty()) {
            // Check Homebrew share directory (for installed packages)
            candidates.push_back(exe_path + "/../../share/delta-cli/webui");
            candidates.push_back(exe_path + "/../../../share/delta-cli/webui");
            // Check macOS app bundle Resources directory (for DMG installs)
            // Executable is at Contents/MacOS/delta, web UI is at Contents/Resources/webui
            candidates.push_back(exe_path + "/../Resources/webui");
            candidates.push_back(exe_path + "/../../Resources/webui");
            // Check relative to executable (Delta web UI from public/)
            candidates.push_back(exe_path + "/../public");
            candidates.push_back(exe_path + "/../../public");
            candidates.push_back(exe_path + "/../webui");
            candidates.push_back(exe_path + "/../../webui");
        }
        // Check standard Homebrew locations
        candidates.push_back("/opt/homebrew/share/delta-cli/webui");
        candidates.push_back("/usr/local/share/delta-cli/webui");
        // Check relative paths (Delta web UI from public/)
        candidates.push_back("public");
        candidates.push_back("./public");
        candidates.push_back("../public");
        candidates.push_back("webui");
        candidates.push_back("./webui");
        candidates.push_back("../webui");
        
        // Check each candidate
        for (const auto& candidate : candidates) {
            std::filesystem::path path(candidate);
            
            // Try to resolve to absolute path first
            std::filesystem::path abs_path;
            try {
                if (path.is_absolute()) {
                    abs_path = path;
                } else {
                    // Try to resolve relative to current working directory
                    abs_path = std::filesystem::absolute(path);
                }
                
                // Normalize the path (resolve .. and .)
                abs_path = std::filesystem::canonical(abs_path);
            } catch (...) {
                // If canonical fails, try absolute
                try {
                    abs_path = std::filesystem::absolute(path);
                } catch (...) {
                    continue; // Skip this candidate
                }
            }
            
            if (std::filesystem::exists(abs_path) && std::filesystem::is_directory(abs_path)) {
                std::filesystem::path index_file = abs_path / "index.html.gz";
                std::filesystem::path index_file2 = abs_path / "index.html";
                if (std::filesystem::exists(index_file) || std::filesystem::exists(index_file2)) {
                    return abs_path.string();
                }
            }
        }
        
        return "";  // Not found, server will use embedded UI
    }
    
    std::string build_llama_server_command(const std::string& model_path, int ctx_size, const std::string& model_alias) {
        std::string cmd = llama_server_path_;
        cmd += " -m \"" + model_path + "\"";
        cmd += " --port " + std::to_string(port_);
        cmd += " --parallel " + std::to_string(max_parallel_);
        cmd += " -c " + std::to_string(ctx_size);
        
        // Add --flash-attn flag
        if (ctx_size > 16384) {
            cmd += " --flash-attn off";
            if (ctx_size > 32768) {
                cmd += " --gpu-layers 0";
            }
        } else {
            cmd += " --flash-attn auto";
        }
        
        // Add --jinja flag for gemma3 models
        std::string model_path_lower = model_path;
        std::transform(model_path_lower.begin(), model_path_lower.end(), model_path_lower.begin(), ::tolower);
        if (model_path_lower.find("gemma3") != std::string::npos) {
            cmd += " --jinja";
        }
        
        // Add --alias if provided
        if (!model_alias.empty()) {
            cmd += " --alias \"" + model_alias + "\"";
        }
        
        // Add --path flag to use Delta web UI if found
        std::string webui_path = find_webui_path();
        if (!webui_path.empty()) {
            cmd += " --path \"" + webui_path + "\"";
        }
        
        if (enable_embedding_) {
            cmd += " --embedding";
        }
        
        if (enable_reranking_) {
            cmd += " --reranking";
        }
        
        if (!draft_model_.empty()) {
            cmd += " --md \"" + draft_model_ + "\"";
        }
        
        if (!grammar_file_.empty()) {
            cmd += " --grammar-file \"" + grammar_file_ + "\"";
        }
        
        return cmd;
    }
    
    void stop_llama_server() {
        std::lock_guard<std::mutex> lock(llama_server_mutex_);
        if (llama_server_pid_ != 0) {
            pid_t pid_to_kill = (llama_server_pid_ < 0) ? llama_server_pid_ : llama_server_pid_;
            // Kill the delta-server process (or process group if negative)
            kill(pid_to_kill, SIGTERM);
            // Wait a bit for it to terminate
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // Force kill if still running
            int status;
            pid_t actual_pid = (llama_server_pid_ < 0) ? -llama_server_pid_ : llama_server_pid_;
            if (waitpid(actual_pid, &status, WNOHANG) == 0) {
                // Process still running, force kill
                kill(pid_to_kill, SIGKILL);
                waitpid(actual_pid, &status, 0); // Wait for it to die
            }
            llama_server_pid_ = 0;
            llama_server_running_ = false;
        }
    }
    
    bool restart_llama_server(const std::string& new_model_path, const std::string& model_name, int ctx_size, const std::string& model_alias) {
        std::lock_guard<std::mutex> lock(llama_server_mutex_);
        
        std::cout << "🔄 Switching to model: " << model_name << std::endl;
        std::cout << "   Path: " << new_model_path << std::endl;
        
        // Stop current delta-server
        if (llama_server_running_ && llama_server_pid_ != 0) {
            std::cout << "   Stopping current model..." << std::endl;
            stop_llama_server();
            // Wait a bit more to ensure port is free
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // Update model path
        model_path_ = new_model_path;
        max_context_ = ctx_size;
        
        // Build new command
        std::string cmd = build_llama_server_command(new_model_path, ctx_size, model_alias);
        
        // Start delta-server in background using fork
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: create new process group
            setsid(); // Create new session and process group
            
            // Execute delta-server
            // Keep stdout/stderr for debugging, but could redirect to /dev/null if needed
            execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)NULL);
            _exit(1); // Should not reach here
        } else if (pid > 0) {
            // Parent process: store PID (negative for process group)
            llama_server_pid_ = -pid; // Store negative PID for process group
            llama_server_running_ = true;
            
            // Wait a moment for server to start
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            
            // Check if process is still running
            int status;
            if (waitpid(pid, &status, WNOHANG) == 0) {
                // Process is still running - good!
                // Also verify server is responding
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                std::cout << "   ✓ Model loaded successfully!" << std::endl;
                return true;
            } else {
                // Process exited - check if it was successful
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    std::cout << "   ✓ Model loaded successfully!" << std::endl;
                    return true;
                } else {
                    std::cerr << "   ✗ Failed to start delta-server (exited with code " 
                              << (WIFEXITED(status) ? WEXITSTATUS(status) : -1) << ")" << std::endl;
                    llama_server_running_ = false;
                    llama_server_pid_ = 0;
                    return false;
                }
            }
        } else {
            std::cerr << "   ✗ Failed to fork process" << std::endl;
            return false;
        }
    }

    int start_server() {
        if (!find_llama_server()) {
            std::cerr << "Error: delta-server not found!" << std::endl;
            std::cerr << "Please ensure delta-server is built and installed with Delta CLI." << std::endl;
            return 1;
        }

        if (model_path_.empty()) {
            std::cerr << "Error: No model specified!" << std::endl;
            return 1;
        }

        // Find and use Delta web UI
        std::string webui_path = find_webui_path();
        
        // Debug: Print web UI path if found
        if (!webui_path.empty()) {
            std::cout << "🌐 Web UI path: " << webui_path << std::endl;
        } else {
            std::cout << "⚠️  Web UI path not found, using embedded UI" << std::endl;
        }

        std::cout << "🚀 Starting Delta CLI Server..." << std::endl;
        std::cout << "📡 Server: http://localhost:" << port_ << std::endl;
        std::cout << "🤖 Model: " << model_path_ << std::endl;
        std::cout << "⚡ Parallel: " << max_parallel_ << std::endl;
        std::cout << "🧠 Context: " << max_context_ << std::endl;
        std::cout << "🌐 Web UI: http://localhost:" << port_ << std::endl;
        std::cout << "📡 API: http://localhost:" << port_ << "/v1/chat/completions" << std::endl;
        std::cout << "🔧 Model Management API: http://localhost:8081" << std::endl;
        std::cout << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        std::cout << std::endl;

        // Start model management API server on port 8081
        delta::start_model_api_server(8081);
        
        // Give the model API server a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Set up model switch callback
        delta::set_model_switch_callback([this](const std::string& model_path, const std::string& model_name, int ctx_size, const std::string& model_alias) -> bool {
            return this->restart_llama_server(model_path, model_name, ctx_size, model_alias);
        });
        
        // Start delta-server in background
        std::cout << "🚀 Starting delta-server..." << std::endl;
        if (!restart_llama_server(model_path_, "", max_context_, "")) {
            std::cerr << "Failed to start delta-server" << std::endl;
            delta::stop_model_api_server();
            return 1;
        }
        
        // Wait for delta-server to exit (or be killed)
        while (llama_server_running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            // Check if process is still running
            if (llama_server_pid_ > 0) {
                int status;
                if (waitpid(llama_server_pid_, &status, WNOHANG) != 0) {
                    // Process exited
                    llama_server_running_ = false;
                    break;
                }
            }
        }
        
        // Stop model API server when delta-server exits
        delta::stop_model_api_server();
        
        return 0;
    }
};

} // namespace delta

int main(int argc, char* argv[]) {
    delta::DeltaServerWrapper wrapper;
    
    // Parse command line arguments (simplified)
    std::string model_path;
    int port = 8080;
    int max_parallel = 4;
    int max_context = 16384;
    bool enable_embedding = false;
    bool enable_reranking = false;
    std::string draft_model;
    std::string grammar_file;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-m" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--parallel" && i + 1 < argc) {
            max_parallel = std::stoi(argv[++i]);
        } else if (arg == "-c" && i + 1 < argc) {
            max_context = std::stoi(argv[++i]);
        } else if (arg == "--embedding") {
            enable_embedding = true;
        } else if (arg == "--reranking") {
            enable_reranking = true;
        } else if (arg == "--md" && i + 1 < argc) {
            draft_model = argv[++i];
        } else if (arg == "--grammar-file" && i + 1 < argc) {
            grammar_file = argv[++i];
        }
    }

    wrapper.set_model_path(model_path);
    wrapper.set_port(port);
    wrapper.set_max_parallel(max_parallel);
    wrapper.set_max_context(max_context);
    wrapper.set_embedding(enable_embedding);
    wrapper.set_reranking(enable_reranking);
    wrapper.set_draft_model(draft_model);
    wrapper.set_grammar_file(grammar_file);

    return wrapper.start_server();
}
