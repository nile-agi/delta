/**
 * Interactive Commands - Slash command system implementation
 */

 #include "commands.h"
 #include "update.h"
 #include "history.h"
 #include "model_api_server.h"
 #include <iostream>
 #include <sstream>
 #include <thread>
 #include <chrono>
 #include <fstream>
 #include <map>
 #include <iomanip>
 #include <algorithm>
 #include <cctype>
 #include <cstdio>
 #include <memory>
 #include <vector>
 #include <mutex>
#ifdef _WIN32
    #include <windows.h>
    #include <limits.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <io.h>
    #include <direct.h>
    #include <process.h>
    typedef DWORD pid_t;
    #define WNOHANG 1
    #define WIFEXITED(status) ((status) != STILL_ACTIVE)
    #define WEXITSTATUS(status) (status)
    #define SIGTERM 15
    #define SIGKILL 9
#else
     #include <signal.h>
     #include <sys/wait.h>
     #include <fcntl.h>
     #include <unistd.h>
     #include <limits.h>
     #include <sys/socket.h>
     #include <netinet/in.h>
     #include <arpa/inet.h>
     #ifndef PATH_MAX
         #define PATH_MAX 4096
     #endif
 #endif
 
 namespace delta {
 
// Static member initialization
std::map<std::string, CommandHandler> Commands::command_map_;
bool Commands::initialized_ = false;
process_id_t Commands::llama_server_pid_ = 0;
std::string Commands::current_model_path_ = "";
int Commands::current_port_ = 8080;
std::mutex Commands::server_mutex_;

// Launch server automatically (public method)
bool Commands::launch_server_auto(const std::string& model_path, int port, int ctx_size, const std::string& model_alias) {
    port = 8080;  // Single port for macOS, Linux, Windows
    // Prefer "server" (llama.cpp HTTP server); fallback to delta-server wrapper
    std::vector<std::string> server_candidates;
    std::string exe_dir = tools::FileOps::get_executable_dir();
#ifdef _WIN32
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "server.exe"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server.exe"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../server.exe"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../delta-server.exe"));
#else
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "server"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "llama-server"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../server"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../llama-server"));
    server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../delta-server"));
#endif
    server_candidates.push_back("/opt/homebrew/bin/server");
    server_candidates.push_back("/opt/homebrew/bin/llama-server");
    server_candidates.push_back("/opt/homebrew/bin/delta-server");
    server_candidates.push_back("/usr/local/bin/server");
    server_candidates.push_back("/usr/local/bin/llama-server");
    server_candidates.push_back("/usr/local/bin/delta-server");
    server_candidates.push_back("/usr/bin/server");
    server_candidates.push_back("/usr/bin/llama-server");
    server_candidates.push_back("/usr/bin/delta-server");
#ifdef _WIN32
    server_candidates.push_back("C:\\Program Files\\Delta CLI\\server.exe");
    server_candidates.push_back("C:\\Program Files\\Delta CLI\\delta-server.exe");
    server_candidates.push_back("server.exe");
#else
    server_candidates.push_back("server");
    server_candidates.push_back("llama-server");
#endif
    server_candidates.push_back("delta-server");

    std::string server_bin;
     for (const auto& candidate : server_candidates) {
         if (tools::FileOps::file_exists(candidate)) {
             server_bin = candidate;
             break;
         }
     }
     
     if (server_bin.empty()) {
         UI::print_error("HTTP server binary not found. Looked for 'server' and 'delta-server' in PATH and install locations.");
         UI::print_info("From source: run 'make install' so the 'server' binary is installed. Homebrew: run 'brew reinstall delta-cli'.");
         UI::print_info("Ensure vendor/llama.cpp exists (git submodule update --init vendor/llama.cpp) and rebuild.");
         return false;
     }
     
     // Verify model path exists
     if (!tools::FileOps::file_exists(model_path)) {
         // Model path is invalid
         return false;
     }
     
     // Get the path to the web UI directory (use Delta web UI from public/ only)
     std::string public_path;
     
     // Get executable directory and project root
     std::string exe_parent = tools::FileOps::join_path(exe_dir, "..");
     std::string exe_grandparent = tools::FileOps::join_path(exe_parent, "..");
     
     // CWD-based candidates first so "delta server" from project root or build/ finds public/
     std::vector<std::string> public_candidates;
#ifdef _WIN32
     {
         char cwd[MAX_PATH];
         if (_getcwd(cwd, MAX_PATH) != nullptr) {
             std::string cwd_str(cwd);
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "public"));
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "..\\public"));
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "webui"));
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "..\\webui"));
         }
     }
#else
     {
         char cwd[PATH_MAX];
         if (getcwd(cwd, sizeof(cwd)) != nullptr) {
             std::string cwd_str(cwd);
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "public"));
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "../public"));
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "webui"));
             public_candidates.push_back(tools::FileOps::join_path(cwd_str, "../webui"));
         }
     }
#endif
     // Relative to executable (Delta web UI from public/)
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../public"));
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../../public"));
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../../../public"));
     public_candidates.push_back(tools::FileOps::join_path(exe_grandparent, "public"));
     // Current directory (when running from project root)
     public_candidates.push_back("public");
     public_candidates.push_back("./public");
     public_candidates.push_back("../public");
     // Homebrew / install locations
     public_candidates.push_back("/opt/homebrew/share/delta-cli/webui");
     public_candidates.push_back("/usr/local/share/delta-cli/webui");
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../../share/delta-cli/webui"));
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../../../share/delta-cli/webui"));
     // macOS app bundle
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../Resources/webui"));
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../../Resources/webui"));
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../webui"));
     public_candidates.push_back(tools::FileOps::join_path(exe_dir, "../../webui"));
     public_candidates.push_back("webui");
     public_candidates.push_back("./webui");
     public_candidates.push_back("../webui");
     
     for (const auto& candidate : public_candidates) {
         if (tools::FileOps::dir_exists(candidate)) {
             // Check for index.html.gz first (preferred), then index.html
             std::string index_file_gz = tools::FileOps::join_path(candidate, "index.html.gz");
             std::string index_file = tools::FileOps::join_path(candidate, "index.html");
             if (tools::FileOps::file_exists(index_file_gz) || tools::FileOps::file_exists(index_file)) {
                 public_path = candidate;
                // Convert to absolute path for reliability
#ifdef _WIN32
                if (!public_path.empty() && !(public_path.length() >= 2 && public_path[1] == ':')) {
                    char resolved[MAX_PATH];
                    if (_fullpath(resolved, public_path.c_str(), MAX_PATH) != nullptr) {
                        public_path = std::string(resolved);
                    } else {
                        // Try relative to current working directory
                        char cwd[MAX_PATH];
                        if (_getcwd(cwd, MAX_PATH) != nullptr) {
                            std::string full_path = tools::FileOps::join_path(std::string(cwd), public_path);
                            if (_fullpath(resolved, full_path.c_str(), MAX_PATH) != nullptr) {
                                public_path = std::string(resolved);
                            }
                        }
                    }
                }
#else
                if (!public_path.empty() && public_path[0] != '/') {
                    char resolved[PATH_MAX];
                    if (realpath(public_path.c_str(), resolved) != nullptr) {
                        public_path = std::string(resolved);
                    } else {
                        // Try relative to current working directory
                        char cwd[PATH_MAX];
                        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                            std::string full_path = tools::FileOps::join_path(std::string(cwd), public_path);
                            if (realpath(full_path.c_str(), resolved) != nullptr) {
                                public_path = std::string(resolved);
                            }
                        }
                    }
                }
#endif
                 break;
             }
         }
     }
     
    // Convert relative path to absolute path for delta-server
#ifdef _WIN32
    if (!public_path.empty() && !(public_path.length() >= 2 && public_path[1] == ':')) {
        // Try multiple strategies to get absolute path
        char resolved_path[MAX_PATH];
        bool resolved = false;
        
        // Strategy 1: Try _fullpath on the path directly (if it exists from current dir)
        if (_fullpath(resolved_path, public_path.c_str(), MAX_PATH) != nullptr) {
            public_path = std::string(resolved_path);
            resolved = true;
        }
        
        // Strategy 2: Try relative to current working directory
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
        
        // Strategy 3: Try relative to executable directory
        if (!resolved) {
            std::string exe_based_path = tools::FileOps::join_path(exe_dir, public_path);
            if (_fullpath(resolved_path, exe_based_path.c_str(), MAX_PATH) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // Strategy 4: Try relative to project root (exe_grandparent)
        if (!resolved) {
            std::string project_path = tools::FileOps::join_path(exe_grandparent, public_path);
            if (_fullpath(resolved_path, project_path.c_str(), MAX_PATH) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // If all else fails, construct absolute path manually from exe_dir
        if (!resolved) {
            // Build absolute path from executable directory
            std::string abs_path = tools::FileOps::join_path(exe_dir, public_path);
            // Normalize the path (remove .. and .)
            char normalized[MAX_PATH];
            if (_fullpath(normalized, abs_path.c_str(), MAX_PATH) != nullptr) {
                public_path = std::string(normalized);
            } else {
                // Last resort: use exe_grandparent
                abs_path = tools::FileOps::join_path(exe_grandparent, public_path);
                if (_fullpath(normalized, abs_path.c_str(), MAX_PATH) != nullptr) {
                    public_path = std::string(normalized);
                }
            }
        }
    }
#else
    if (!public_path.empty() && public_path[0] != '/') {
        // Try multiple strategies to get absolute path
        char resolved_path[PATH_MAX];
        bool resolved = false;
        
        // Strategy 1: Try realpath on the path directly (if it exists from current dir)
        if (realpath(public_path.c_str(), resolved_path) != nullptr) {
            public_path = std::string(resolved_path);
            resolved = true;
        }
        
        // Strategy 2: Try relative to current working directory
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
        
        // Strategy 3: Try relative to executable directory
        if (!resolved) {
            std::string exe_based_path = tools::FileOps::join_path(exe_dir, public_path);
            if (realpath(exe_based_path.c_str(), resolved_path) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // Strategy 4: Try relative to project root (exe_grandparent)
        if (!resolved) {
            std::string project_path = tools::FileOps::join_path(exe_grandparent, public_path);
            if (realpath(project_path.c_str(), resolved_path) != nullptr) {
                public_path = std::string(resolved_path);
                resolved = true;
            }
        }
        
        // If all else fails, construct absolute path manually from exe_dir
        if (!resolved) {
            // Build absolute path from executable directory
            std::string abs_path = tools::FileOps::join_path(exe_dir, public_path);
            // Normalize the path (remove .. and .)
            char normalized[PATH_MAX];
            if (realpath(abs_path.c_str(), normalized) != nullptr) {
                public_path = std::string(normalized);
            } else {
                // Last resort: use exe_grandparent
                abs_path = tools::FileOps::join_path(exe_grandparent, public_path);
                if (realpath(abs_path.c_str(), normalized) != nullptr) {
                    public_path = std::string(normalized);
                }
            }
        }
     }
#endif
     
     // Server must have a web UI path or it returns 404 for / and all static requests
     if (public_path.empty()) {
         UI::print_error("Web UI directory not found. Looked for public/ or share/delta-cli/webui.");
         UI::print_info("Run from project root (where public/ exists) or install delta-cli so the web UI is in share/delta-cli/webui.");
         UI::print_info("Build the web UI first: cd assets && npm install && npm run build");
         return false;
     }
     
     // Stop existing delta-server if running
     stop_llama_server();
     
     // Build command
     std::string cmd_str = build_llama_server_cmd(server_bin, model_path, port, ctx_size, model_alias, public_path);
     
    // Create error log file path
#ifdef _WIN32
    char temp_path[MAX_PATH];
    GetTempPathA(MAX_PATH, temp_path);
    std::string err_file = std::string(temp_path) + "delta-server-err-" + std::to_string(port) + ".log";
#else
    std::string err_file = "/tmp/delta-server-err-" + std::to_string(port) + ".log";
#endif
    std::remove(err_file.c_str());
    
    // Start delta-server
#ifdef _WIN32
    // Windows: Use CreateProcess
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    
    // CreateProcess requires a mutable string
    std::vector<char> cmd_line(cmd_str.begin(), cmd_str.end());
    cmd_line.push_back('\0');
    
    if (!CreateProcessA(NULL, cmd_line.data(), NULL, NULL, TRUE, 
                       CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
        UI::print_error("Failed to create process for delta-server (Error: " + std::to_string(GetLastError()) + ")");
        return false;
    }
    
    CloseHandle(pi.hThread);
    
    // Store PID
    {
        std::lock_guard<std::mutex> lock(server_mutex_);
        llama_server_pid_ = pi.dwProcessId;
        current_model_path_ = model_path;
        current_port_ = port;
    }
    
    // Close process handle (we'll use OpenProcess when needed)
    CloseHandle(pi.hProcess);
    
    int result = 0; // Process created successfully
#else
    // Unix: Use fork/exec
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: create new process group
        setsid();
        
        // Redirect output to error log file
        int devnull = open("/dev/null", O_WRONLY);
        int err_fd = open(err_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            if (err_fd >= 0) {
                dup2(err_fd, STDERR_FILENO);
                close(err_fd);
            } else {
                dup2(devnull, STDERR_FILENO);
            }
            close(devnull);
        }
        close(STDIN_FILENO);
        
        // Execute delta-server
        execl("/bin/sh", "sh", "-c", cmd_str.c_str(), (char*)NULL);
        _exit(1);
    } else if (pid > 0) {
        // Parent process: store PID
        std::lock_guard<std::mutex> lock(server_mutex_);
        llama_server_pid_ = -pid; // Store negative for process group
        current_model_path_ = model_path;
        current_port_ = port;
    } else {
        UI::print_error("Failed to fork process for delta-server");
        return false;
    }
    
    int result = 0; // Fork succeeded
#endif
     
     // Wait for server to start and verify it's running
     // Give server time to initialize (especially for model loading)
     // Models can take 30-60 seconds to load, so we wait up to 60 seconds
     bool server_listening = false;
     UI::print_info("Waiting for server to start (this may take 30-60 seconds while loading the model)...");
     for (int attempt = 0; attempt < 120; attempt++) {  // 120 * 500ms = 60 seconds
         std::this_thread::sleep_for(std::chrono::milliseconds(500));
         
         // Show progress every 10 seconds
         if (attempt > 0 && attempt % 20 == 0) {
             UI::print_info("Still waiting for server... (" + std::to_string(attempt / 2) + " seconds)");
         }
         
         // Check if port is listening using socket connection
 #ifdef _WIN32
         WSADATA wsaData;
         if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
             SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
             if (sock != INVALID_SOCKET) {
                 sockaddr_in addr;
                 addr.sin_family = AF_INET;
                 addr.sin_port = htons(port);
                 inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
                 if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
                     server_listening = true;
                     closesocket(sock);
                 } else {
                     closesocket(sock);
                 }
             }
             WSACleanup();
         }
 #else
         int sock = socket(AF_INET, SOCK_STREAM, 0);
         if (sock >= 0) {
             struct sockaddr_in addr;
             addr.sin_family = AF_INET;
             addr.sin_port = htons(port);
             inet_aton("127.0.0.1", &addr.sin_addr);
             if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                 server_listening = true;
                 close(sock);
             } else {
                 close(sock);
             }
         }
 #endif
         if (server_listening) {
             break;
         }
     }
     
     
     // Check error log for any startup errors BEFORE checking if listening
     bool has_startup_error = false;
     std::string error_message = "";
#ifndef _WIN32
     std::ifstream err_log(err_file);
     if (err_log.is_open()) {
         std::string line;
         std::vector<std::string> error_lines;
         while (std::getline(err_log, line)) {
             if (!line.empty()) {
                 // Filter out informational messages
                 std::string line_lower = line;
                 std::transform(line_lower.begin(), line_lower.end(), line_lower.begin(), ::tolower);
                 // Look for actual error messages (not just info)
                 if (line_lower.find("error") != std::string::npos ||
                     line_lower.find("failed") != std::string::npos ||
                     line_lower.find("fatal") != std::string::npos ||
                     (line_lower.find("unknown") != std::string::npos && line_lower.find("option") != std::string::npos) ||
                     line_lower.find("cannot") != std::string::npos ||
                     line_lower.find("unable") != std::string::npos) {
                     error_lines.push_back(line);
                 }
             }
         }
         err_log.close();
         
         if (!error_lines.empty()) {
             has_startup_error = true;
             error_message = error_lines[0];  // Show first error
             // Show errors - these indicate the server failed to start
             UI::print_error("Server startup errors detected:");
             for (size_t i = 0; i < error_lines.size() && i < 5; i++) {
                 std::cerr << "  " << error_lines[i] << std::endl;
             }
             std::cerr << "\nFull error log: " << err_file << std::endl;
             std::cerr << "\nTip: If you see 'unknown option' errors, your delta-server build may not support all flags." << std::endl;
         }
     }
#endif
     
     if (result != 0) {
         UI::print_error("Failed to start server process");
         return false;
     }
     
     if (has_startup_error) {
         UI::print_error("Server failed to start due to errors. Check the error log above.");
         return false;
     }
     
     if (!server_listening) {
         UI::print_error("Server process started but port " + std::to_string(port) + " is not listening after 60 seconds");
         UI::print_info("Error log: " + err_file);
         // Dump error log contents so user can see what went wrong
#ifndef _WIN32
         std::ifstream err_read(err_file);
         if (err_read.is_open()) {
             std::string line;
             std::vector<std::string> lines;
             while (std::getline(err_read, line)) {
                 lines.push_back(line);
             }
             err_read.close();
             if (!lines.empty()) {
                 UI::print_info("--- Last 40 lines of server log ---");
                 size_t start = (lines.size() > 40) ? (lines.size() - 40) : 0;
                 for (size_t i = start; i < lines.size(); i++) {
                     std::cerr << "  " << lines[i] << std::endl;
                 }
                 UI::print_info("--- End of server log ---");
             }
         }
#endif
         UI::print_info("You can run the server manually to see errors: delta-server -m <model-path> --port " + std::to_string(port));
         UI::print_info("Or check if the server is running: ps aux | grep delta-server");
         return false;
     }
     
     // Server is confirmed listening - proceed with setup
     UI::print_success("Delta Server started successfully on port " + std::to_string(port));
     UI::print_info("Open: http://localhost:" + std::to_string(port) + "/index.html");
     
     // Start model management API server on port 8081
     std::cerr << "[DEBUG] Starting model API server on port 8081" << std::endl;
     delta::start_model_api_server(8081);
     
     // Set up model switch callback to restart delta-server
     // IMPORTANT: Always set the callback, even if the server was already running
     std::cerr << "[DEBUG] Setting up model switch callback" << std::endl;
     delta::set_model_switch_callback([](const std::string& model_path, const std::string& model_name, 
                                          int ctx_size, const std::string& model_alias) -> bool {
         std::cerr << "[DEBUG] Model switch callback invoked: model=" << model_name 
                   << ", path=" << model_path << std::endl;
         bool result = Commands::restart_llama_server(model_path, model_name, ctx_size, model_alias);
         std::cerr << "[DEBUG] Model switch callback returning: " << (result ? "true" : "false") << std::endl;
         return result;
     });
     std::cerr << "[DEBUG] Model switch callback set successfully" << std::endl;
     
     UI::print_info("Model Management API: http://localhost:8081");
     
     return true;
 }
 
 // Helper to build delta-server command
 // Uses minimal flags for maximum compatibility; some builds may not support --flash-attn or --jinja
std::string Commands::build_llama_server_cmd(const std::string& server_bin, const std::string& model_path, 
                                              int port, int ctx_size, const std::string& model_alias, 
                                              const std::string& public_path) {
    std::stringstream cmd;
    cmd << server_bin
        << " -m \"" << model_path << "\""
        << " --host 0.0.0.0"
        << " --port " << port;
    if (ctx_size > 0) {
        cmd << " -c " << ctx_size;
    }
    
    // Add --path flag to use Delta web UI if found (required for UI to load)
    if (!public_path.empty()) {
        cmd << " --path \"" << public_path << "\"";
    }
    
    // Optional flags - some llama.cpp builds support these
    if (ctx_size > 16384) {
        cmd << " --gpu-layers 0";
    }
     
     // Add --alias if provided
     if (!model_alias.empty()) {
         cmd << " --alias \"" << model_alias << "\"";
     }
     
     return cmd.str();
 }
 
void Commands::stop_llama_server() {
    std::lock_guard<std::mutex> lock(server_mutex_);
    if (llama_server_pid_ != 0) {
#ifdef _WIN32
        // Windows: Use TerminateProcess
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, llama_server_pid_);
        if (hProcess != NULL) {
            // Try graceful termination first (SIGTERM equivalent)
            TerminateProcess(hProcess, 0);
            // Wait a bit for it to terminate
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Check if still running
            DWORD exit_code;
            if (GetExitCodeProcess(hProcess, &exit_code) && exit_code == STILL_ACTIVE) {
                // Force kill
                TerminateProcess(hProcess, 1);
                WaitForSingleObject(hProcess, 5000); // Wait up to 5 seconds
            }
            CloseHandle(hProcess);
        }
#else
        // Unix: Use kill/waitpid
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
#endif
        llama_server_pid_ = 0;
        current_model_path_ = "";
    }
}
 
 bool Commands::restart_llama_server(const std::string& model_path, const std::string& model_name, 
                                      int ctx_size, const std::string& model_alias) {
     std::cerr << "[DEBUG] restart_llama_server called: model=" << model_name << ", path=" << model_path << std::endl;
     
     UI::print_info("🔄 Switching to model: " + model_name);
     UI::print_info("   Path: " + model_path);
     
     // Stop current delta-server (check and stop with lock)
     {
         std::lock_guard<std::mutex> lock(server_mutex_);
         if (llama_server_pid_ != 0) {
             std::cerr << "[DEBUG] Stopping delta-server with PID: " << llama_server_pid_ << std::endl;
#ifdef _WIN32
             // Windows: Use TerminateProcess
             HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, llama_server_pid_);
             if (hProcess != NULL) {
                 TerminateProcess(hProcess, 0);
                 std::this_thread::sleep_for(std::chrono::milliseconds(500));
                 DWORD exit_code;
                 if (GetExitCodeProcess(hProcess, &exit_code) && exit_code == STILL_ACTIVE) {
                     TerminateProcess(hProcess, 1);
                     WaitForSingleObject(hProcess, 5000);
                 }
                 CloseHandle(hProcess);
             }
#else
             // Unix: Use kill/waitpid
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
#endif
             llama_server_pid_ = 0;
             current_model_path_ = "";
             UI::print_info("   Stopping current model...");
         }
     }
     
     // Wait a bit more to ensure port is free
     std::this_thread::sleep_for(std::chrono::milliseconds(1000));
     
     // Now acquire lock for the rest of the function
     std::lock_guard<std::mutex> lock(server_mutex_);
     
     // Use same server binary as launch_server_auto (prefer "server", then delta-server)
     std::vector<std::string> server_candidates;
     std::string exe_dir = tools::FileOps::get_executable_dir();
#ifdef _WIN32
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "server.exe"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server.exe"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../server.exe"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../delta-server.exe"));
#else
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "server"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "llama-server"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "delta-server"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../server"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../llama-server"));
     server_candidates.push_back(tools::FileOps::join_path(exe_dir, "../delta-server"));
#endif
     server_candidates.push_back("/opt/homebrew/bin/server");
     server_candidates.push_back("/opt/homebrew/bin/llama-server");
     server_candidates.push_back("/opt/homebrew/bin/delta-server");
     server_candidates.push_back("/usr/local/bin/server");
     server_candidates.push_back("/usr/local/bin/llama-server");
     server_candidates.push_back("/usr/local/bin/delta-server");
     server_candidates.push_back("/usr/bin/server");
     server_candidates.push_back("/usr/bin/llama-server");
     server_candidates.push_back("/usr/bin/delta-server");
#ifdef _WIN32
     server_candidates.push_back("server.exe");
     server_candidates.push_back("llama-server.exe");
#else
     server_candidates.push_back("server");
     server_candidates.push_back("llama-server");
#endif
     server_candidates.push_back("delta-server");

     std::string server_bin;
     for (const auto& candidate : server_candidates) {
         if (tools::FileOps::file_exists(candidate)) {
             server_bin = candidate;
             break;
         }
     }

     if (server_bin.empty()) {
         UI::print_error("Server binary not found for model switch");
         return false;
     }
     
     // Find web UI path (reuse logic from launch_server_auto)
     std::string public_path;
     std::vector<std::string> public_candidates = {
         tools::FileOps::join_path(exe_dir, "../public"),
         tools::FileOps::join_path(exe_dir, "../../public"),
         "public",
         "./public",
         "/opt/homebrew/share/delta-cli/webui",
         "/usr/local/share/delta-cli/webui"
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
     
     // Build command
     std::string cmd_str = build_llama_server_cmd(server_bin, model_path, current_port_, ctx_size, model_alias, public_path);
     
     // Start delta-server
#ifdef _WIN32
     // Windows: Use CreateProcess
     std::cerr << "[DEBUG] Creating delta-server process with command: " << cmd_str << std::endl;
     STARTUPINFOA si = {0};
     PROCESS_INFORMATION pi = {0};
     si.cb = sizeof(si);
     si.dwFlags = STARTF_USESTDHANDLES;
     si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
     si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
     si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
     
     std::vector<char> cmd_line(cmd_str.begin(), cmd_str.end());
     cmd_line.push_back('\0');
     
     if (!CreateProcessA(NULL, cmd_line.data(), NULL, NULL, TRUE, 
                        CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
         std::cerr << "[DEBUG] CreateProcess failed (Error: " << GetLastError() << ")" << std::endl;
         UI::print_error("   ✗ Failed to create process");
         return false;
     }
     
     CloseHandle(pi.hThread);
     llama_server_pid_ = pi.dwProcessId;
     current_model_path_ = model_path;
     std::cerr << "[DEBUG] delta-server started with PID: " << llama_server_pid_ << std::endl;
     
     // Wait a moment for server to start
     std::this_thread::sleep_for(std::chrono::milliseconds(2000));
     
     // Check if process is still running
     DWORD exit_code;
     if (GetExitCodeProcess(pi.hProcess, &exit_code) && exit_code == STILL_ACTIVE) {
         std::cerr << "[DEBUG] delta-server is running" << std::endl;
         UI::print_info("   ✓ Model loaded successfully!");
         CloseHandle(pi.hProcess);
         return true;
     } else {
         std::cerr << "[DEBUG] delta-server failed to start (exited with code " << exit_code << ")" << std::endl;
         UI::print_error("   ✗ Failed to start delta-server");
         CloseHandle(pi.hProcess);
         llama_server_pid_ = 0;
         return false;
     }
#else
     // Unix: Use fork/exec
     std::cerr << "[DEBUG] Forking delta-server with command: " << cmd_str << std::endl;
     pid_t pid = fork();
     if (pid == 0) {
         // Child process: create new process group
         setsid();
         
         // Redirect output
         int devnull = open("/dev/null", O_WRONLY);
         if (devnull >= 0) {
             dup2(devnull, STDOUT_FILENO);
             dup2(devnull, STDERR_FILENO);
             close(devnull);
         }
         close(STDIN_FILENO);
         
         // Execute delta-server
         execl("/bin/sh", "sh", "-c", cmd_str.c_str(), (char*)NULL);
         _exit(1);
     } else if (pid > 0) {
         // Parent process: store PID
         llama_server_pid_ = -pid; // Store negative for process group
         current_model_path_ = model_path;
         std::cerr << "[DEBUG] delta-server started with PID: " << pid << " (stored as: " << llama_server_pid_ << ")" << std::endl;
         
         // Wait a moment for server to start
         std::this_thread::sleep_for(std::chrono::milliseconds(2000));
         
         // Check if process is still running
         int status;
         if (waitpid(pid, &status, WNOHANG) == 0) {
             std::cerr << "[DEBUG] delta-server is running (waitpid returned 0)" << std::endl;
             UI::print_info("   ✓ Model loaded successfully!");
             return true;
         } else {
             std::cerr << "[DEBUG] delta-server failed to start (waitpid returned non-zero)" << std::endl;
             UI::print_error("   ✗ Failed to start delta-server");
             llama_server_pid_ = 0;
             return false;
         }
     } else {
         std::cerr << "[DEBUG] fork() failed" << std::endl;
         UI::print_error("   ✗ Failed to fork process");
         return false;
     }
#endif
 }
 
 void Commands::init() {
     if (initialized_) return;
     
     // Register all commands
     command_map_["download"] = handle_download;
     command_map_["pull"] = handle_download;
     command_map_["remove"] = handle_remove;
     command_map_["delete"] = handle_remove;
     command_map_["list"] = handle_list;
     command_map_["list-models"] = handle_list;
     command_map_["models"] = handle_list;
     command_map_["list-local"] = handle_list;
     command_map_["use"] = handle_use;
     command_map_["available"] = handle_available;
     command_map_["list-available"] = handle_available;
     command_map_["clear-screen"] = handle_clear_screen;
     command_map_["help"] = handle_help;
     
     initialized_ = true;
 }
 
 bool Commands::process_command(const std::string& input, InteractiveSession& session) {
     // Parse command and arguments
     std::vector<std::string> args = parse_args(input);
     if (args.empty()) return false;
     
     std::string command = args[0];
     args.erase(args.begin()); // Remove command from args
     
     // Check if command exists
     auto it = command_map_.find(command);
     if (it == command_map_.end()) {
         UI::print_info("ℹ Type /help to see available commands");
         return true;
     }
     
     // Check if online command and we're offline
     if (is_online_command(command)) {
         // For now, we'll try the command and let it handle offline gracefully
         // In a real implementation, you'd check network connectivity here
     }
     
     // Execute command
     return it->second(args, session);
 }
 
 std::vector<std::string> Commands::parse_args(const std::string& input) {
     std::vector<std::string> args;
     std::istringstream iss(input);
     std::string arg;
     
     while (iss >> arg) {
         args.push_back(arg);
     }
     
     return args;
 }
 
 void Commands::show_help() {
     std::cout << "\n" << UI::BRIGHT_GREEN << UI::BOLD << "Interactive Commands:" << UI::RESET << std::endl;
     std::cout << "  " << UI::GREEN << "/download <model>" << UI::RESET << "     - Download a model" << std::endl;
     std::cout << "  " << UI::GREEN << "/remove <model>" << UI::RESET << "       - Remove a model (alias: /delete)" << std::endl;
     std::cout << "  " << UI::GREEN << "/list" << UI::RESET << "                - List local models" << std::endl;
     std::cout << "  " << UI::GREEN << "/available" << UI::RESET << "            - List available models" << std::endl;
     std::cout << "  " << UI::GREEN << "/use <model>" << UI::RESET << "          - Switch to another model" << std::endl;
     std::cout << "  " << UI::GREEN << "/clear-screen" << UI::RESET << "         - Clear the terminal screen" << std::endl;
     std::cout << "  " << UI::GREEN << "/help" << UI::RESET << "                 - Show this help" << std::endl;
     std::cout << std::endl;
     std::cout << "  " << UI::YELLOW << "exit, quit" << UI::RESET << "            - Exit interactive mode" << std::endl;
     std::cout << std::endl;
 }
 
 bool Commands::is_online_command(const std::string& command) {
     return (command == "download" || command == "pull");
 }
 
 void Commands::show_offline_message(const std::string& command) {
     UI::print_error("Command /" + command + " requires internet connection");
     UI::print_info("Please check your connection and try again");
 }
 
 // Command implementations
 bool Commands::handle_download(const std::vector<std::string>& args, InteractiveSession& session) {
     if (args.empty()) {
         UI::print_error("Please specify a model name");
         UI::print_info("Usage: /download <model-name>");
         UI::print_info("Example: /download qwen3:0.6b");
         return true;
     }
     
     std::string model_name = args[0];
     UI::print_info("Downloading model: " + model_name);
     
     // Set progress callback for download with visual progress bar
     session.model_mgr->set_progress_callback([](double progress, long long current, long long total) {
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
     });
     
     bool success = session.model_mgr->pull_model(model_name);
     session.model_mgr->set_progress_callback(nullptr);
     
     if (success) {
         std::cout << std::endl;
         UI::print_info("✓ Model downloaded successfully!");
         UI::print_info("You can now use: /use " + model_name);
     } else {
         std::cout << std::endl;
         UI::print_error("✗ Download failed");
         UI::print_info("Check your internet connection and try again");
     }
     
     return true;
 }
 
 bool Commands::handle_list(const std::vector<std::string>& args, InteractiveSession& session) {
     (void)args; // Suppress unused parameter warning
     // Get friendly model list (local models only)
     auto models = session.model_mgr->get_friendly_model_list(false);
     
     if (models.empty()) {
         UI::print_info("No models found locally.");
         UI::print_info("Download a model with: /download <model-name>");
         UI::print_info("See available models: /available");
         return true;
     }
     
     UI::print_border("Locally Cached Models");
     
     for (const auto& model : models) {
         std::string current_indicator = (model.name == session.current_model) ? " [CURRENT]" : "";
         std::cout << "  • " << model.name << current_indicator << std::endl;
         std::cout << "      " << model.display_name << " - " << model.description << std::endl;
         std::cout << "      Size: " << model.size_str << " | Quant: " << model.quantization << std::endl;
         std::cout << std::endl;
     }
     
     UI::print_info("Use '/use " + models[0].name + "' to switch to a model");
     return true;
 }
 
 
 bool Commands::handle_use(const std::vector<std::string>& args, InteractiveSession& session) {
     if (args.empty()) {
         UI::print_error("Please specify a model name");
         UI::print_info("Usage: /use <model-name>");
         UI::print_info("Example: /use qwen3:0.6b");
         return true;
     }
     
     std::string model_name = args[0];
     
     // Check if model is installed
     if (!session.model_mgr->is_model_installed(model_name)) {
         UI::print_error("Model not found: " + model_name);
         UI::print_info("Use /list to see available models");
         UI::print_info("Use /download to download a model");
         return true;
     }
     
     // Get model path
     std::string model_path = session.model_mgr->get_model_path(model_name);
     if (model_path.empty()) {
         UI::print_error("Could not find model path for: " + model_name);
         return true;
     }
     
     UI::print_info("Switching to model: " + model_name);
     UI::print_info("Loading model...");
     
     // Update config
     session.config->model_path = model_path;
     session.current_model = model_name;
     
     // Reload model
     if (!session.engine->load_model(*session.config)) {
         UI::print_error("Failed to load model: " + model_name);
         return true;
     }
     
     UI::print_info("✓ Model loaded successfully!");
     UI::print_info("Current model: " + session.current_model);
     
// Automatically launch web UI server with the new model
    int ctx_size = session.model_mgr->get_max_context_for_model(model_name);
    if (ctx_size <= 0 && session.config->n_ctx > 0) {
        ctx_size = session.config->n_ctx;
    }
     
     // Get short_name for model alias in web UI
     std::string model_alias;
     std::string registry_name_for_alias = model_name;
     if (session.model_mgr->is_in_registry(registry_name_for_alias)) {
         auto entry = session.model_mgr->get_registry_entry(registry_name_for_alias);
         if (!entry.short_name.empty()) {
             model_alias = entry.short_name;
         }
     } else {
         // Try converting dash to colon format
         size_t last_dash = registry_name_for_alias.find_last_of('-');
         if (last_dash != std::string::npos) {
             std::string colon_name = registry_name_for_alias.substr(0, last_dash) + ":" + 
                                      registry_name_for_alias.substr(last_dash + 1);
             if (session.model_mgr->is_in_registry(colon_name)) {
                 auto entry = session.model_mgr->get_registry_entry(colon_name);
                 if (!entry.short_name.empty()) {
                     model_alias = entry.short_name;
                 }
             }
         }
     }
     
     // If still not found, try to get short_name from filename
     if (model_alias.empty()) {
         // Extract filename from model_path
         std::string filename = model_path;
         size_t last_slash = filename.find_last_of("/\\");
         if (last_slash != std::string::npos) {
             filename = filename.substr(last_slash + 1);
         }
         model_alias = session.model_mgr->get_short_name_from_filename(filename);
     }
     
     // Restart delta-server with new model (if it's running)
     if (llama_server_pid_ != 0) {
         // Server is already running, restart it with new model
         if (restart_llama_server(model_path, model_name, ctx_size, model_alias)) {
             UI::print_success("Delta Server restarted with new model");
         } else {
             UI::print_error("Failed to restart server with new model");
         }
     } else {
         // Server not running, start it
         // launch_server_auto now waits for server to be ready before returning
         if (Commands::launch_server_auto(model_path, 8080, ctx_size, model_alias)) {
             // Server is confirmed listening, get the actual port used
             int actual_port = Commands::get_current_port();
             std::string url = "http://localhost:" + std::to_string(actual_port) + "/index.html";
             std::this_thread::sleep_for(std::chrono::milliseconds(500));
             if (tools::Browser::open_url(url)) {
                 UI::print_info("Browser opened automatically");
             }
         } else {
             UI::print_error("Failed to start server. Check error messages above.");
         }
     }
     
     return true;
 }
 
 
 bool Commands::handle_available(const std::vector<std::string>& args, InteractiveSession& session) {
     (void)args; // Suppress unused parameter warning
     // Get friendly model list (all available models)
     auto models = session.model_mgr->get_friendly_model_list(true);
     
     if (models.empty()) {
         UI::print_error("No models available in registry");
         return true;
     }
     
     UI::print_border("Available Models to Download");
     UI::print_info("Use '/download <model-name>' to download");
     std::cout << std::endl;
     
     for (const auto& model : models) {
         std::string status = model.installed ? "[✓ Installed]" : "[ Download  ]";
         std::string current_indicator = (model.name == session.current_model) ? " [CURRENT]" : "";
         
         std::cout << "  " << status << " " << model.name << current_indicator << std::endl;
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
     return true;
 }
 
 
 
 
 
 
 // Removed: handle_server - command removed for simplicity
 
 // Removed: handle_update, handle_no_color - commands removed for simplicity
 
 bool Commands::handle_help(const std::vector<std::string>& args, InteractiveSession& session) {
     (void)args; // Suppress unused parameter warning
     (void)session; // Suppress unused parameter warning
     show_help();
     return true;
 }
 
 bool Commands::handle_remove(const std::vector<std::string>& args, InteractiveSession& session) {
     if (args.empty()) {
         UI::print_error("Please specify a model name");
         UI::print_info("Usage: /remove <model-name>");
         UI::print_info("Example: /remove qwen2.5:0.6b");
         UI::print_info("Use /list to see installed models");
         return true;
     }
     
     std::string model_name = args[0];
     
     // Check if model is currently in use
     if (!session.current_model.empty() && session.current_model == model_name) {
         UI::print_error("Cannot delete model '" + model_name + "' - it is currently in use");
         UI::print_info("Switch to another model first with /use <model-name>");
         return true;
     }
     
     // Perform deletion with confirmation
     bool success = session.model_mgr->remove_model_with_confirmation(model_name);
     
     if (success) {
         UI::print_success("Model '" + model_name + "' removed successfully");
     }
     
     return true;
 }
 
 // ============================================================================
 // HISTORY AND SESSION MANAGEMENT COMMANDS
 // ============================================================================
 
 bool Commands::handle_clear_screen(const std::vector<std::string>& args, InteractiveSession& session) {
     (void)args;
     (void)session;
     
     UI::clear_screen();
     return true;
 }
 
 // Removed: handle_history, handle_delete_history, handle_new_session, handle_list_sessions,
 //          handle_switch_session, handle_delete_session, handle_active_session, handle_export_session
 //          - commands removed for simplicity
 
 } // namespace delta
 