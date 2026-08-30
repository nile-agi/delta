/**
 * Delta CLI Server Wrapper
 * Uses Delta web UI from public/ directory (built from assets/)
 */

#include "delta_cli.h"
#include "model_api_server.h"
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <libgen.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
    bool enable_embedding_;
    bool enable_reranking_;
    std::string draft_model_;
    std::string grammar_file_;

    std::thread llama_server_thread_;
    std::atomic<bool> llama_server_running_;
    std::atomic<bool> should_stop_;
#ifdef _WIN32
    HANDLE llama_server_process_;
    DWORD llama_server_pid_;
    HANDLE job_object_;
#else
    pid_t llama_server_pid_;
#endif
    std::mutex llama_server_mutex_;

    // DHATS Brain: persistent hardware monitor (avoids re-probing NVML/ROCm every load)
    delta::HardwareMonitor hw_monitor_;

    // DHATS Brain: self-heal watchdog state
    std::atomic<int> heal_pending_{0};   // 1 = halve GPU layers, 2 = go CPU-only
    std::atomic<int> oom_hits_{0};       // consecutive OOM errors seen in stderr
    int ngl_override_ = -1;              // -1 = trust planner; >=0 = forced override
    int last_ngl_ = 0;
    int heal_count_ = 0;
    std::string last_heal_reason_;

    void note_server_stderr(const std::string& line) {
        static const char* kOom[] = {
            "Insufficient Memory", "OutOfMemory", "failed to fit params",
            "error state from a previous command buffer", "Compute error",
            "common_fit_params: failed", "ggml_metal_synchronize: error"
        };
        for (const char* s : kOom) {
            if (line.find(s) != std::string::npos) {
                int hits = ++oom_hits_;
                if (hits >= 3 && heal_pending_ == 0) {
                    // First OOM burst: halve GPU layers. Second: force CPU-only.
                    heal_pending_ = (heal_count_ == 0) ? 1 : 2;
                }
                return;
            }
        }
    }

  public:
    DeltaServerWrapper()
        : port_(8080), model_api_port_(8081), max_parallel_(4), max_context_(0), enable_embedding_(false),
          enable_reranking_(false), llama_server_running_(false), should_stop_(false)
#ifdef _WIN32
          ,
          llama_server_process_(NULL), llama_server_pid_(0), job_object_(NULL)
#else
          ,
          llama_server_pid_(0)
#endif
    {
#ifdef _WIN32
        job_object_ = CreateJobObjectA(NULL, NULL);
        if (job_object_) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job_object_, JobObjectExtendedLimitInformation, &info, sizeof(info));
        }
#endif
    }

    ~DeltaServerWrapper() {
#ifdef _WIN32
        if (job_object_) {
            CloseHandle(job_object_);
            job_object_ = NULL;
        }
#endif
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

    std::string build_llama_server_command(const std::string& model_path, int ctx_size, const std::string& model_alias) {
        std::string cmd;
#ifdef _WIN32
        cmd = "\"" + llama_server_path_ + "\"";
#else
        cmd = llama_server_path_;
#endif
        if (!model_path.empty()) {
            cmd += " -m \"" + model_path + "\"";
        } else if (!models_dir_.empty()) {
            std::string dir_arg = delta::tools::FileOps::absolute_path(models_dir_);
            if (dir_arg.empty())
                dir_arg = models_dir_;
            cmd += " --models-dir \"" + dir_arg + "\"";
        }
        cmd += " --host 127.0.0.1";
        cmd += " --port " + std::to_string(port_);
        if (ctx_size > 0) {
            cmd += " -c " + std::to_string(ctx_size);
        }

        cmd += " --jinja";
        cmd += " --metrics";
        cmd += " --slots";
        cmd += " --props";
        cmd += " --cache-prompt";

        if (ctx_size > 16384) {
            cmd += " --flash-attn off";
            if (ctx_size > 32768) {
                cmd += " --gpu-layers 0";
            }
        } else {
            cmd += " --flash-attn auto";
        }

        if (!model_alias.empty()) {
            cmd += " --alias \"" + model_alias + "\"";
        }
        std::string webui_path = find_webui_path();
        if (!webui_path.empty()) {
            cmd += " --path \"" + webui_path + "\"";
        }
        if (enable_embedding_)
            cmd += " --embedding";
        if (enable_reranking_)
            cmd += " --reranking";
        if (!draft_model_.empty())
            cmd += " --model-draft \"" + draft_model_ + "\"";
        if (!grammar_file_.empty())
            cmd += " --grammar-file \"" + grammar_file_ + "\"";

        if (enable_embedding_) cmd += " --embedding";
        if (enable_reranking_) cmd += " --reranking";
        
        // ============================================================================
        // ENHANCEMENT D: Model Context Protocol (MCP) Integration
        // llama-server natively supports MCP via the --tools directory argument.
        // ============================================================================
        std::string config_dir = get_executable_dir();
        std::string mcp_dir = config_dir + "/mcp_servers";
        if (!std::filesystem::exists(mcp_dir)) {
            mcp_dir = config_dir + "/tools";
        }
        if (std::filesystem::exists(mcp_dir) && std::filesystem::is_directory(mcp_dir)) {
            cmd += " --tools \"" + mcp_dir + "\"";
            std::cout << "[MCP] Injecting external tools from directory: " << mcp_dir << std::endl;
        }

        // ============================================================================
        // ENHANCEMENT B: Speculative Decoding for Tool Loops
        // If no draft model is explicitly provided, auto-detect a small GGUF in the models directory.
        // ============================================================================
        if (draft_model_.empty()) {
            std::string models_dir_path = models_dir_.empty() ? (get_executable_dir() + "/models") : models_dir_;
            if (std::filesystem::exists(models_dir_path) && std::filesystem::is_directory(models_dir_path)) {
                for (const auto& entry : std::filesystem::directory_iterator(models_dir_path)) {
                    std::string fname = entry.path().filename().string();
                    std::string lower_fname = fname;
                    for (auto& c : lower_fname) c = std::tolower(static_cast<unsigned char>(c));
                    
                    bool is_small = (lower_fname.find("0.5b") != std::string::npos || 
                                     lower_fname.find("1.5b") != std::string::npos ||
                                     lower_fname.find("tinyllama") != std::string::npos || 
                                     lower_fname.find("smollm") != std::string::npos ||
                                     lower_fname.find("draft") != std::string::npos);
                                     
                    if (is_small && lower_fname.find(".gguf") != std::string::npos) {
                        draft_model_ = entry.path().string();
                        std::cout << "[Speculative] Auto-detected draft model: " << draft_model_ << std::endl;
                        break;
                    }
                }
            }
        }
        if (!draft_model_.empty()) {
            cmd += " --model-draft \"" + draft_model_ + "\"";
            cmd += " --draft-max 16"; // Standard draft token limit for speculative decoding
        }
        
        if (!grammar_file_.empty()) cmd += " --grammar-file \"" + grammar_file_ + "\"";

        // ============================================================================
        // DHATS: Inject RPC Arguments
        // ============================================================================
        auto rpc_nodes = agent::AgentDatabase::instance().get_enabled_rpc_nodes();
        if (!rpc_nodes.empty()) {
            std::string rpc_arg = " --rpc ";
            for (size_t i = 0; i < rpc_nodes.size(); ++i) {
                rpc_arg += rpc_nodes[i].endpoint;
                if (i < rpc_nodes.size() - 1) rpc_arg += ",";
            }
            cmd += rpc_arg;
            std::cout << "[DHATS] Injecting " << rpc_nodes.size() << " RPC worker node(s) into llama-server context." << std::endl;
        }

        // ============================================================================
        // DHATS Brain: Intelligent multi-tier distribution
        // ============================================================================
        long long model_size_bytes = 0;
        int n_layers = 32;
        try {
            if (!model_path.empty() && std::filesystem::exists(model_path)) {
                model_size_bytes = std::filesystem::file_size(model_path);
            }
        } catch (...) {}

        if (model_size_bytes > 0) {
            auto plan = hw_monitor_.plan_tiered_offload(model_size_bytes, n_layers, ctx_size);
            
            int ngl = plan.ngl;
            if (ngl_override_ >= 0) {
                ngl = std::min(ngl, ngl_override_);
            }
            last_ngl_ = ngl;

            if (plan.cpu_only && ngl == 0) {
                cmd += " -ngl 0";
                std::cout << "[DHATS] CPU-only mode (no GPU available or insufficient VRAM)." << std::endl;
            } else if (plan.all_layers) {
                cmd += " -ngl 999";
                cmd += " --batch-size " + std::to_string(plan.batch);
                cmd += " --ubatch-size " + std::to_string(plan.ubatch);
                std::cout << "[DHATS] All layers on GPU (" << plan.gpu_layers << " layers, " 
                          << std::fixed << std::setprecision(1) << plan.gpu_mem_needed << "GB)" << std::endl;
            } else {
                cmd += " -ngl " + std::to_string(ngl);
                cmd += " --batch-size " + std::to_string(plan.batch);
                cmd += " --ubatch-size " + std::to_string(plan.ubatch);
                std::cout << "[DHATS] Split offload: " << plan.gpu_layers << " layers on GPU ("
                          << std::fixed << std::setprecision(1) << plan.gpu_mem_needed << "GB), "
                          << plan.cpu_layers << " layers on CPU (" << plan.cpu_mem_needed << "GB)" << std::endl;
            }

            // Log efficiency warning
            if (!plan.efficient && !plan.efficiency_warning.empty()) {
                std::cerr << "[DHATS] ⚠ " << plan.efficiency_warning << std::endl;
                if (!plan.recommendation.empty()) {
                    std::cerr << "[DHATS] Recommendation: " << plan.recommendation << std::endl;
                }
                delta::report_heal(heal_count_, ngl, plan.efficiency_warning);
            } else {
                delta::report_heal(heal_count_, ngl, "");
            }
        }

        return cmd;
    }

    void stop_llama_server_locked() {
#ifdef _WIN32
        if (llama_server_process_ != NULL) {
            TerminateProcess(llama_server_process_, 0);
            WaitForSingleObject(llama_server_process_, 5000);
            CloseHandle(llama_server_process_);
            llama_server_process_ = NULL;
            llama_server_pid_ = 0;
            llama_server_running_ = false;
        }
#else
        if (llama_server_pid_ != 0) {
            pid_t pid_to_kill = (llama_server_pid_ < 0) ? llama_server_pid_ : llama_server_pid_;
            kill(pid_to_kill, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            int status;
            pid_t actual_pid = (llama_server_pid_ < 0) ? -llama_server_pid_ : llama_server_pid_;
            if (waitpid(actual_pid, &status, WNOHANG) == 0) {
                kill(pid_to_kill, SIGKILL);
                waitpid(actual_pid, &status, 0);
            }
            llama_server_pid_ = 0;
            llama_server_running_ = false;
        }
#endif
    }

    void stop_llama_server() {
        std::lock_guard<std::mutex> lock(llama_server_mutex_);
        stop_llama_server_locked();
    }

    bool restart_llama_server(const std::string& model_path, const std::string& model_name, int ctx_size, const std::string& model_alias) {
        
        // Make a mutable local copy — model_path parameter is const std::string&
        std::string model_path_to_use = model_path;
        
        std::lock_guard<std::mutex> lock(llama_server_mutex_);

        if (llama_server_running_ && !model_path_.empty() && !model_path_to_use.empty() && model_path_ == model_path_to_use) {
            return true;
        }

        if (llama_server_running_ && !models_dir_.empty() && !model_path_to_use.empty()) {
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
                if (same_dir) {
                    if (!model_name.empty()) {
                        std::cout << "Selected model: " << model_name << std::endl;
                    }
                    model_path_ = model_path_to_use;
                    return true;
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
            try { size_bytes = std::filesystem::file_size(model_path_to_use); } catch (...) {}

            if (size_bytes > 0) {
                auto compat = hw_monitor_.check_model_compatibility(
                    size_bytes, estimate_model_layers(size_bytes), ctx_size);

                if (!compat.can_run) {
                    delta::report_model_block({true, model_name,
                                            compat.warning,
                                            compat.recommendation,
                                            compat.suggested_context});
                    std::cerr << "[DHATS] ❌ BLOCKED: " << model_name << " — "
                            << compat.warning << std::endl;
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

#ifdef _WIN32
        if (llama_server_running_ && llama_server_process_ != NULL) {
#else
        if (llama_server_running_ && llama_server_pid_ != 0) {
#endif
            stop_llama_server_locked();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        model_path_ = model_path_to_use;
        max_context_ = ctx_size;

        std::string cmd = build_llama_server_command(model_path_to_use, ctx_size, model_alias);

#ifdef _WIN32
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = NULL;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        std::vector<char> cmd_line(cmd.begin(), cmd.end());
        cmd_line.push_back('\0');

        std::string work_dir = get_executable_dir();
        const char* work_dir_p = work_dir.empty() ? NULL : work_dir.c_str();

        if (CreateProcessA(NULL, cmd_line.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL,
                           work_dir_p, &si, &pi)) {
            CloseHandle(pi.hThread);
            llama_server_process_ = pi.hProcess;
            llama_server_pid_ = pi.dwProcessId;
            llama_server_running_ = true;
            if (job_object_) {
                AssignProcessToJobObject(job_object_, pi.hProcess);
            }

            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            bool port_ok = false;
            for (int attempt = 0; attempt < 120; ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                DWORD ec;
                if (GetExitCodeProcess(pi.hProcess, &ec) && ec != STILL_ACTIVE)
                    break;
                SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock == INVALID_SOCKET)
                    continue;
                struct sockaddr_in addr{};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(static_cast<u_short>(port_));
                addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                    closesocket(sock);
                    port_ok = true;
                    break;
                }
                closesocket(sock);
            }
            WSACleanup();

            if (port_ok) {
                std::cout << "Server ready" << std::endl;
                return true;
            }
            DWORD exit_code;
            if (GetExitCodeProcess(pi.hProcess, &exit_code) && exit_code == STILL_ACTIVE) {
                std::cout << "Server ready (process running)" << std::endl;
                return true;
            }
            std::cerr << "Failed to start server" << std::endl;
            CloseHandle(pi.hProcess);
            llama_server_process_ = NULL;
            llama_server_pid_ = 0;
            llama_server_running_ = false;
            return false;
        } else {
            std::cerr << "Failed to create process" << std::endl;
            return false;
        }
#else
        int out_pipe[2] = {-1, -1};
        bool has_pipe = (pipe(out_pipe) == 0);

        pid_t pid = fork();
        if (pid == 0) {
            if (has_pipe)
                close(out_pipe[0]);
            setsid();
            if (has_pipe) {
                dup2(out_pipe[1], STDOUT_FILENO);
                dup2(out_pipe[1], STDERR_FILENO);
                close(out_pipe[1]);
            } else {
                int dn = open("/dev/null", O_WRONLY);
                if (dn >= 0) {
                    dup2(dn, STDOUT_FILENO);
                    dup2(dn, STDERR_FILENO);
                    close(dn);
                }
            }
            execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)NULL);
            _exit(1);
        } else if (pid > 0) {
            if (has_pipe)
                close(out_pipe[1]);
            llama_server_pid_ = -pid;
            llama_server_running_ = true;

            if (has_pipe) {
                int stats_fd = out_pipe[0];
                DeltaServerWrapper* self = this;
                std::thread([stats_fd, self]() {
                    FILE* f = fdopen(stats_fd, "r");
                    if (!f) {
                        close(stats_fd);
                        return;
                    }
                    char line[4096];
                    double prompt_ms = 0;
                    int prompt_tokens = 0;
                    double gen_ms = 0;
                    int gen_tokens = 0;
                    while (fgets(line, sizeof(line), f)) {
                        if (std::strstr(line, "prompt eval time") && std::strstr(line, "=")) {
                            char* eq = std::strstr(line, "=");
                            double ms;
                            int tokens;
                            if (std::sscanf(eq, "= %lf ms / %d tokens", &ms, &tokens) == 2) {
                                prompt_ms = ms;
                                prompt_tokens = tokens;
                            }
                        } else if (std::strstr(line, "eval time") && !std::strstr(line, "prompt eval time") &&
                                   std::strstr(line, "=")) {
                            char* eq = std::strstr(line, "=");
                            double ms;
                            int tokens;
                            if (std::sscanf(eq, "= %lf ms / %d tokens", &ms, &tokens) == 2) {
                                gen_ms = ms;
                                gen_tokens = tokens;
                            }
                        } else if (std::strstr(line, "total time") && std::strstr(line, "=")) {
                            if (gen_tokens > 0) {
                                double ttft_s = prompt_ms / 1000.0;
                                double tps = (gen_ms > 0) ? (gen_tokens * 1000.0 / gen_ms) : 0;
                                char buf[256];
                                std::snprintf(buf, sizeof(buf), "  %d in / %d out | ttft %.2fs | %.1f tok/s",
                                              prompt_tokens, gen_tokens, ttft_s, tps);
                                std::puts(buf);
                                std::fflush(stdout);
                            }
                            prompt_ms = 0;
                            prompt_tokens = 0;
                            gen_ms = 0;
                            gen_tokens = 0;
                        } else {
                            std::string s(line);
                            // DHATS Brain: feed stderr to OOM detector
                            self->note_server_stderr(s);

                            if (s.find("error") != std::string::npos ||
                                s.find("Error") != std::string::npos ||
                                s.find("unknown") != std::string::npos ||
                                s.find("fail") != std::string::npos ||
                                s.find("Fail") != std::string::npos ||
                                s.find("warning") != std::string::npos ||
                                s.find("Warning") != std::string::npos) {
                                std::cerr << "[llama-server] " << line << std::flush;
                            }
                        }
                    }
                    std::fclose(f);
                }).detach();
            }

            bool port_ok = false;
            for (int attempt = 0; attempt < 24; ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                int sock = socket(AF_INET, SOCK_STREAM, 0);
                if (sock < 0)
                    continue;
                struct sockaddr_in addr{};
                addr.sin_family = AF_INET;
                addr.sin_port = htons(port_);
                addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                    close(sock);
                    port_ok = true;
                    break;
                }
                close(sock);
            }

            if (port_ok) {
                std::cout << "Server ready" << std::endl;
                return true;
            }

            int status;
            if (waitpid(pid, &status, WNOHANG) == 0) {
                std::cout << "Server ready (process running)" << std::endl;
                return true;
            } else {
                std::cerr << "Failed to start server" << std::endl;
                llama_server_running_ = false;
                llama_server_pid_ = 0;
                return false;
            }
        } else {
            std::cerr << "Failed to fork process" << std::endl;
            return false;
        }
#endif
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
        std::cout << "  Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;

        delta::start_model_api_server(model_api_port_);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        delta::set_model_switch_callback([this](const std::string& model_path, const std::string& model_name,
                                                int ctx_size, const std::string& model_alias) -> bool {
            return this->restart_llama_server(model_path, model_name, ctx_size, model_alias);
        });
        delta::set_model_unload_callback([this]() { this->stop_llama_server(); });

        std::string path_to_load = model_path_;
        if (path_to_load.empty() && !models_dir_.empty()) {
            path_to_load = "";
        }
        if (!restart_llama_server(path_to_load, "", max_context_, "")) {
            if (!path_to_load.empty()) {
                std::cerr << "Failed to start server" << std::endl;
                delta::stop_model_api_server();
                return 1;
            }
            std::cerr << "Warning: initial server start returned failure, but model API will remain running for "
                         "on-demand model loading"
                      << std::endl;
        }

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
            int hp = heal_pending_.exchange(0);
            if (hp && !model_path_.empty()) {
                heal_count_++;
                oom_hits_ = 0;
                if (hp == 1) {
                    ngl_override_ = std::max(0, last_ngl_ / 2);
                    last_heal_reason_ = "GPU out-of-memory — halved GPU layers";
                } else {
                    ngl_override_ = 0;
                    last_heal_reason_ = "GPU out-of-memory — fell back to CPU";
                }
                std::cout << "[DHATS] Self-heal #" << heal_count_
                          << ": restarting llama-server with -ngl " << ngl_override_ << std::endl;
                restart_llama_server(model_path_, "", max_context_, "");
            }

#ifdef _WIN32
            if (llama_server_process_ != NULL) {
                DWORD exit_code;
                if (GetExitCodeProcess(llama_server_process_, &exit_code) && exit_code != STILL_ACTIVE) {
                    CloseHandle(llama_server_process_);
                    llama_server_process_ = NULL;
                    llama_server_pid_ = 0;
                    llama_server_running_ = false;
                }
            }
#else
            if (llama_server_pid_ != 0) {
                pid_t actual_pid = (llama_server_pid_ < 0) ? -llama_server_pid_ : llama_server_pid_;
                int status;
                if (waitpid(actual_pid, &status, WNOHANG) != 0) {
                    llama_server_pid_ = 0;
                    llama_server_running_ = false;
                }
            }
#endif
        }

        std::cout << "\nStopping server..." << std::endl;
        stop_llama_server();

#ifdef _WIN32
        g_win_should_stop = nullptr;
#else
        g_wrapper_instance = nullptr;
#endif
        delta::stop_model_api_server();

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
        }
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