#ifndef DELTA_TOOL_CALENDAR_H
#define DELTA_TOOL_CALENDAR_H

#include <string>

namespace delta {
namespace agent {

void register_calendar_tools();

// Resolve a model-provided datetime to a valid YYYY-MM-DDTHH:MM string.
// Accepts: ISO format, natural language ("friday 2pm", "tomorrow 13:00"),
// day names, am/pm, military time (1300), bare hours.
std::string resolve_datetime(const std::string& raw);

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_CALENDAR_H
