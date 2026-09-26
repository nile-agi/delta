#ifndef DELTA_TOOL_FILES_H
#define DELTA_TOOL_FILES_H

#include <string>
#include <vector>

namespace delta {
namespace agent {

void register_file_tools();

// Path fragments that mark credential stores (~/.ssh, ~/.aws, ...). Shared with the shell tool so
// a command line cannot reach what the file tools refuse.
const std::vector<std::string>& credential_path_needles();

// Resolves `path` and decides whether the agent may touch it.
// Returns an empty string when allowed, or a human-readable reason when not.
std::string path_denied_reason(const std::string& resolved_path);

// Expands ~, makes the path absolute, and follows symlinks on whatever part of it exists, so the
// scope check judges where the path really leads. The path itself need not exist.
std::string resolve_agent_path(const std::string& raw);

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_FILES_H
