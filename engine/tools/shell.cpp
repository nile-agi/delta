/**
 * Shell Integration Tool for Delta CLI
 */

#include "../delta_cli.h"
#include <cstdlib>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <wordexp.h>
#endif

extern char** environ;

namespace delta {
namespace tools {

std::string Shell::get_shell() {
#ifdef _WIN32
    const char* comspec = getenv("COMSPEC");
    return comspec ? comspec : "cmd.exe";
#else
    const char* shell = getenv("SHELL");
    return shell ? shell : "/bin/sh";
#endif
}

std::string Shell::expand_path(const std::string& path) {
#ifdef _WIN32
    char expanded[MAX_PATH];
    ExpandEnvironmentStringsA(path.c_str(), expanded, MAX_PATH);
    return std::string(expanded);
#else
    if (path.empty() || path[0] != '~') {
        return path;
    }
    
    wordexp_t exp;
    if (wordexp(path.c_str(), &exp, 0) == 0) {
        std::string result = exp.we_wordv[0];
        wordfree(&exp);
        return result;
    }
    
    // Fallback: simple tilde expansion
    const char* home = getenv("HOME");
    if (home && path.length() > 1 && path[1] == '/') {
        return std::string(home) + path.substr(1);
    }
    
    return path;
#endif
}

std::map<std::string, std::string> Shell::get_env() {
    std::map<std::string, std::string> env_map;
    
#ifdef _WIN32
    char* env = GetEnvironmentStringsA();
    if (env) {
        char* ptr = env;
        while (*ptr) {
            std::string entry(ptr);
            size_t pos = entry.find('=');
            if (pos != std::string::npos) {
                env_map[entry.substr(0, pos)] = entry.substr(pos + 1);
            }
            ptr += entry.length() + 1;
        }
        FreeEnvironmentStringsA(env);
    }
#else
    for (char** env = environ; *env != nullptr; env++) {
        std::string entry(*env);
        size_t pos = entry.find('=');
        if (pos != std::string::npos) {
            env_map[entry.substr(0, pos)] = entry.substr(pos + 1);
        }
    }
#endif
    
    return env_map;
}

} // namespace tools
} // namespace delta

