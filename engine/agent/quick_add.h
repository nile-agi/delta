#ifndef DELTA_QUICK_ADD_H
#define DELTA_QUICK_ADD_H

#include <ctime>
#include <optional>
#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

// A calendar item spotted in something the user said, for the UI to offer as a one-tap add. Used
// when the model cannot call tools itself, so nothing is ever created without the user's tap.
struct QuickAddCandidate {
    std::string title;
    std::string type = "event"; // event | task
    std::string start_time;     // YYYY-MM-DDTHH:MM:00, the format the calendar form writes
    bool all_day = false;       // a day was named but no time

    nlohmann::json to_json() const;
};

// Deterministic, no model involved: needs an activity cue ("attending", "meeting", "remind me")
// plus a day or a time, and skips questions. `now` fixes "today" so tests can pin it.
std::optional<QuickAddCandidate> parse_quick_add(const std::string& text, std::time_t now);

} // namespace agent
} // namespace delta

#endif // DELTA_QUICK_ADD_H
