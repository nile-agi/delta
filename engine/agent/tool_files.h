#ifndef DELTA_TOOL_FILES_H
#define DELTA_TOOL_FILES_H

#include <string>

namespace delta {
namespace agent {

void register_file_tools();

// Resolves `path` and decides whether the agent may touch it.
// Returns an empty string when allowed, or a human-readable reason when not.
std::string path_denied_reason(const std::string& resolved_path);

// Expands ~ and makes the path absolute without requiring it to exist.
std::string resolve_agent_path(const std::string& raw);

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_FILES_H
