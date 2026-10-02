#ifndef DELTA_TOOL_SHELL_H
#define DELTA_TOOL_SHELL_H

#include <string>

namespace delta {
namespace agent {

void register_shell_tools();

struct ShellOutput {
    int exit_code = -1;
    std::string output; // stdout and stderr, interleaved as the process wrote them
    bool timed_out = false;
    std::string error;
};

// The credential path fragment `command` mentions (see credential_path_needles()), or "".
std::string command_reaches_credentials(const std::string& command);

// Runs `command` through the system shell, capturing output and enforcing a wall-clock limit.
ShellOutput run_shell_command(const std::string& command, const std::string& working_dir, int timeout_seconds);

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_SHELL_H
