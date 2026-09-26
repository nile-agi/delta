#ifndef DELTA_TOOL_CALENDAR_H
#define DELTA_TOOL_CALENDAR_H

#include <optional>
#include <string>

namespace delta {
namespace agent {

void register_calendar_tools();

// Resolve a model-provided datetime to a valid YYYY-MM-DDTHH:MM string.
// Accepts: ISO format, natural language ("friday 2pm", "tomorrow 13:00"),
// day names, am/pm, military time (1300), bare hours.
std::string resolve_datetime(const std::string& raw);

// The clock time named anywhere in `text` as HH:MM ("noon", "3pm", "15:30", "1300", "at 13"), or
// nothing when it names none.
std::optional<std::string> extract_clock_time(const std::string& text);

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_CALENDAR_H
