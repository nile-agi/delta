/**
 * Browser Utilities for Delta CLI
 * Portable way to open URLs in the default web browser
 */

#include "../delta_cli.h"
#include <cstdlib>
#include <string>
#include <fstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
    #include <windows.h>
    #include <shellapi.h>
#elif defined(__APPLE__)
    #include <unistd.h>
#else
    #include <unistd.h>
#endif

namespace delta {
namespace tools {

bool Browser::open_url(const std::string& url) {
#ifdef _WIN32
    // Windows: Use ShellExecute
    HINSTANCE result = ShellExecuteA(
        NULL,
        "open",
        url.c_str(),
        NULL,
        NULL,
        SW_SHOWNORMAL
    );
    return reinterpret_cast<intptr_t>(result) > 32;
    
#elif defined(__APPLE__)
    // macOS: Use 'open' command
    std::string command = "open \"" + url + "\"";
    int result = system(command.c_str());
    return (result == 0);
    
#else
    // Linux: Try xdg-open first, then fallback to other methods
    // xdg-open is the standard on most Linux distributions
    int result = system(("xdg-open \"" + url + "\" 2>/dev/null").c_str());
    if (result == 0) {
        return true;
    }
    
    // Fallback to x-www-browser (Debian/Ubuntu)
    result = system(("x-www-browser \"" + url + "\" 2>/dev/null").c_str());
    if (result == 0) {
        return true;
    }
    
    // Fallback to sensible-browser (some distributions)
    result = system(("sensible-browser \"" + url + "\" 2>/dev/null").c_str());
    if (result == 0) {
        return true;
    }
    
    // Last resort: try common browsers directly
    const char* browsers[] = {
        "firefox", "chromium", "chrome", "opera", "konqueror", "epiphany"
    };
    
    for (const char* browser : browsers) {
        result = system(("which " + std::string(browser) + " >/dev/null 2>&1").c_str());
        if (result == 0) {
            result = system((std::string(browser) + " \"" + url + "\" 2>/dev/null &").c_str());
            if (result == 0) {
                return true;
            }
        }
    }

    // WSL: no Linux GUI browser — open the URL in the Windows default browser
    {
        std::ifstream rel("/proc/sys/kernel/osrelease");
        std::string osrel;
        if (std::getline(rel, osrel)) {
            std::string low = osrel;
            std::transform(low.begin(), low.end(), low.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (low.find("microsoft") != std::string::npos ||
                low.find("wsl") != std::string::npos) {
                std::string esc;
                for (char c : url) {
                    if (c == '"')
                        esc += "\\\"";
                    else
                        esc += c;
                }
                const char* cmd_paths[] = {
                    "cmd.exe",
                    "/mnt/c/Windows/System32/cmd.exe",
                    "/mnt/c/WINDOWS/System32/cmd.exe"
                };
                for (const char* cmd_base : cmd_paths) {
                    std::string wcmd = std::string(cmd_base) +
                        " /c start \"\" \"" + esc + "\"";
                    if (system(wcmd.c_str()) == 0) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
#endif
}

} // namespace tools
} // namespace delta

