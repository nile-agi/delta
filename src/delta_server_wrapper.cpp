/**
 * Delta CLI Server Wrapper
 * Uses original llama.cpp web UI
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <limits.h>
#include <algorithm>
#include <cctype>
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

public:
    DeltaServerWrapper() 
        : port_(8080), max_parallel_(4), max_context_(16384), 
          enable_embedding_(false), enable_reranking_(false) {}

    bool find_llama_server() {
        std::vector<std::string> possible_paths = {
            "llama-server",
            "./llama-server",
            "/opt/homebrew/bin/llama-server",
            "/usr/local/bin/llama-server",
            "/usr/bin/llama-server"
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
        // Find the original llama.cpp web UI directory
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
        
        // Build candidate paths - check Homebrew share directory first, then public/ (built web UI from assets/), then assets/
        // Homebrew installs web UI to share/delta-cli/webui relative to the prefix
        if (!exe_path.empty()) {
            // Check Homebrew share directory (for installed packages)
            candidates.push_back(exe_path + "/../../share/delta-cli/webui");
            candidates.push_back(exe_path + "/../../../share/delta-cli/webui");
            // Check relative to executable
            candidates.push_back(exe_path + "/../public");
            candidates.push_back(exe_path + "/../../public");
            candidates.push_back(exe_path + "/../assets");
            candidates.push_back(exe_path + "/../../assets");
        }
        // Check standard Homebrew locations
        candidates.push_back("/opt/homebrew/share/delta-cli/webui");
        candidates.push_back("/usr/local/share/delta-cli/webui");
        // Check relative paths
        candidates.push_back("public");
        candidates.push_back("./public");
        candidates.push_back("../public");
        candidates.push_back("assets");
        candidates.push_back("./assets");
        candidates.push_back("../assets");
        
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

    int start_server() {
        if (!find_llama_server()) {
            std::cerr << "Error: llama-server not found!" << std::endl;
            std::cerr << "Please install llama-server:" << std::endl;
            std::cerr << "  macOS: brew install llama.cpp" << std::endl;
            std::cerr << "  Ubuntu: apt install llama.cpp-server" << std::endl;
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

        // Construct llama-server command
        std::string cmd = llama_server_path_;
        cmd += " -m \"" + model_path_ + "\"";
        cmd += " --port " + std::to_string(port_);
        cmd += " --parallel " + std::to_string(max_parallel_);
        cmd += " -c " + std::to_string(max_context_);
        
        // Add --flash-attn flag for all models (requires value: on, off, or auto)
        cmd += " --flash-attn on";
        
        // Add --jinja flag for gemma3 models
        // Check model_path for gemma3 (case-insensitive)
        std::string model_path_lower = model_path_;
        std::transform(model_path_lower.begin(), model_path_lower.end(), model_path_lower.begin(), ::tolower);
        if (model_path_lower.find("gemma3") != std::string::npos) {
            cmd += " --jinja";
        }

        // Add --path flag to use Delta web UI if found
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

        std::cout << "🚀 Starting Delta CLI Server..." << std::endl;
        std::cout << "📡 Server: http://localhost:" << port_ << std::endl;
        std::cout << "🤖 Model: " << model_path_ << std::endl;
        std::cout << "⚡ Parallel: " << max_parallel_ << std::endl;
        std::cout << "🧠 Context: " << max_context_ << std::endl;
        std::cout << "🌐 Web UI: http://localhost:" << port_ << std::endl;
        std::cout << "📡 API: http://localhost:" << port_ << "/v1/chat/completions" << std::endl;
        std::cout << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        std::cout << std::endl;

        // Execute llama-server
        int result = system(cmd.c_str());
        return result;
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
