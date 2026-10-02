#ifndef DELTA_TOOL_TASK_H
#define DELTA_TOOL_TASK_H

#include <string>

namespace delta {
namespace agent {

void register_task_tools();

// The harness sets this for the duration of a run so the plan tools know which scratchpad to write.
void set_active_run_id(const std::string& run_id);
std::string active_run_id();

// The conversation a saved memory belongs to. Set alongside the run id; unlike the run id it can
// outlive the session, so a memory saved today is still recalled tomorrow.
void set_active_memory_scope(const std::string& scope);
std::string active_memory_scope();

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_TASK_H
