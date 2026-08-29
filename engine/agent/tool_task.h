#ifndef DELTA_TOOL_TASK_H
#define DELTA_TOOL_TASK_H

#include <string>

namespace delta {
namespace agent {

void register_task_tools();

// The harness sets this for the duration of a run so the plan tools know which scratchpad to write.
void set_active_run_id(const std::string& run_id);
std::string active_run_id();

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_TASK_H
