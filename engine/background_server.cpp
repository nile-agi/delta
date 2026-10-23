#include "background_server.h"
#include "delta_cli.h"
#include "vendor/llama.cpp/vendor/cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace delta::background {
namespace {

struct State {
    long pid = 0;
    int port = 8080;
};

std::string delta_dir() {
    return tools::FileOps::join_path(tools::FileOps::get_home_dir(), ".delta-cli");
}
std::string state_path() {
    return tools::FileOps::join_path(delta_dir(), "server.json");
}
std::string log_path() {
    return tools::FileOps::join_path(tools::FileOps::join_path(delta_dir(), "logs"), "server.log");
}

bool read_state(State& state) {
    std::ifstream in(state_path());
    if (!in)
        return false;
    try {
        auto j = nlohmann::json::parse(in);
        state.pid = j.value("pid", 0L);
        state.port = j.value("port", 8080);
        return state.pid > 0;
    } catch (...) {
        return false;
    }
}

void write_state(const State& state) {
    std::ofstream(state_path()) << nlohmann::json{{"pid", state.pid}, {"port", state.port}, {"log", log_path()}}.dump(
        2);
}

bool alive(long pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h)
        return false;
    DWORD code = 0;
    const bool running = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return running;
#else
    int status = 0;
    if (waitpid(static_cast<pid_t>(pid), &status, WNOHANG) == pid)
        return false; // our own child that has exited
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

// The API is the web UI port + 1; its /v1/models answers once the server is up.
bool answering(int port, nlohmann::json* models = nullptr) {
    httplib::Client cli("127.0.0.1", port + 1);
    cli.set_connection_timeout(1);
    cli.set_read_timeout(3);
    auto res = cli.Get("/v1/models");
    if (!res || res->status != 200)
        return false;
    if (models) {
        try {
            *models = nlohmann::json::parse(res->body).value("data", nlohmann::json::array());
        } catch (...) {
        }
    }
    return true;
}

void print_tail(size_t n) {
    std::ifstream in(log_path());
    std::deque<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        lines.push_back(line);
        if (lines.size() > n)
            lines.pop_front();
    }
    for (const auto& line : lines)
        std::cout << line << "\n";
    std::cout << std::flush;
}

void print_urls(int port) {
    std::cout << "  Web UI:  http://localhost:" << port << "\n"
              << "  API:     http://127.0.0.1:" << port + 1 << "/v1\n"
              << "  Status:  delta server status   Logs: delta server logs   Stop: delta server stop" << std::endl;
}

int port_from(const std::vector<std::string>& args) {
    for (size_t i = 0; i + 1 < args.size(); i++)
        if (args[i] == "--port")
            try {
                return std::stoi(args[i + 1]);
            } catch (...) {
            }
    return 8080;
}

} // namespace

int start(const std::vector<std::string>& server_args) {
    State state;
    if (read_state(state) && alive(state.pid)) {
        UI::print_info("Delta is already running in the background (pid " + std::to_string(state.pid) + ").");
        print_urls(state.port);
        return 0;
    }
    tools::FileOps::create_dir(delta_dir());
    tools::FileOps::create_dir(tools::FileOps::join_path(delta_dir(), "logs"));
    state.port = port_from(server_args);
    const std::string self = tools::FileOps::join_path(tools::FileOps::get_executable_dir(), "delta");

#ifdef _WIN32
    std::string cmd = "\"" + self + ".exe\" --server";
    for (const auto& a : server_args)
        cmd += " \"" + a + "\"";
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE log = CreateFileA(log_path().c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = log;
    si.hStdError = log;
    si.hStdInput = nullptr;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(log);
        UI::print_error("Could not start Delta in the background.");
        return 1;
    }
    CloseHandle(log);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    state.pid = static_cast<long>(pi.dwProcessId);
#else
    const pid_t pid = fork();
    if (pid < 0) {
        UI::print_error("Could not start Delta in the background.");
        return 1;
    }
    if (pid == 0) {
        setsid(); // its own session: no terminal, so closing this one or Ctrl+Z does not touch it
        const int log = open(log_path().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        const int null = open("/dev/null", O_RDONLY);
        if (log >= 0) {
            dup2(log, STDOUT_FILENO);
            dup2(log, STDERR_FILENO);
        }
        if (null >= 0)
            dup2(null, STDIN_FILENO);
        std::vector<std::string> args{self, "--server"};
        args.insert(args.end(), server_args.begin(), server_args.end());
        std::vector<char*> argv;
        for (auto& a : args)
            argv.push_back(a.data());
        argv.push_back(nullptr);
        execv(self.c_str(), argv.data());
        _exit(127);
    }
    state.pid = pid;
#endif
    write_state(state);

    UI::print_info("Starting Delta in the background...");
    for (int i = 0; i < 180; i++) {
        if (!alive(state.pid)) {
            UI::print_error("Delta stopped while starting. Last lines of " + log_path() + ":");
            print_tail(15);
            std::remove(state_path().c_str());
            return 1;
        }
        if (answering(state.port)) {
            UI::print_success("Delta is running in the background (pid " + std::to_string(state.pid) + ")");
            print_urls(state.port);
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    UI::print_info("Delta is still starting (pid " + std::to_string(state.pid) + "); check: delta server status");
    print_urls(state.port);
    return 0;
}

int stop() {
    State state;
    if (!read_state(state) || !alive(state.pid)) {
        std::remove(state_path().c_str());
        UI::print_info("Delta is not running in the background.");
        return 0;
    }
#ifdef _WIN32
    std::system(("taskkill /PID " + std::to_string(state.pid) + " /T /F >NUL 2>&1").c_str());
#else
    // The whole session: delta, delta-server and llama-server.
    kill(-static_cast<pid_t>(state.pid), SIGTERM);
    for (int i = 0; i < 40 && alive(state.pid); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (alive(state.pid))
        kill(-static_cast<pid_t>(state.pid), SIGKILL);
#endif
    std::remove(state_path().c_str());
    UI::print_success("Stopped the background Delta server.");
    return 0;
}

int status() {
    State state;
    if (!read_state(state) || !alive(state.pid)) {
        std::remove(state_path().c_str());
        UI::print_info("Delta is not running in the background. Start it with: delta server start");
        return 1;
    }
    nlohmann::json models;
    const bool up = answering(state.port, &models);
    std::cout << (up ? "● running" : "● starting") << "  pid " << state.pid << "\n";
    print_urls(state.port);
    if (up) {
        std::string loaded, available;
        for (const auto& m : models) {
            const std::string id = m.value("id", "");
            const bool is_loaded = m.contains("status") && m["status"].value("value", "") == "loaded";
            (is_loaded ? loaded : available) += (id.empty() ? "" : "\n    " + id);
        }
        std::cout << "  Models loaded:" << (loaded.empty() ? " none (they load on first request)" : loaded) << "\n";
        if (!available.empty())
            std::cout << "  Available:" << available << "\n";
    }
    std::cout << "  Log:     " << log_path() << std::endl;
    return 0;
}

int logs() {
    if (!tools::FileOps::file_exists(log_path())) {
        UI::print_info("No background server log yet. Start one with: delta server start");
        return 1;
    }
    print_tail(60);
    std::cout << "(" << log_path() << ")" << std::endl;
    return 0;
}

int running_port() {
    State state;
    return read_state(state) && alive(state.pid) && answering(state.port) ? state.port : 0;
}

} // namespace delta::background
