/**
 * Delta CLI Server Wrapper
 * Uses Delta web UI from public/ directory (built from assets/)
 */

#include "delta_cli.h"
#include "model_api_server.h"
#include "api/openai_api.h"
#include "server_health.h"
#include "owned_process.h"
#include "agent/agent_database.h"
#include "tools/hardware_monitor.h"
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <limits.h>
#include <cctype>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shlwapi.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <libgen.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

namespace delta {

class DeltaServerWrapper;

#ifndef _WIN32
static volatile sig_atomic_t g_wrapper_stop_requested = 0;
static DeltaServerWrapper* g_wrapper_instance = nullptr;

static void wrapper_signal_handler(int) {
    g_wrapper_stop_requested = 1;
}
#else
static std::atomic<bool>* g_win_should_stop = nullptr;

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_BREAK_EVENT) {
        if (g_win_should_stop)
            g_win_should_stop->store(true);
        return TRUE;
    }
    return FALSE;
}
#endif

#ifdef _WIN32
typedef DWORD pid_t;
#define WNOHANG 1
#define WIFEXITED(status) ((status) != STILL_ACTIVE)
#define WEXITSTATUS(status) (status)
#endif

class DeltaServerWrapper {
  private:
    std::string llama_server_path_;
    std::string model_path_;
    std::string models_dir_;
    int port_;
    int model_api_port_;
    int max_parallel_;
    int max_context_;
    bool llama_server_is_router_ = false;
    bool enable_embedding_;
    bool enable_reranking_;
    std::string draft_model_;
    std::string grammar_file_;

    std::unique_ptr<ModelRuntime> runtime_;
    std::optional<RuntimeConfig> effective_config_;
    ModelRuntime::Cancelled load_cancelled_;
    std::atomic<bool> llama_server_running_;
    std::atomic<bool> should_stop_;
    OwnedProcess owned_process_;
    uint64_t process_generation_ = 0;
    bool intentional_stop_ = false;
    std::mutex llama_server_mutex_;

    // DHATS Brain: persistent hardware monitor (avoids re-probing NVML/ROCm every load)
    delta::HardwareMonitor hw_monitor_;

    std::mutex errors_mutex_;
    std::string recent_errors_;

    static void report_child_exit(long long code, int signal) {
        // A bounded numeric record lets the desktop report child failures separately from its own exit.
        std::fprintf(stderr, "[llama-server-exit] code=%lld signal=%d\n", code, signal);
        std::fflush(stderr);
    }

    void note_server_stderr(const std::string& line) {
        std::lock_guard<std::mutex> lock(errors_mutex_);
        recent_errors_ += line.substr(0, 4096) + "\n";
        if (recent_errors_.size() > 16384)
            recent_errors_.erase(0, recent_errors_.size() - 16384);
    }

  public:
    DeltaServerWrapper()
        : port_(8080), model_api_port_(8081), max_parallel_(4), max_context_(0), enable_embedding_(false),
          enable_reranking_(false), llama_server_running_(false), should_stop_(false) {}

    ~DeltaServerWrapper() {
        if (runtime_)
            runtime_->shutdown();
        stop_llama_server();
    }

    std::string get_executable_path() {
        std::string exe_path;
#ifdef _WIN32
        char exe_buf[MAX_PATH];
        if (GetModuleFileNameA(NULL, exe_buf, MAX_PATH)) {
            exe_path = exe_buf;
        }
#elif defined(__APPLE__)
        char exe_buf[PATH_MAX];
        uint32_t size = sizeof(exe_buf);
        if (_NSGetExecutablePath(exe_buf, &size) == 0) {
            char resolved[PATH_MAX];
            if (realpath(exe_buf, resolved) != nullptr) {
                exe_path = resolved;
            }
        }
#else
        char exe_buf[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (len != -1) {
            exe_buf[len] = '\0';
            exe_path = exe_buf;
        }
#endif
        return exe_path;
    }

    std::string get_executable_dir() {
        std::string exe_path = get_executable_path();
        if (exe_path.empty())
            return "";
        size_t last_slash = exe_path.find_last_of("/\\");
        return (last_slash != std::string::npos) ? exe_path.substr(0, last_slash) : "";
    }

    static std::string resolve_path(const std::string& path) {
        try {
            std::filesystem::path p(path);
            if (p.is_relative()) {
                p = std::filesystem::absolute(p);
            }
            return std::filesystem::canonical(p).string();
        } catch (...) {
            return path;
        }
    }

    bool find_llama_server() {
        std::string self_path = resolve_path(get_executable_path());
        std::string exe_dir = get_executable_dir();
        std::vector<std::string> possible_paths;
        if (!exe_dir.empty()) {
#ifdef _WIN32
            possible_paths.push_back(exe_dir + "\\server.exe");
            possible_paths.push_back(exe_dir + "\\llama-server.exe");
            possible_paths.push_back(exe_dir + "\\..\\server.exe");
            possible_paths.push_back(exe_dir + "\\..\\llama-server.exe");
#else
            possible_paths.push_back(exe_dir + "/server");
            possible_paths.push_back(exe_dir + "/llama-server");
            possible_paths.push_back(exe_dir + "/bin/server");
            possible_paths.push_back(exe_dir + "/bin/llama-server");
            possible_paths.push_back(exe_dir + "/../server");
            possible_paths.push_back(exe_dir + "/../llama-server");
            possible_paths.push_back(exe_dir + "/../bin/server");
            possible_paths.push_back(exe_dir + "/../bin/llama-server");
#endif
            try {
                for (const auto& entry : std::filesystem::directory_iterator(exe_dir)) {
                    std::string fname = entry.path().filename().string();
                    if ((fname.find("llama-server-") == 0 || fname.find("server-") == 0) &&
                        fname.find("delta-server") == std::string::npos) {
                        possible_paths.push_back(entry.path().string());
                    }
                }
            } catch (...) {
            }
        }
        possible_paths.push_back("server");
        possible_paths.push_back("llama-server");
        possible_paths.push_back("./server");
        possible_paths.push_back("./llama-server");
#ifndef _WIN32
        possible_paths.push_back("/opt/homebrew/bin/server");
        possible_paths.push_back("/opt/homebrew/bin/llama-server");
        possible_paths.push_back("/usr/local/bin/server");
        possible_paths.push_back("/usr/local/bin/llama-server");
        possible_paths.push_back("/usr/bin/server");
        possible_paths.push_back("/usr/bin/llama-server");
#endif

        for (const auto& path : possible_paths) {
            if (!std::filesystem::exists(path))
                continue;
            std::string resolved = resolve_path(path);
            if (!resolved.empty() && resolved == self_path)
                continue;
            llama_server_path_ = path;
            return true;
        }
        return false;
    }

    void set_model_path(const std::string& path) { model_path_ = path; }
    void set_models_dir(const std::string& dir) { models_dir_ = dir; }
    void set_port(int port) { port_ = port; }
    void set_max_parallel(int np) { max_parallel_ = np; }
    void set_model_api_port(int port) { model_api_port_ = port; }
    void set_max_context(int ctx) { max_context_ = ctx; }
    void set_embedding(bool enable) { enable_embedding_ = enable; }
    void set_reranking(bool enable) { enable_reranking_ = enable; }
    void set_draft_model(const std::string& model) { draft_model_ = model; }
    void set_grammar_file(const std::string& file) { grammar_file_ = file; }

    void print_api_urls() const {
        const std::string port = std::to_string(model_api_port_);
        std::cout << "  OpenAI-compatible API: http://127.0.0.1:" << port << "/v1" << std::endl;
        const std::string host = delta::model_api_host();
        if (!delta::api::is_loopback(host)) {
            const std::string lan = host == "0.0.0.0" ? delta::api::lan_address() : host;
            if (!lan.empty())
                std::cout << "  On your network:       http://" << lan << ":" << port << "/v1" << std::endl;
        }
        if (!delta::model_api_key().empty())
            std::cout << "  API key required:      Authorization: Bearer <key>" << std::endl;
    }

    std::string find_webui_path() {
        std::vector<std::string> candidates;

#ifndef _WIN32
        {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                std::string cwd_str(cwd);
                candidates.push_back(cwd_str + "/public");
                candidates.push_back(cwd_str + "/../public");
                candidates.push_back(cwd_str + "/webui");
                candidates.push_back(cwd_str + "/../webui");
            }
        }
#else
        {
            char cwd[MAX_PATH];
            if (_getcwd(cwd, MAX_PATH) != nullptr) {
                std::string cwd_str(cwd);
                candidates.push_back(cwd_str + "\\public");
                candidates.push_back(cwd_str + "\\..\\public");
                candidates.push_back(cwd_str + "\\webui");
                candidates.push_back(cwd_str + "\\..\\webui");
            }
        }
#endif
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

        if (!exe_path.empty()) {
            candidates.push_back(exe_path + "/../../share/delta-cli/webui");
            candidates.push_back(exe_path + "/../../../share/delta-cli/webui");
            candidates.push_back(exe_path + "/../Resources/webui");
            candidates.push_back(exe_path + "/../../Resources/webui");
            candidates.push_back(exe_path + "/webui");
            candidates.push_back(exe_path + "/public");
            candidates.push_back(exe_path + "/../public");
            candidates.push_back(exe_path + "/../../public");
            candidates.push_back(exe_path + "/../../../public");
            candidates.push_back(exe_path + "/../webui");
            candidates.push_back(exe_path + "/../../webui");
        }
        candidates.push_back("/opt/homebrew/share/delta-cli/webui");
        candidates.push_back("/usr/local/share/delta-cli/webui");
        candidates.push_back("public");
        candidates.push_back("./public");
        candidates.push_back("../public");
        candidates.push_back("webui");
        candidates.push_back("./webui");
        candidates.push_back("../webui");

        for (const auto& candidate : candidates) {
            std::filesystem::path path(candidate);
            std::filesystem::path abs_path;
            try {
                if (path.is_absolute()) {
                    abs_path = path;
                } else {
                    abs_path = std::filesystem::absolute(path);
                }
                abs_path = std::filesystem::canonical(abs_path);
            } catch (...) {
                try {
                    abs_path = std::filesystem::absolute(path);
                } catch (...) {
                    continue;
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

        return "";
    }

    // The model family a GGUF belongs to, version included, since generations of one family can
    // change tokenizer: "qwen2.5-0.5b..." -> "qwen2.5", "gemma-3-270m..." -> "gemma-3".
    std::vector<std::string> build_llama_server_arguments(const std::string& model_path, int ctx_size,
                                                          const std::string& model_alias) {
        std::vector<std::string> args{std::filesystem::absolute(llama_server_path_).string()};
        auto add = [&](const std::string& key, const std::string& value) {
            args.push_back(key);
            args.push_back(value);
        };
        if (!model_path.empty())
            add("-m", model_path);
        else if (!models_dir_.empty())
            add("--models-dir", std::filesystem::absolute(models_dir_).string());
        add("--host", "127.0.0.1");
        add("--port", std::to_string(port_));
        RuntimeConfig config;
        if (effective_config_)
            config = *effective_config_;
        else {
            config.context = ctx_size > 0 ? ctx_size : 8192;
            config.parallel = max_parallel_;
            config.batch = runtime_env_int("LLAMA_ARG_BATCH", 512);
            config.microbatch = runtime_env_int("LLAMA_ARG_UBATCH", 128);
            config.gpu_layers = runtime_env_int("LLAMA_ARG_N_GPU_LAYERS", 0);
        }
        validate_runtime_config(config);
        add("-c", std::to_string(config.context));
        add("--parallel", std::to_string(config.parallel));
        add("--batch-size", std::to_string(config.batch));
        add("--ubatch-size", std::to_string(config.microbatch));
        add("--gpu-layers", std::to_string(config.gpu_layers));
        add("--flash-attn", config.context > 16384 ? "off" : "auto");
        for (const char* flag : {"--jinja", "--metrics", "--slots", "--props", "--cache-prompt"})
            args.push_back(flag);
        if (!model_alias.empty())
            add("--alias", model_alias);
        auto webui = find_webui_path();
        if (!webui.empty())
            add("--path", webui);
        if (enable_embedding_)
            args.push_back("--embedding");
        if (enable_reranking_)
            args.push_back("--reranking");
        auto draft = effective_config_ ? config.draft_path : draft_model_;
        if (!draft.empty()) {
            add("--model-draft", draft);
            add("--spec-draft-n-max", "16");
        }
        if (!grammar_file_.empty())
            add("--grammar-file", grammar_file_);
        const auto nodes = agent::AgentDatabase::instance().get_enabled_rpc_nodes();
        std::string rpc;
        for (const auto& node : nodes) {
            if (!rpc.empty())
                rpc += ",";
            rpc += node.endpoint;
        }
        if (!rpc.empty())
            add("--rpc", rpc);
        return args;
    }

    void stop_llama_server_locked() {
        intentional_stop_ = owned_process_.alive();
        owned_process_.stop();
        intentional_stop_ = false;
        llama_server_running_ = false;
    }
    void stop_llama_server() {
        std::lock_guard<std::mutex> lock(llama_server_mutex_);
        stop_llama_server_locked();
    }
    ServerReadyState wait_until_model_loaded() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < deadline) {
            if ((load_cancelled_ && load_cancelled_()) || should_stop_)
                return ServerReadyState::StillLoading;
            if (!owned_process_.alive()) {
                stop_llama_server_locked(); // Drain final stderr before reporting failure.
                return ServerReadyState::Exited;
            }
            if (probe_server_health(port_, 1))
                return ServerReadyState::Ready;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return ServerReadyState::StillLoading;
    }

    ServerReadyState restart_llama_server(const std::string& new_model_path, const std::string& model_name,
                                          int ctx_size, const std::string& model_alias) {
        return launch_llama_server(new_model_path, model_name, ctx_size, model_alias);
    }

    ServerReadyState launch_llama_server(const std::string& new_model_path, const std::string& model_name, int ctx_size,
                                         const std::string& model_alias) {
        // Mutable local copy: DHATS self-heal below may clear or rewrite the path
        std::string model_path_to_use = new_model_path;

        std::lock_guard<std::mutex> lock(llama_server_mutex_);

        // Skip restart if the same model is already loaded and running
        if (llama_server_running_ && !model_path_.empty() && !model_path_to_use.empty() &&
            model_path_ == model_path_to_use && ctx_size == max_context_) {
            return probe_server_health(port_) ? ServerReadyState::Ready : ServerReadyState::StillLoading;
        }

        if (llama_server_running_ && llama_server_is_router_ && !models_dir_.empty() && !model_path_to_use.empty()) {
            try {
                std::filesystem::path model_parent = std::filesystem::path(model_path_to_use).parent_path();
                std::filesystem::path models_dir_p = std::filesystem::path(models_dir_);
                bool same_dir = false;
                try {
                    same_dir = std::filesystem::canonical(model_parent) == std::filesystem::canonical(models_dir_p);
                } catch (...) {
                    auto abs1 = std::filesystem::absolute(model_parent).lexically_normal();
                    auto abs2 = std::filesystem::absolute(models_dir_p).lexically_normal();
                    same_dir = abs1 == abs2;
                }
                if (same_dir && ctx_size == max_context_) {
                    if (!model_name.empty()) {
                        std::cout << "Selected model: " << model_name << std::endl;
                    }
                    model_path_ = model_path_to_use;
                    // Router mode loads on demand, so the router being up is all the readiness
                    // there is to report here; per-model status comes from /v1/models.
                    return probe_server_health(port_) ? ServerReadyState::Ready : ServerReadyState::StillLoading;
                }
                if (same_dir) {
                    // Workers inherit the router's startup arguments. Recreate the router when
                    // its context changes, rather than reporting a limit it never applied.
                    model_path_to_use.clear();
                }
            } catch (...) {
            }
        }

        if (!model_name.empty()) {
            std::cout << "Loading model: " << model_name << std::endl;
            if (!model_path_to_use.empty()) {
                try {
                    auto fsize = std::filesystem::file_size(model_path_to_use);
                    double mb = static_cast<double>(fsize) / (1024.0 * 1024.0);
                    if (mb >= 1024.0) {
                        std::cout << "  Size: " << std::fixed << std::setprecision(1) << (mb / 1024.0) << " GB"
                                  << std::endl;
                    } else {
                        std::cout << "  Size: " << std::fixed << std::setprecision(1) << mb << " MB" << std::endl;
                    }
                } catch (...) {
                }
            }
        }

        // ===== DHATS: HARD BLOCK — never launch a model that cannot run =====
        if (!model_path_to_use.empty()) {
            long long size_bytes = 0;
            try {
                size_bytes = std::filesystem::file_size(model_path_to_use);
            } catch (...) {
            }

            if (size_bytes > 0) {
                auto compat =
                    hw_monitor_.check_model_compatibility(size_bytes, estimate_model_layers(size_bytes), ctx_size);

                if (!compat.can_run) {
                    delta::report_model_block(
                        {true, model_name, compat.warning, compat.recommendation, compat.suggested_context});
                    std::cerr << "[DHATS] ❌ BLOCKED: " << model_name << " — " << compat.warning << std::endl;
                    std::cerr << "[DHATS] " << compat.recommendation << std::endl;

                    // Keep the app usable: restart in idle/router mode (no -m)
                    model_path_to_use = "";
                    model_path_ = "";
                } else {
                    delta::report_model_block({false, "", "", "", 0});
                    if (!compat.efficient) {
                        std::cerr << "[DHATS] ⚠ " << compat.warning << std::endl;
                        std::cerr << "[DHATS] Recommendation: " << compat.recommendation << std::endl;
                    }
                }
            }
        }
        // ===== end DHATS block =====

        if (llama_server_running_) {
            stop_llama_server_locked();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        model_path_ = model_path_to_use;
        max_context_ = ctx_size;
        llama_server_is_router_ = model_path_to_use.empty() && !models_dir_.empty();

        auto args =
            build_llama_server_arguments(model_path_to_use, ctx_size, llama_server_is_router_ ? "" : model_alias);
        process_generation_ = effective_config_ ? effective_config_->generation : 0;
        try {
            owned_process_.start(
                {args, get_executable_dir(), process_generation_},
                [this](uint64_t generation, const std::string& line) {
                    if (generation != process_generation_)
                        return;
                    note_server_stderr(line);
                    // Runtime-native records are filtered/redacted again at the desktop boundary.
                    if (line.find("error") != std::string::npos || line.find("Error") != std::string::npos ||
                        line.find("fail") != std::string::npos || line.find("Fail") != std::string::npos ||
                        line.find("warning") != std::string::npos || line.find("not found") != std::string::npos ||
                        line.find("symbol lookup") != std::string::npos || line.find("dyld:") != std::string::npos)
                        std::cerr << "[llama-server] " << line << std::endl;
                },
                [this](uint64_t generation, ProcessExit exit) {
                    if (generation == process_generation_ && !intentional_stop_)
                        report_child_exit(exit.code, exit.signal);
                });
            llama_server_running_ = true;
            const auto state = wait_until_model_loaded();
            if (state == ServerReadyState::Ready)
                std::cout << "Server ready" << std::endl;
            else if (state == ServerReadyState::Exited)
                std::cerr << "Failed to start server: llama-server exited while loading the model" << std::endl;
            return state;
        } catch (const std::exception& error) {
            std::cerr << "Failed to start server: " << error.what() << std::endl;
            stop_llama_server_locked();
            return ServerReadyState::Exited;
        }
    }

    int start_server() {
        if (!find_llama_server()) {
            std::cerr << "Error: HTTP server binary ('server') not found." << std::endl;
            std::cerr << "The HTTP server binary ('server') was not found. Delta-server is a wrapper and needs the "
                         "llama.cpp 'server' binary."
                      << std::endl;
            std::cerr << "  • From source: run 'make install' from your build directory so 'server' is installed "
                         "alongside delta."
                      << std::endl;
            std::cerr << "  • Homebrew: run 'brew reinstall delta-cli' to install the server binary." << std::endl;
            std::cerr << "  • Ensure vendor/llama.cpp is present (git submodule update --init vendor/llama.cpp) and "
                         "rebuild with LLAMA_BUILD_SERVER=ON."
                      << std::endl;
            return 1;
        }

        if (model_path_.empty() && models_dir_.empty()) {
            std::string home = delta::tools::FileOps::get_home_dir();
            models_dir_ =
                delta::tools::FileOps::join_path(delta::tools::FileOps::join_path(home, ".delta-cli"), "models");
        }
        if (!model_path_.empty()) {
            std::string abs_path = delta::tools::FileOps::absolute_path(model_path_);
            if (!abs_path.empty())
                model_path_ = abs_path;
        }

        std::string webui_path = find_webui_path();

        std::cout << R"(
 ██████╗ ███████╗██╗  ████████╗ █████╗      ██████╗██╗     ██╗
 ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗    ██╔════╝██║     ██║
 ██║  ██║█████╗  ██║     ██║   ███████║    ██║     ██║     ██║
 ██║  ██║██╔══╝  ██║     ██║   ██╔══██║    ██║     ██║     ██║
 ██████╔╝███████╗███████╗██║   ██║  ██║    ╚██████╗███████╗██║
 ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝     ╚═════╝╚═════╝╚═╝
)" << std::endl;
#ifndef DELTA_VERSION
#define DELTA_VERSION "dev"
#endif
        std::cout << "  Delta v" << DELTA_VERSION << std::endl;
        std::cout << "  http://localhost:" << port_ << std::endl;
        if (!model_path_.empty()) {
            std::cout << "  Model: " << model_path_ << std::endl;
        }
        print_api_urls();
        std::cout << "  Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;

        delta::set_model_api_llama_port(port_);

        std::string path_to_load = model_path_;
        if (path_to_load.empty() && !models_dir_.empty()) {
            path_to_load = "";
        }
        // Only a dead process is fatal here: a model that is merely slow keeps loading in the
        // background, and the UI polls /health for the real answer.
        if (restart_llama_server(path_to_load, "", max_context_, "") == ServerReadyState::Exited) {
            if (!path_to_load.empty()) {
                std::cerr << "Failed to start server" << std::endl;
                delta::stop_model_api_server();
                return 1;
            }
            std::cerr << "Warning: initial server start returned failure, but model API will remain running for "
                         "on-demand model loading"
                      << std::endl;
        }

        ModelRuntime::Hooks hooks;
        hooks.stop = [this] { stop_llama_server(); };
        hooks.launch = [this](const RuntimeConfig& config, const ModelRuntime::Cancelled& cancelled) {
            {
                std::lock_guard<std::mutex> lock(errors_mutex_);
                recent_errors_.clear();
            }
            effective_config_ = config;
            std::cerr << "llama_runtime: generation=" << config.generation
                      << " model=" << std::filesystem::path(config.model_path).filename().string()
                      << " context=" << config.context << " parallel=" << config.parallel << " batch=" << config.batch
                      << " microbatch=" << config.microbatch << " gpu_layers=" << config.gpu_layers << std::endl;
            load_cancelled_ = cancelled;
            const auto state = launch_llama_server(models_dir_.empty() ? config.model_path : "", config.model_name,
                                                   config.context, models_dir_.empty() ? config.model_alias : "");
            if (cancelled())
                return;
            if (state != ServerReadyState::Ready)
                throw std::runtime_error("Engine did not become available within 30 seconds.");
            // Confirm exact identity before sending a load request; no request should autoload another model.
            if (llama_server_is_router_) {
                auto initial = probe_model_worker(port_, config, true);
                if (initial.failed)
                    throw std::runtime_error(initial.failure.message);
            }
            if (!request_model_load(port_, config, llama_server_is_router_))
                throw std::runtime_error("Engine rejected the selected model load request.");
        };
        hooks.probe = [this](const RuntimeConfig& config) {
            std::lock_guard<std::mutex> lock(llama_server_mutex_);
            if (!owned_process_.alive()) {
                stop_llama_server_locked();
                auto exit = owned_process_.exit_status();
                WorkerProbeResult failed;
                failed.failed = true;
                failed.engine_available = false;
                failed.failure = {"worker_exit", "Model process exited. Copy the report for details.", exit.code,
                                  exit.signal};
                return failed;
            }
            return probe_model_worker(port_, config, llama_server_is_router_);
        };
        hooks.classify = [this](const RuntimeFailure& original) {
            if (original.code != "worker_exit" && original.code != "runtime")
                return original;
            ProcessEvidence evidence;
            {
                std::lock_guard<std::mutex> lock(errors_mutex_);
                evidence.errors = recent_errors_;
            }
            evidence.exit_code = original.exit_code;
            evidence.signal = original.signal;
            if (evidence.exit_code == 0 && evidence.signal == 0) {
                auto exit = owned_process_.exit_status();
                evidence.exit_code = exit.code;
                evidence.signal = exit.signal;
            }
            auto failure = classify_runtime_failure(evidence);
            if (failure.code == "unknown_exit" && original.code == "runtime")
                failure.message = original.message;
            return failure;
        };
        runtime_ = std::make_unique<ModelRuntime>(std::move(hooks));
        set_model_runtime(runtime_.get(), [this](RuntimeConfig config) {
            config.parallel = max_parallel_;
            auto plan =
                hw_monitor_.plan_tiered_offload(std::filesystem::file_size(config.model_path), 32, config.context);
            config.batch = runtime_env_int("LLAMA_ARG_BATCH", plan.batch);
            config.microbatch = runtime_env_int("LLAMA_ARG_UBATCH", plan.ubatch);
            config.gpu_layers = runtime_env_int("LLAMA_ARG_N_GPU_LAYERS", plan.ngl);
            config.draft_path = draft_model_;
            return config;
        });
        delta::set_model_unload_callback([this]() {
            if (auto s = runtime_->current())
                runtime_->cancel(s->attempt_id);
            this->stop_llama_server();
        });

        delta::start_model_api_server(model_api_port_);

#ifdef _WIN32
        g_win_should_stop = &should_stop_;
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
        g_wrapper_instance = this;
        g_wrapper_stop_requested = 0;
        struct sigaction sa;
        sa.sa_handler = wrapper_signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGHUP, &sa, nullptr);
        sigaction(SIGINT, &sa, nullptr);
#endif

        while (!should_stop_) {
#ifndef _WIN32
            if (g_wrapper_stop_requested) {
                should_stop_ = true;
                break;
            }
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(250));

            // ====================================================================
            // DHATS Brain: self-heal watchdog
            // ====================================================================
            // ModelRuntime owns loading and retries. The legacy watchdog must not race it.
            if (runtime_->current())
                continue;
            std::lock_guard<std::mutex> process_lock(llama_server_mutex_);

            if (!owned_process_.alive())
                stop_llama_server_locked();
        }

        std::cout << "\nStopping server..." << std::endl;
        runtime_->shutdown();
        stop_llama_server();

#ifdef _WIN32
        g_win_should_stop = nullptr;
#else
        g_wrapper_instance = nullptr;
#endif
        delta::stop_model_api_server();
        set_model_runtime(nullptr);

        return 0;
    }
};

} // namespace delta

int main(int argc, char* argv[]) {
    delta::DeltaServerWrapper wrapper;

    std::string model_path;
    std::string models_dir;
    int port = 8080;
    int model_api_port = 8081;
    int max_parallel = 4;
    int max_context = 0;
    bool enable_embedding = false;
    bool enable_reranking = false;
    std::string draft_model;
    std::string grammar_file;
    std::string api_host;
    const char* env_key = std::getenv("DELTA_API_KEY");
    std::string api_key = env_key ? env_key : "";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-m" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--models-dir" && i + 1 < argc) {
            models_dir = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--model-api-port" && i + 1 < argc) {
            model_api_port = std::stoi(argv[++i]);
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
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "delta-server: runs llama-server and Delta's API (normally started by 'delta server').\n"
                         "  -m <path> | --models-dir <dir>   one model, or pick from a folder\n"
                         "  --port <N>                       llama-server and web UI port (default 8080)\n"
                         "  --model-api-port <N>             Delta and OpenAI-compatible API port (default 8081)\n"
                         "  --host <addr>                    API address (default 127.0.0.1; 0.0.0.0 shares it)\n"
                         "  --api-key <key>                  require this key on /v1 (or set DELTA_API_KEY)\n"
                         "Run 'delta server --help' for endpoints and examples."
                      << std::endl;
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            api_host = argv[++i];
        } else if (arg == "--api-key" && i + 1 < argc) {
            api_key = argv[++i];
        }
    }

    delta::set_model_api_overrides(api_host, api_key);
    if (!delta::api::is_loopback(delta::model_api_host()) && delta::model_api_key().empty()) {
        std::cerr << "Sharing the API on the network needs a key: add --api-key <key> (or set DELTA_API_KEY)."
                  << std::endl;
        return 2;
    }

    wrapper.set_model_path(model_path);
    wrapper.set_models_dir(models_dir);
    wrapper.set_port(port);
    wrapper.set_model_api_port(model_api_port);
    wrapper.set_max_parallel(max_parallel);
    wrapper.set_max_context(max_context);
    wrapper.set_embedding(enable_embedding);
    wrapper.set_reranking(enable_reranking);
    wrapper.set_draft_model(draft_model);
    wrapper.set_grammar_file(grammar_file);

    return wrapper.start_server();
}
