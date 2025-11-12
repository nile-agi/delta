/**
 * Delta CLI Server Wrapper
 * Customizes llama-server with Delta CLI branding
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>

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

    bool setup_custom_ui() {
        // Use original llama.cpp web UI
        // This function is kept for compatibility but does nothing
            return true;
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

        // Setup custom UI
        setup_custom_ui();

        // Construct llama-server command
        std::string cmd = llama_server_path_;
        cmd += " -m \"" + model_path_ + "\"";
        cmd += " --port " + std::to_string(port_);
        cmd += " --parallel " + std::to_string(max_parallel_);
        cmd += " -c " + std::to_string(max_context_);

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
