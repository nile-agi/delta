#include "tool_calendar.h"
#include "agent_database.h"
#include "time_compat.h"
#include "tool_registry.h"
#include <cctype>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <vector>

namespace delta {
namespace agent {

static nlohmann::json strip_id(nlohmann::json obj) {
    obj.erase("id");
    return obj;
}

// Resolve a model-provided start_time to a valid YYYY-MM-DDTHH:MM string.
// Handles: valid ISO, past-date correction, "tomorrow", bare "HH:MM", garbage.
std::string resolve_datetime(const std::string& raw) {
    time_t now = time(nullptr);
    struct tm t_now{};
    local_time(&now, &t_now);
    char today_buf[16];
    strftime(today_buf, sizeof(today_buf), "%Y-%m-%d", &t_now);
    std::string today(today_buf);

    time_t tomorrow_t = now + 24 * 3600;
    struct tm tm_tom{};
    local_time(&tomorrow_t, &tm_tom);
    char tom_buf[16];
    strftime(tom_buf, sizeof(tom_buf), "%Y-%m-%d", &tm_tom);
    std::string tomorrow(tom_buf);

    // Extract time from anywhere in the string.
    // Handles: HH:MM, HH:MM am/pm, Hpm/Ham, HHMM military, bare HH.
    auto extract_time = [](const std::string& s) -> std::string {
        std::string lower = s;
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        // 1. HH:MM pattern, optionally followed by am/pm
        for (size_t i = 0; i + 4 < lower.size(); i++) {
            if (std::isdigit(static_cast<unsigned char>(lower[i])) &&
                std::isdigit(static_cast<unsigned char>(lower[i + 1])) && lower[i + 2] == ':' &&
                std::isdigit(static_cast<unsigned char>(lower[i + 3])) &&
                std::isdigit(static_cast<unsigned char>(lower[i + 4]))) {
                int h = (lower[i] - '0') * 10 + (lower[i + 1] - '0');
                int m = (lower[i + 3] - '0') * 10 + (lower[i + 4] - '0');
                if (h < 24 && m < 60) {
                    size_t after = i + 5;
                    while (after < lower.size() && lower[after] == ' ')
                        after++;
                    if (after + 1 < lower.size()) {
                        if (lower[after] == 'p' && lower[after + 1] == 'm' && h < 12)
                            h += 12;
                        if (lower[after] == 'a' && lower[after + 1] == 'm' && h == 12)
                            h = 0;
                    }
                    char buf[6];
                    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
                    return std::string(buf);
                }
            }
        }

        // 2. Digit(s) followed by am/pm: "1pm", "2am", "11pm", "12am"
        for (size_t i = 0; i < lower.size(); i++) {
            if (!std::isdigit(static_cast<unsigned char>(lower[i])))
                continue;
            int h = lower[i] - '0';
            size_t j = i + 1;
            if (j < lower.size() && std::isdigit(static_cast<unsigned char>(lower[j]))) {
                h = h * 10 + (lower[j] - '0');
                j++;
            }
            size_t k = j;
            while (k < lower.size() && lower[k] == ' ')
                k++;
            if (k + 1 < lower.size()) {
                bool is_pm = (lower[k] == 'p' && lower[k + 1] == 'm');
                bool is_am = (lower[k] == 'a' && lower[k + 1] == 'm');
                if ((is_am || is_pm) && h >= 1 && h <= 12) {
                    if (is_pm && h != 12)
                        h += 12;
                    if (is_am && h == 12)
                        h = 0;
                    char buf[6];
                    snprintf(buf, sizeof(buf), "%02d:00", h);
                    return std::string(buf);
                }
            }
        }

        // 3. 4-digit military time: "1300" -> "13:00"
        for (size_t i = 0; i + 3 < lower.size(); i++) {
            if (i > 0 && (std::isdigit(static_cast<unsigned char>(lower[i - 1])) || lower[i - 1] == '-'))
                continue;
            if (std::isdigit(static_cast<unsigned char>(lower[i])) &&
                std::isdigit(static_cast<unsigned char>(lower[i + 1])) &&
                std::isdigit(static_cast<unsigned char>(lower[i + 2])) &&
                std::isdigit(static_cast<unsigned char>(lower[i + 3]))) {
                if (i + 4 < lower.size() &&
                    (std::isdigit(static_cast<unsigned char>(lower[i + 4])) || lower[i + 4] == '-'))
                    continue;
                int h = (lower[i] - '0') * 10 + (lower[i + 1] - '0');
                int m = (lower[i + 2] - '0') * 10 + (lower[i + 3] - '0');
                if (h < 24 && m < 60) {
                    char buf[6];
                    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
                    return std::string(buf);
                }
            }
        }

        // 4. Bare two-digit hour: "13" -> "13:00"
        for (size_t i = 0; i + 1 < lower.size(); i++) {
            if (std::isdigit(static_cast<unsigned char>(lower[i])) &&
                std::isdigit(static_cast<unsigned char>(lower[i + 1]))) {
                int h = (lower[i] - '0') * 10 + (lower[i + 1] - '0');
                if (h >= 0 && h <= 23 &&
                    (i + 2 >= lower.size() || !std::isdigit(static_cast<unsigned char>(lower[i + 2])))) {
                    char buf[6];
                    snprintf(buf, sizeof(buf), "%02d:00", h);
                    return std::string(buf);
                }
            }
        }

        return "09:00";
    };

    // Check if it's already valid ISO YYYY-MM-DDTHH:MM
    auto is_iso = [](const std::string& s) -> bool {
        return s.size() >= 16 && std::isdigit(static_cast<unsigned char>(s[0])) &&
               std::isdigit(static_cast<unsigned char>(s[3])) && s[4] == '-' && s[7] == '-' && s[10] == 'T' &&
               s[13] == ':';
    };

    if (is_iso(raw)) {
        std::string date_part = raw.substr(0, 10);
        std::string time_part = raw.substr(11, 5);
        if (date_part < today) {
            // Past date -- keep the time, use today
            std::cerr << "[delta-tool] corrected past date " << date_part << " -> " << today << std::endl;
            return today + "T" + time_part;
        }
        return raw.substr(0, 16);
    }

    // Lowercase for keyword matching
    std::string lower = raw;
    for (auto& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string time_part = extract_time(raw);
    std::string date_part = today;

    if (lower.find("tomorrow") != std::string::npos) {
        date_part = tomorrow;
    } else if (lower.find("today") != std::string::npos) {
        date_part = today;
    } else {
        // Check for day-of-week names and compute next occurrence
        static const char* day_names[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
        int current_wday = t_now.tm_wday; // 0=Sun
        for (int d = 0; d < 7; d++) {
            if (lower.find(day_names[d]) != std::string::npos) {
                int days_ahead = d - current_wday;
                if (days_ahead <= 0)
                    days_ahead += 7;
                time_t target = now + days_ahead * 24 * 3600;
                struct tm t_target{};
                local_time(&target, &t_target);
                char tbuf[16];
                strftime(tbuf, sizeof(tbuf), "%Y-%m-%d", &t_target);
                date_part = std::string(tbuf);
                break;
            }
        }
    }

    std::cerr << "[delta-tool] resolved \"" << raw << "\" -> " << date_part << "T" << time_part << std::endl;
    return date_part + "T" + time_part;
}

void register_calendar_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"create_event",
         "Create a calendar event, task, meeting, appointment, or reminder. "
         "Use type='task' for to-dos and actionable items.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Name of the event or task"}}},
            {"start_time",
             {{"type", "string"},
              {"description", "When: pass naturally ('friday 2pm', 'tomorrow 13:00', '1300') or as YYYY-MM-DDTHH:MM. "
                              "For tasks this is the deadline. Default 09:00 if no time given."}}},
            {"end_time",
             {{"type", "string"}, {"description", "End time: same formats as start_time (optional, events only)"}}},
            {"description", {{"type", "string"}, {"description", "Additional details (optional)"}}},
            {"location", {{"type", "string"}, {"description", "Where it takes place (optional)"}}},
            {"type",
             {{"type", "string"},
              {"enum", {"event", "task"}},
              {"description", "Type of item. 'event' for meetings/appointments, 'task' for to-dos/actionable items. "
                              "Default 'event'."}}},
            {"priority",
             {{"type", "string"},
              {"enum", {"low", "medium", "high", "urgent"}},
              {"description", "Priority level. Default 'medium'."}}},
            {"tags", {{"type", "string"}, {"description", "Comma-separated tags (optional)"}}},
            {"reminder_minutes",
             {{"type", "integer"},
              {"description",
               "Minutes before to send reminder. Events default 15, tasks default 0. Use 0 for no reminder."}}}}},
          {"required", nlohmann::json::array({"title", "start_time"})}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string start_time = resolve_datetime(args.value("start_time", ""));
            std::string title = args.value("title", "");

            std::string title_lower = title;
            for (auto& c : title_lower)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            std::vector<std::string> overlaps;
            if (!start_time.empty()) {
                auto existing = db.list_events(start_time, start_time, 10);
                for (auto& e : existing) {
                    if (e.value("start_time", "") != start_time)
                        continue;
                    std::string et = e.value("title", "");
                    std::string et_lower = et;
                    for (auto& c : et_lower)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (et_lower == title_lower) {
                        return {true, strip_id(e).dump(), ""};
                    }
                    overlaps.push_back(et);
                }
            }

            nlohmann::json resolved_args = args;
            resolved_args["start_time"] = start_time;
            if (resolved_args.contains("end_time") && resolved_args["end_time"].is_string())
                resolved_args["end_time"] = resolve_datetime(resolved_args["end_time"].get<std::string>());

            // Auto-detect type if not explicitly set by model
            if (!resolved_args.contains("type") || resolved_args["type"].get<std::string>() == "event") {
                static const char* task_keywords[] = {
                    "work on",  "finish",   "review", "prepare", "submit",  "fix",    "build",  "write",
                    "read",     "buy",      "clean",  "call",    "email",   "send",   "study",  "practice",
                    "complete", "update",   "check",  "setup",   "install", "deploy", "design", "plan",
                    "organize", "schedule", "remind", "pick up", nullptr};
                for (int k = 0; task_keywords[k]; k++) {
                    if (title_lower.find(task_keywords[k]) != std::string::npos) {
                        resolved_args["type"] = "task";
                        break;
                    }
                }
            }

            std::string id = db.create_event(resolved_args);
            if (id.empty())
                return {false, "", "Failed to create item."};
            auto event = db.get_event(id);
            auto result = strip_id(event);
            if (!overlaps.empty()) {
                nlohmann::json overlap_list = nlohmann::json::array();
                for (auto& t : overlaps)
                    overlap_list.push_back(t);
                result["overlaps"] = overlap_list;
                result["overlap_note"] = "Created, but overlaps with other item(s) at the same time.";
            }
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"list_events",
         "Show calendar events and tasks. Use for 'what's on my calendar', 'what do I have today', 'show my tasks', "
         "etc.",
         {{"type", "object"},
          {"properties",
           {{"start_date",
             {{"type", "string"}, {"description", "Start of range YYYY-MM-DD. Defaults to today if omitted."}}},
            {"end_date",
             {{"type", "string"},
              {"description", "End of range YYYY-MM-DD. Defaults to same as start_date if omitted."}}},
            {"type",
             {{"type", "string"}, {"enum", {"event", "task"}}, {"description", "Filter by type. Omit to show all."}}},
            {"status",
             {{"type", "string"},
              {"enum", {"upcoming", "in_progress", "completed", "cancelled"}},
              {"description", "Filter by status."}}},
            {"priority",
             {{"type", "string"},
              {"enum", {"low", "medium", "high", "urgent"}},
              {"description", "Filter by priority."}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string start = args.value("start_date", "");
            std::string end = args.value("end_date", "");
            if (start.empty()) {
                time_t now = time(nullptr);
                struct tm t{};
                local_time(&now, &t);
                char buf[16];
                strftime(buf, sizeof(buf), "%Y-%m-%d", &t);
                start = std::string(buf);
            }
            if (end.empty())
                end = start;
            auto events = db.list_events(start, end, args.value("limit", 50), args.value("type", ""),
                                         args.value("status", ""), args.value("priority", ""), args.value("tags", ""));
            nlohmann::json result = {{"items", nlohmann::json::array()}, {"count", events.size()}};
            for (auto& e : events)
                result["items"].push_back(strip_id(e));
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"delete_event",
         "Delete/remove a calendar event or task permanently.",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "Event/task ID from context (exact match)"}}},
            {"title", {{"type", "string"}, {"description", "Title (or partial match) to delete"}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");

            if (id.empty()) {
                std::string title = args.value("title", "");
                if (title.empty())
                    return {false, "", "Provide either id or title"};

                auto events = db.list_events("", "", 50);
                std::vector<nlohmann::json> matches;
                std::string title_lower = title;
                for (auto& c : title_lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                for (auto& e : events) {
                    std::string et = e.value("title", "");
                    std::string et_lower = et;
                    for (auto& c : et_lower)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (et_lower.find(title_lower) != std::string::npos) {
                        matches.push_back(e);
                    }
                }

                if (matches.empty())
                    return {false, "", "No items found matching \"" + title + "\""};

                if (matches.size() == 1) {
                    id = matches[0].value("id", "");
                } else {
                    nlohmann::json result = {{"message", "Multiple items match. Which one?"},
                                             {"matches", nlohmann::json::array()}};
                    for (auto& m : matches) {
                        result["matches"].push_back({{"title", m.value("title", "")},
                                                     {"start_time", m.value("start_time", "")},
                                                     {"type", m.value("type", "event")}});
                    }
                    return {true, result.dump(), ""};
                }
            }

            if (!db.delete_event(id))
                return {false, "", "Item not found"};
            return {true, R"({"deleted": true})", ""};
        });

    registry.register_tool(
        {"update_event",
         "Update, reschedule, or mark a calendar event or task as done. "
         "Use for 'move to 3pm', 'mark as done', 'change priority', etc.",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "Event/task ID from context (exact match)"}}},
            {"title", {{"type", "string"}, {"description", "Current title (or partial match) to find"}}},
            {"new_title", {{"type", "string"}, {"description", "New title if renaming"}}},
            {"start_time",
             {{"type", "string"},
              {"description", "New date/time: pass naturally ('friday 2pm', 'tomorrow') or as YYYY-MM-DDTHH:MM"}}},
            {"end_time", {{"type", "string"}, {"description", "New end time: same formats as start_time"}}},
            {"description", {{"type", "string"}, {"description", "Notes or description"}}},
            {"location", {{"type", "string"}, {"description", "New location"}}},
            {"status",
             {{"type", "string"},
              {"enum", {"upcoming", "in_progress", "completed", "cancelled"}},
              {"description", "done/completed = completed, cancel = cancelled, start/begin = in_progress"}}},
            {"priority",
             {{"type", "string"},
              {"enum", {"low", "medium", "high", "urgent"}},
              {"description", "New priority level"}}},
            {"tags", {{"type", "string"}, {"description", "New comma-separated tags"}}},
            {"reminder_minutes",
             {{"type", "integer"}, {"description", "Minutes before to send reminder. Use 0 for no reminder."}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");

            if (id.empty()) {
                std::string title = args.value("title", "");
                if (title.empty())
                    return {false, "", "Provide either id or title"};

                auto events = db.list_events("", "", 50);
                std::vector<nlohmann::json> matches;
                std::string title_lower = title;
                for (auto& c : title_lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                for (auto& e : events) {
                    std::string et = e.value("title", "");
                    std::string et_lower = et;
                    for (auto& c : et_lower)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (et_lower.find(title_lower) != std::string::npos) {
                        matches.push_back(e);
                    }
                }

                if (matches.empty())
                    return {false, "", "No items found matching \"" + title + "\""};

                if (matches.size() == 1) {
                    id = matches[0].value("id", "");
                } else {
                    nlohmann::json result = {{"message", "Multiple items match. Which one?"},
                                             {"matches", nlohmann::json::array()}};
                    for (auto& m : matches) {
                        result["matches"].push_back({{"title", m.value("title", "")},
                                                     {"start_time", m.value("start_time", "")},
                                                     {"type", m.value("type", "event")}});
                    }
                    return {true, result.dump(), ""};
                }
            }

            nlohmann::json update_data;
            if (args.contains("new_title"))
                update_data["title"] = args["new_title"];
            if (args.contains("start_time"))
                update_data["start_time"] = resolve_datetime(args["start_time"].get<std::string>());
            if (args.contains("end_time"))
                update_data["end_time"] = resolve_datetime(args["end_time"].get<std::string>());
            if (args.contains("description"))
                update_data["description"] = args["description"];
            if (args.contains("location"))
                update_data["location"] = args["location"];
            if (args.contains("status"))
                update_data["status"] = args["status"];
            if (args.contains("priority"))
                update_data["priority"] = args["priority"];
            if (args.contains("tags"))
                update_data["tags"] = args["tags"];
            if (args.contains("reminder_minutes"))
                update_data["reminder_minutes"] = args["reminder_minutes"];

            if (!db.update_event(id, update_data))
                return {false, "", "Item not found or update failed"};
            auto event = db.get_event(id);
            return {true, strip_id(event).dump(), ""};
        });

    registry.register_tool(
        {"get_current_time",
         "Get the current date and time. NOTE: CURRENT TIME, TODAY, and TOMORROW are already in the system prompt. "
         "Only call this if you need date info beyond what is provided above.",
         {{"type", "object"}, {"properties", nlohmann::json::object()}, {"required", nlohmann::json::array()}}},
        [](const nlohmann::json&) -> ToolResult {
            time_t now = time(nullptr);
            struct tm t{};
            local_time(&now, &t);
            char iso_buf[32], date_buf[16], day_buf[16];
            strftime(iso_buf, sizeof(iso_buf), "%Y-%m-%dT%H:%M:%S", &t);
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &t);
            strftime(day_buf, sizeof(day_buf), "%A", &t);

            time_t tomorrow_t = now + 24 * 3600;
            struct tm tm_tom{};
            local_time(&tomorrow_t, &tm_tom);
            char tom_buf[16], tom_day_buf[16];
            strftime(tom_buf, sizeof(tom_buf), "%Y-%m-%d", &tm_tom);
            strftime(tom_day_buf, sizeof(tom_day_buf), "%A", &tm_tom);

            nlohmann::json result = {{"datetime", std::string(iso_buf)},
                                     {"date", std::string(date_buf)},
                                     {"day_of_week", std::string(day_buf)},
                                     {"tomorrow", std::string(tom_buf)},
                                     {"tomorrow_day", std::string(tom_day_buf)}};
            return {true, result.dump(), ""};
        });
}

} // namespace agent
} // namespace delta
