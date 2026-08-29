#include "tool_files.h"
#include "context_manager.h"
#include "tool_registry.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

namespace delta {
namespace agent {

namespace {

constexpr size_t kMaxReadBytes = 200000;   // a single read_file result before truncation
constexpr size_t kMaxWriteBytes = 2000000; // refuse to write more than this in one call
constexpr int kMaxListEntries = 300;

std::string home_dir() {
#if defined(_WIN32)
    const char* profile = std::getenv("USERPROFILE");
    if (profile)
        return profile;
    const char* drive = std::getenv("HOMEDRIVE");
    const char* path = std::getenv("HOMEPATH");
    if (drive && path)
        return std::string(drive) + path;
    return "";
#else
    const char* home = std::getenv("HOME");
    return home ? home : "";
#endif
}

std::string normalize_separators(std::string p) {
#if defined(_WIN32)
    std::replace(p.begin(), p.end(), '\\', '/');
#endif
    return p;
}

// Collapses "." and ".." lexically so a path cannot escape an allowed root by traversal.
std::string lexically_normal(const std::string& input) {
    const bool absolute = !input.empty() && input[0] == '/';
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string part;
    while (std::getline(ss, part, '/')) {
        if (part.empty() || part == ".")
            continue;
        if (part == "..") {
            if (!parts.empty() && parts.back() != "..")
                parts.pop_back();
            else if (!absolute)
                parts.push_back("..");
            continue;
        }
        parts.push_back(part);
    }
    std::string out;
    for (const auto& p : parts)
        out += "/" + p;
    if (out.empty())
        out = absolute ? "/" : ".";
    return out;
}

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool within(const std::string& path, const std::string& root) {
    if (root.empty())
        return false;
    if (path == root)
        return true;
    return starts_with(path, root.back() == '/' ? root : root + "/");
}

bool is_directory(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

bool path_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}

ToolResult ok_json(const nlohmann::json& payload) {
    return {true, payload.dump(), ""};
}

} // namespace

std::string resolve_agent_path(const std::string& raw) {
    std::string p = normalize_separators(raw);
    if (p.empty())
        return "";
    if (p == "~")
        p = home_dir();
    else if (p.rfind("~/", 0) == 0)
        p = home_dir() + p.substr(1);

    if (p.empty())
        return "";
#if defined(_WIN32)
    const bool absolute = p.size() > 1 && p[1] == ':';
#else
    const bool absolute = p[0] == '/';
#endif
    if (!absolute) {
        char buf[4096];
#if defined(_WIN32)
        if (_getcwd(buf, sizeof(buf)))
#else
        if (getcwd(buf, sizeof(buf)))
#endif
            p = std::string(buf) + "/" + p;
    }
    return lexically_normal(normalize_separators(p));
}

std::string path_denied_reason(const std::string& resolved_path) {
    if (resolved_path.empty())
        return "Empty path";

    const std::string home = normalize_separators(home_dir());

    // The agent works inside the user's own space plus temp. Everything else -- system
    // directories, other users' homes -- is out of reach.
    const bool in_scope = (!home.empty() && within(resolved_path, lexically_normal(home))) ||
                          within(resolved_path, "/tmp") || within(resolved_path, "/private/tmp") ||
                          within(resolved_path, "/var/tmp");
    if (!in_scope)
        return "Path is outside the home directory and temp folders, which is where Delta is allowed to work";

    // Secrets stay off limits even inside the home directory.
    static const char* forbidden[] = {"/.ssh",
                                      "/.aws",
                                      "/.gnupg",
                                      "/.gpg",
                                      "/.netrc",
                                      "/.docker/config.json",
                                      "/.kube",
                                      "/.config/gcloud",
                                      "/Library/Keychains",
                                      "/.password-store",
                                      "/.delta-cli/agent.db",
                                      nullptr};
    for (int i = 0; forbidden[i]; i++) {
        const std::string needle = forbidden[i];
        if (resolved_path.find(needle) != std::string::npos)
            return std::string("Path looks like it holds credentials (") + needle + "), which Delta will not touch";
    }
    return "";
}

void register_file_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"read_file",
         "Read a text file from disk. Use it to look at the user's documents, notes, code, or "
         "configuration before answering questions about them.",
         {{"type", "object"},
          {"properties",
           {{"path", {{"type", "string"}, {"description", "Absolute path, or one starting with ~"}}},
            {"max_bytes", {{"type", "integer"}, {"description", "Stop after this many bytes (default 200000)"}}}}},
          {"required", {"path"}}},
         ToolRisk::Caution,
         "files"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string path = resolve_agent_path(args.value("path", ""));
            const std::string denied = path_denied_reason(path);
            if (!denied.empty())
                return {false, "", denied};
            if (!path_exists(path))
                return {false, "", "No such file: " + path};
            if (is_directory(path))
                return {false, "", path + " is a directory. Use list_directory instead."};

            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {false, "", "Could not open " + path};
            const size_t cap =
                std::min<size_t>(kMaxReadBytes, std::max(1, args.value("max_bytes", (int)kMaxReadBytes)));
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const size_t original = content.size();
            bool truncated = false;
            if (content.size() > cap) {
                content = ContextManager::truncate_middle(content, cap);
                truncated = true;
            }
            return ok_json({{"path", path}, {"bytes", original}, {"truncated", truncated}, {"content", content}});
        });

    registry.register_tool(
        {"write_file",
         "Write text to a file, creating it if needed. Overwrites whatever is there, so read the "
         "file first when you mean to change only part of it.",
         {{"type", "object"},
          {"properties",
           {{"path", {{"type", "string"}, {"description", "Absolute path, or one starting with ~"}}},
            {"content", {{"type", "string"}, {"description", "Full new contents of the file"}}},
            {"append", {{"type", "boolean"}, {"description", "Append instead of overwriting (default false)"}}}}},
          {"required", {"path", "content"}}},
         ToolRisk::Caution,
         "files"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string path = resolve_agent_path(args.value("path", ""));
            const std::string denied = path_denied_reason(path);
            if (!denied.empty())
                return {false, "", denied};
            const std::string content = args.value("content", "");
            if (content.size() > kMaxWriteBytes)
                return {false, "", "Refusing to write more than 2 MB in one call"};
            if (is_directory(path))
                return {false, "", path + " is a directory"};

            const bool append = args.value("append", false);
            std::ofstream out(path, append ? (std::ios::binary | std::ios::app) : std::ios::binary);
            if (!out)
                return {false, "", "Could not write to " + path + " (does the parent directory exist?)"};
            out << content;
            out.close();
            return ok_json({{"path", path}, {"bytes_written", content.size()}, {"appended", append}});
        });

    registry.register_tool(
        {"list_directory",
         "List the files and folders in a directory. Use it to find your way around before reading files.",
         {{"type", "object"},
          {"properties", {{"path", {{"type", "string"}, {"description", "Absolute path, or one starting with ~"}}}}},
          {"required", {"path"}}},
         ToolRisk::Caution,
         "files"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string path = resolve_agent_path(args.value("path", ""));
            const std::string denied = path_denied_reason(path);
            if (!denied.empty())
                return {false, "", denied};
            if (!path_exists(path))
                return {false, "", "No such directory: " + path};
            if (!is_directory(path))
                return {false, "", path + " is a file, not a directory"};

            nlohmann::json entries = nlohmann::json::array();
            bool truncated = false;
#if defined(_WIN32)
            WIN32_FIND_DATAA fd;
            HANDLE h = FindFirstFileA((path + "\\*").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    const std::string name = fd.cFileName;
                    if (name == "." || name == "..")
                        continue;
                    if (entries.size() >= kMaxListEntries) {
                        truncated = true;
                        break;
                    }
                    entries.push_back(
                        {{"name", name}, {"type", (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "dir" : "file"}});
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
#else
            DIR* dir = opendir(path.c_str());
            if (!dir)
                return {false, "", "Could not open " + path};
            while (struct dirent* entry = readdir(dir)) {
                const std::string name = entry->d_name;
                if (name == "." || name == "..")
                    continue;
                if (entries.size() >= kMaxListEntries) {
                    truncated = true;
                    break;
                }
                entries.push_back({{"name", name}, {"type", is_directory(path + "/" + name) ? "dir" : "file"}});
            }
            closedir(dir);
#endif
            return ok_json({{"path", path}, {"count", entries.size()}, {"truncated", truncated}, {"entries", entries}});
        });

    registry.register_tool(
        {"delete_file",
         "Permanently delete a file. There is no undo, so be sure the user asked for this.",
         {{"type", "object"},
          {"properties", {{"path", {{"type", "string"}, {"description", "Absolute path of the file to delete"}}}}},
          {"required", {"path"}}},
         ToolRisk::Destructive,
         "files"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string path = resolve_agent_path(args.value("path", ""));
            const std::string denied = path_denied_reason(path);
            if (!denied.empty())
                return {false, "", denied};
            if (!path_exists(path))
                return {false, "", "No such file: " + path};
            if (is_directory(path))
                return {false, "", "Refusing to delete a directory"};
            if (std::remove(path.c_str()) != 0)
                return {false, "", "Could not delete " + path};
            return ok_json({{"deleted", true}, {"path", path}});
        });
}

} // namespace agent
} // namespace delta
