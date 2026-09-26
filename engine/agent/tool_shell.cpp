#include "tool_shell.h"
#include "context_manager.h"
#include "tool_files.h"
#include "tool_registry.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <csignal>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace delta {
namespace agent {

namespace {
constexpr size_t kMaxOutputChars = 30000;
constexpr int kDefaultTimeout = 60;
constexpr int kMaxTimeout = 600;
} // namespace

#if defined(_WIN32)

// Windows has no straightforward way to time out _popen, so the timeout is advisory here: the
// command runs to completion and the limit is only reported.
ShellOutput run_shell_command(const std::string& command, const std::string& working_dir, int timeout_seconds) {
    (void)timeout_seconds;
    ShellOutput result;
    std::string full = command + " 2>&1";
    if (!working_dir.empty())
        full = "cd /d \"" + working_dir + "\" && " + full;
    FILE* pipe = _popen(full.c_str(), "r");
    if (!pipe) {
        result.error = "Could not start the shell";
        return result;
    }
    std::array<char, 4096> buf{};
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) {
        if (result.output.size() < kMaxOutputChars * 4)
            result.output += buf.data();
    }
    result.exit_code = _pclose(pipe);
    return result;
}

#else

ShellOutput run_shell_command(const std::string& command, const std::string& working_dir, int timeout_seconds) {
    ShellOutput result;

    int fds[2];
    if (pipe(fds) != 0) {
        result.error = "Could not create a pipe";
        return result;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        result.error = "Could not fork a process";
        return result;
    }

    if (pid == 0) {
        // Child: own process group so a timeout can kill the whole tree, not just the shell.
        setpgid(0, 0);
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        if (!working_dir.empty() && chdir(working_dir.c_str()) != 0)
            _exit(127);
        if (working_dir.empty()) {
            // No directory given: run from home, as the tool's description promises, rather
            // than from wherever the server happened to be started.
            const char* home = getenv("HOME");
            if (home && *home)
                (void)chdir(home);
        }
        const char* shell = getenv("SHELL");
        if (!shell || !*shell)
            shell = "/bin/sh";
        execl(shell, shell, "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }

    // Parent: read until EOF or the deadline passes.
    close(fds[1]);
    const time_t deadline = time(nullptr) + timeout_seconds;
    bool killed = false;
    for (;;) {
        const time_t now = time(nullptr);
        if (now >= deadline) {
            killed = true;
            break;
        }
        struct timeval tv;
        tv.tv_sec = std::max<time_t>(1, deadline - now);
        tv.tv_usec = 0;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fds[0], &set);
        const int ready = select(fds[0] + 1, &set, nullptr, nullptr, &tv);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready == 0) {
            killed = true;
            break;
        }
        char buf[4096];
        const ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n <= 0)
            break;
        // Keep reading past the cap so the process is never blocked on a full pipe; the surplus
        // is dropped rather than buffered without limit.
        if (result.output.size() < kMaxOutputChars * 4)
            result.output.append(buf, static_cast<size_t>(n));
    }
    close(fds[0]);

    // Output has ended, but the child may still be running (it closed stdout, or left a
    // background job holding the pipe). Give it until the deadline, then kill the whole group.
    int status = 0;
    if (!killed) {
        for (;;) {
            const pid_t done = waitpid(pid, &status, WNOHANG);
            if (done == pid || (done < 0 && errno != EINTR))
                break;
            if (time(nullptr) >= deadline) {
                killed = true;
                break;
            }
            usleep(20000);
        }
    }
    if (killed) {
        result.timed_out = true;
        kill(-pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
    // Anything the shell left behind in its process group ends with it.
    kill(-pid, SIGTERM);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

#endif

// Returns the credential fragment a command line mentions, or "" when it mentions none. A
// substring check, on purpose: it has to catch "cat ~/.ssh/id_rsa", "cd .aws && cat credentials"
// and quoted forms alike, and a false positive only costs the model a rephrase.
std::string command_reaches_credentials(const std::string& command) {
    for (const auto& needle : credential_path_needles()) {
        if (command.find(needle) != std::string::npos)
            return needle;
        // The same name without its leading slash, when it starts a path token.
        const std::string bare = needle.substr(1);
        size_t pos = command.find(bare);
        while (pos != std::string::npos) {
            const char before = pos == 0 ? ' ' : command[pos - 1];
            if (before == ' ' || before == '\t' || before == '\'' || before == '"' || before == '=' || before == '(' ||
                before == '~')
                return needle;
            pos = command.find(bare, pos + 1);
        }
    }
    return "";
}

void register_shell_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"run_command",
         "Run a shell command on the user's machine and get back its output and exit code. "
         "Use it for things no other tool covers: inspecting the system, searching files, running "
         "a build or a script. Prefer read_file and list_directory for plain file work. "
         "Explain what you are about to run before you run it.",
         {{"type", "object"},
          {"properties",
           {{"command", {{"type", "string"}, {"description", "The command line to run"}}},
            {"working_dir",
             {{"type", "string"}, {"description", "Directory to run it in (defaults to the home directory)"}}},
            {"timeout_seconds",
             {{"type", "integer"}, {"description", "Give up after this long (default 60, max 600)"}}}}},
          {"required", {"command"}}},
         ToolRisk::Destructive,
         "shell"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string command = args.value("command", "");
            if (command.empty())
                return {false, "", "command is required"};
            const std::string secret = command_reaches_credentials(command);
            if (!secret.empty())
                return {false, "",
                        "That command reaches into " + secret +
                            ", which looks like it holds credentials. "
                            "Delta will not touch it."};

            std::string dir;
            if (args.contains("working_dir") && args["working_dir"].is_string() &&
                !args["working_dir"].get<std::string>().empty()) {
                dir = resolve_agent_path(args["working_dir"].get<std::string>());
                const std::string denied = path_denied_reason(dir);
                if (!denied.empty())
                    return {false, "", denied};
            }

            int timeout = args.value("timeout_seconds", kDefaultTimeout);
            timeout = std::max(1, std::min(kMaxTimeout, timeout));

            const ShellOutput out = run_shell_command(command, dir, timeout);
            if (!out.error.empty())
                return {false, "", out.error};

            std::string text = out.output;
            const bool truncated = text.size() > kMaxOutputChars;
            if (truncated)
                text = ContextManager::truncate_middle(text, kMaxOutputChars);

            nlohmann::json payload = {
                {"exit_code", out.exit_code}, {"timed_out", out.timed_out}, {"truncated", truncated}, {"output", text}};
            if (out.timed_out)
                payload["note"] = "The command was killed after " + std::to_string(timeout) + " seconds.";
            // A non-zero exit is a real result the model should reason about, not a tool failure.
            return {true, payload.dump(), ""};
        });
}

} // namespace agent
} // namespace delta
