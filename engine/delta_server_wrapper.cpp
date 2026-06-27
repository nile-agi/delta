/**
 * Delta CLI Server Wrapper
 * Uses Delta web UI from public/ directory (built from assets/)
 */

#include "delta_cli.h"
#include "model_api_server.h"
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
// Signal handler sets this so the run loop can stop llama-server and exit
static volatile sig_atomic_t g_wrapper_stop_requested = 0;
static DeltaServerWrapper* g_wrapper_instance = nullptr;

static void wrapper_signal_handler(int) {
    g_wrapper_stop_requested = 1;
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
    std::string models_dir_; // Router mode: directory to scan for .gguf (no -m)
    int port_;
    int model_api_port_;
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
#ifdef _WIN32
    HANDLE llama_server_process_;
    DWORD llama_server_pid_;
#else
    pid_t llama_server_pid_;
#endif
    std::mutex llama_server_mutex_;

  public:
    DeltaServerWrapper()
        : port_(8080), model_api_port_(8081), max_parallel_(4), max_context_(0), enable_embedding_(false),
          enable_reranking_(false), llama_server_running_(false), should_stop_(false)
#ifdef _WIN32
          ,
          llama_server_process_(NULL), llama_server_pid_(0)
#else
          ,
          llama_server_pid_(0)
#endif
    {
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
        // Only search for the real llama.cpp HTTP server binary ('server' or 'llama-server').
        // Do not include delta-server: we must not run ourselves or another wrapper (avoids recursion/chains).
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
        }
        possible_paths.push_back("server");
        possible_paths.push_back("llama-server");
        possible_paths.push_back("./server");
        possible_paths.push_back("./llama-server");
        possible_paths.push_back("/opt/homebrew/bin/server");
        possible_paths.push_back("/opt/homebrew/bin/llama-server");
        possible_paths.push_back("/usr/local/bin/server");
        possible_paths.push_back("/usr/local/bin/llama-server");
        possible_paths.push_back("/usr/bin/server");
        possible_paths.push_back("/usr/bin/llama-server");

        for (const auto& path : possible_paths) {
            if (!std::filesystem::exists(path))
                continue;
            std::string resolved = resolve_path(path);
            if (!resolved.empty() && resolved == self_path)
                continue; // symlink/hardlink to ourselves
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
        // Find the Delta web UI directory (from public/ only, not llama.cpp web UI)
        std::vector<std::string> candidates;

        // CWD-based candidates first so "delta-server" from project root or build/ finds public/
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
            // Check same directory as executable (Windows Tauri bundles)
            candidates.push_back(exe_path + "/webui");
            candidates.push_back(exe_path + "/public");
            // Check relative to executable (Delta web UI from public/)
            candidates.push_back(exe_path + "/../public");
            candidates.push_back(exe_path + "/../../public");
            candidates.push_back(exe_path + "/../../../public");
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

        return ""; // Not found, server will use embedded UI
    }

    std::string build_llama_server_command(const std::string& model_path, int ctx_size,
                                           const std::string& model_alias) {
        // On Windows, quote the executable path so CreateProcess parses it correctly when path contains spaces (e.g.
        // "C:\Program Files\Delta\server.exe")
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
        cmd += " --host 0.0.0.0";
        cmd += " --port " + std::to_string(port_);
        if (ctx_size > 0) {
            cmd += " -c " + std::to_string(ctx_size);
        }
        // Minimal flags for compatibility; avoid --flash-attn/--jinja which some builds don't support
        if (ctx_size > 16384) {
            cmd += " --gpu-layers 0";
        }

        // Optimize batch sizes for large prompt processing (like LlamaBarn)
        // Larger ubatch-size significantly improves prompt processing speed for large prompts
        // Default ubatch-size is 512, but 1024-2048 provides better throughput for 20k+ token prompts
        if (ctx_size >= 8192) {
            // For large contexts, use larger batch sizes to improve prompt processing speed
            cmd += " --ubatch-size 2048"; // Physical batch size - processes more tokens per batch
            cmd += " --batch-size 4096";  // Logical batch size - allows larger batches
        } else if (ctx_size >= 4096) {
            // Medium contexts get moderate batch size increase
            cmd += " --ubatch-size 1024";
            cmd += " --batch-size 2048";
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
            cmd += " --md \"" + draft_model_ + "\"";
        if (!grammar_file_.empty())
            cmd += " --grammar-file \"" + grammar_file_ + "\"";
        return cmd;
    }

    // Internal stop — caller must already hold llama_server_mutex_
    void stop_llama_server_locked() {
#ifdef _WIN32
        if (llama_server_process_ != NULL) {
            TerminateProcess(llama_server_process_, 0);
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

    bool restart_llama_server(const std::string& new_model_path, const std::string& model_name, int ctx_size,
                              const std::string& model_alias) {
        std::lock_guard<std::mutex> lock(llama_server_mutex_);

        // Skip restart if the same model is already loaded and running
        if (llama_server_running_ && !model_path_.empty() && !new_model_path.empty() && model_path_ == new_model_path) {
            return true;
        }

        // In router mode, no restart needed — router loads models on demand
        if (llama_server_running_ && !models_dir_.empty() && !new_model_path.empty()) {
            try {
                std::filesystem::path model_parent = std::filesystem::path(new_model_path).parent_path();
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
                    model_path_ = new_model_path;
                    return true;
                }
            } catch (...) {
            }
        }

        if (!model_name.empty()) {
            std::cout << "Loading model: " << model_name << std::endl;
            if (!new_model_path.empty()) {
                try {
                    auto fsize = std::filesystem::file_size(new_model_path);
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

        // Stop current llama-server
#ifdef _WIN32
        if (llama_server_running_ && llama_server_process_ != NULL) {
#else
        if (llama_server_running_ && llama_server_pid_ != 0) {
#endif
            stop_llama_server_locked();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        // Update model path
        model_path_ = new_model_path;
        max_context_ = ctx_size;

        // Build new command
        std::string cmd = build_llama_server_command(new_model_path, ctx_size, model_alias);

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
                std::thread([stats_fd]() {
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
                        }
                    }
                    std::fclose(f);
                }).detach();
            }

            // Wait for the server to become reachable on its port
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

        // When no -m: start with --models-dir so UI opens and user picks a model
        if (model_path_.empty() && models_dir_.empty()) {
            // Default models directory
            std::string home = delta::tools::FileOps::get_home_dir();
            models_dir_ =
                delta::tools::FileOps::join_path(delta::tools::FileOps::join_path(home, ".delta-cli"), "models");
        }
        if (!model_path_.empty()) {
            std::string abs_path = delta::tools::FileOps::absolute_path(model_path_);
            if (!abs_path.empty())
                model_path_ = abs_path;
        }

        // Find and use Delta web UI
        std::string webui_path = find_webui_path();

        std::cout << R"(
 ██████╗ ███████╗██╗  ████████╗ █████╗      ██████╗██╗     ██╗
 ██╔══██╗██╔════╝██║  ╚══██╔══╝██╔══██╗    ██╔════╝██║     ██║
 ██║  ██║█████╗  ██║     ██║   ███████║    ██║     ██║     ██║
 ██║  ██║██╔══╝  ██║     ██║   ██╔══██║    ██║     ██║     ██║
 ██████╔╝███████╗███████╗██║   ██║  ██║    ╚██████╗███████╗██║
 ╚═════╝ ╚══════╝╚══════╝╚═╝   ╚═╝  ╚═╝     ╚═════╝╚══════╝╚═╝
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

#ifndef _WIN32
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

        // Keep running until signal (llama-server may be loaded/unloaded via model API)
        while (!should_stop_) {
#ifndef _WIN32
            if (g_wrapper_stop_requested) {
                std::cout << "\nStopping server..." << std::endl;
                stop_llama_server();
                break;
            }
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
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

#ifndef _WIN32
        g_wrapper_instance = nullptr;
#endif
        // Stop model API server when delta-server exits
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
