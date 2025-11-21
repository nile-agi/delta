/**
 * Dependency Protocol Tool - Safe command execution for Delta CLI
 */

#include "../delta_cli.h"
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
<<<<<<< HEAD
    #include <climits>
=======
    #include <limits.h>
    #ifndef PATH_MAX
        #define PATH_MAX 4096
    #endif
>>>>>>> 4d40c1f867b9c2fd7507905a8f09f0891de94e7f
#endif

namespace delta {
namespace tools {

DepProtocol::Result DepProtocol::execute(const std::string& command,
                                        const std::vector<std::string>& args,
                                        const std::string& working_dir) {
    Result result;
    result.exit_code = -1;
    result.success = false;
    
    // Build full command
    std::string full_command = command;
    for (const auto& arg : args) {
        full_command += " \"" + arg + "\"";
    }
    
    // Change directory if specified
    std::string original_dir;
    if (!working_dir.empty()) {
#ifdef _WIN32
        char buffer[MAX_PATH];
        GetCurrentDirectoryA(MAX_PATH, buffer);
        original_dir = buffer;
        SetCurrentDirectoryA(working_dir.c_str());
#else
        char buffer[PATH_MAX];
        if (getcwd(buffer, sizeof(buffer))) {
            original_dir = buffer;
        }
        if (chdir(working_dir.c_str()) != 0) {
            result.error = "Failed to change directory to " + working_dir;
            return result;
        }
#endif
    }
    
    // Execute command and capture output
#ifdef _WIN32
    FILE* pipe = _popen((full_command + " 2>&1").c_str(), "r");
#else
    FILE* pipe = popen((full_command + " 2>&1").c_str(), "r");
#endif
    
    if (!pipe) {
        result.error = "Failed to execute command";
        return result;
    }
    
    // Read output
    std::array<char, 128> buffer;
    std::stringstream ss;
    
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        ss << buffer.data();
    }
    
    result.output = ss.str();
    
    // Get exit code
#ifdef _WIN32
    result.exit_code = _pclose(pipe);
#else
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }
#endif
    
    result.success = (result.exit_code == 0);
    
    // Restore directory
    if (!original_dir.empty()) {
#ifdef _WIN32
        SetCurrentDirectoryA(original_dir.c_str());
#else
        if (chdir(original_dir.c_str()) != 0) {
            // Log error but don't fail - we're in cleanup
            result.error += "\nWarning: Failed to restore original directory";
        }
#endif
    }
    
    return result;
}

} // namespace tools
} // namespace delta

