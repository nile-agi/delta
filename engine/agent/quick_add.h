#ifndef DELTA_QUICK_ADD_H
#define DELTA_QUICK_ADD_H

#include <ctime>
#include <optional>
#include <string>
#include <vector>
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

// An existing calendar item a "move it to Friday" request may mean, with where it would go.
struct QuickMoveOption {
    std::string id;
    std::string title;
    std::string type;
    std::string start_time;
    std::string new_start_time;
    std::string new_end_time; // empty when the item has no end time
    bool all_day = false;

    nlohmann::json to_json() const;
};

// For "move / reschedule / push ... to <day or time>": the items in `items` (as list_events returns
// them) the user most likely means, best first, at most three. Empty when the text is not a move
// or no item fits. "same time" keeps each item's own time.
std::vector<QuickMoveOption> parse_quick_move(const std::string& text, std::time_t now,
                                              const std::vector<nlohmann::json>& items);

// What to offer for `text`: {"kind":"move","options":[...]}, {"kind":"add",...}, or null.
nlohmann::json suggest_quick_action(const std::string& text, std::time_t now, const std::vector<nlohmann::json>& items);

// Deterministic, no model involved: needs an activity cue ("attending", "meeting", "remind me")
// plus a day or a time, and skips questions. `now` fixes "today" so tests can pin it.
std::optional<QuickAddCandidate> parse_quick_add(const std::string& text, std::time_t now);

} // namespace agent
} // namespace delta

#endif // DELTA_QUICK_ADD_H
