/**
 * File Operations Tool for Delta CLI
 */

#include "../delta_cli.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <cstdlib>

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <pwd.h>
    #include <libgen.h>
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <pwd.h>
    #include <libgen.h>
#endif

namespace delta {
namespace tools {

std::string FileOps::read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileOps::write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file << content;
    return file.good();
}

bool FileOps::file_exists(const std::string& path) {
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0) && S_ISREG(buffer.st_mode);
#endif
}

bool FileOps::dir_exists(const std::string& path) {
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
            (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0) && S_ISDIR(buffer.st_mode);
#endif
}

bool FileOps::create_dir(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0 || dir_exists(path);
}

std::vector<std::string> FileOps::list_dir(const std::string& path) {
    std::vector<std::string> files;
    
#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    std::string search_path = path + "\\*";
    HANDLE handle = FindFirstFileA(search_path.c_str(), &find_data);
    
    if (handle == INVALID_HANDLE_VALUE) {
        return files;
    }
    
    do {
        std::string name = find_data.cFileName;
        if (name != "." && name != "..") {
            files.push_back(name);
        }
    } while (FindNextFileA(handle, &find_data));
    
    FindClose(handle);
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return files;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name != "." && name != "..") {
            files.push_back(name);
        }
    }
    
    closedir(dir);
#endif
    
    return files;
}

std::string FileOps::get_home_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path) == S_OK) {
        return std::string(path);
    }
    
    const char* home = getenv("USERPROFILE");
    if (home) return std::string(home);
    
    const char* homedrive = getenv("HOMEDRIVE");
    const char* homepath = getenv("HOMEPATH");
    if (homedrive && homepath) {
        return std::string(homedrive) + std::string(homepath);
    }
    
    return "C:\\";
#else
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home);
    }
    
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::string(pw->pw_dir);
    }
    
    return "/tmp";
#endif
}

std::string FileOps::join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    
#ifdef _WIN32
    char sep = '\\';
    if (a.back() == '\\' || a.back() == '/') {
        return a + b;
    }
#else
    char sep = '/';
    if (a.back() == '/') {
        return a + b;
    }
#endif
    
    return a + sep + b;
}

std::string FileOps::get_executable_dir() {
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (length == 0) return "";
    
    // Find the last backslash
    char* last_slash = strrchr(path, '\\');
    if (last_slash) {
        *last_slash = '\0';
        return std::string(path);
    }
    return "";
#elif defined(__APPLE__)
    // macOS/iOS: Use _NSGetExecutablePath
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        char* dir = dirname(path);
        return std::string(dir);
    }
    return "";
#elif defined(__linux__) || defined(__ANDROID__)
    // Linux/Android: Try /proc/self/exe first
    char path[1024];
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length != -1) {
        path[length] = '\0';
        char* dir = dirname(path);
        return std::string(dir);
    }
    
    // Fallback: try /proc/self/cmdline
    std::ifstream cmdline("/proc/self/cmdline");
    if (cmdline.is_open()) {
        std::string line;
        if (std::getline(cmdline, line)) {
            // cmdline is null-separated, get first argument
            size_t null_pos = line.find('\0');
            if (null_pos != std::string::npos) {
                line = line.substr(0, null_pos);
            }
            if (!line.empty() && line[0] == '/') {
                char* dir = dirname(const_cast<char*>(line.c_str()));
                return std::string(dir);
            }
        }
    }
    return "";
#else
    // Generic Unix fallback
    char path[1024];
    ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length != -1) {
        path[length] = '\0';
        char* dir = dirname(path);
        return std::string(dir);
    }
    return "";
#endif
}

} // namespace tools
} // namespace delta

