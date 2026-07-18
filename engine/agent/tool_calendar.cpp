#include "tool_calendar.h"
#include "agent_database.h"
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
static std::string resolve_datetime(const std::string& raw) {
    time_t now = time(nullptr);
    struct tm t_now{};
#ifdef _WIN32
    localtime_s(&t_now, &now);
#else
    localtime_r(&now, &t_now);
#endif
    char today_buf[16];
    strftime(today_buf, sizeof(today_buf), "%Y-%m-%d", &t_now);
    std::string today(today_buf);

    time_t tomorrow_t = now + 24 * 3600;
    struct tm tm_tom{};
#ifdef _WIN32
    localtime_s(&tm_tom, &tomorrow_t);
#else
    localtime_r(&tomorrow_t, &tm_tom);
#endif
    char tom_buf[16];
    strftime(tom_buf, sizeof(tom_buf), "%Y-%m-%d", &tm_tom);
    std::string tomorrow(tom_buf);

    // Extract HH:MM from anywhere in the string (first match)
    auto extract_time = [](const std::string& s) -> std::string {
        for (size_t i = 0; i + 4 < s.size(); i++) {
            if (std::isdigit(static_cast<unsigned char>(s[i])) && std::isdigit(static_cast<unsigned char>(s[i + 1])) &&
                s[i + 2] == ':' && std::isdigit(static_cast<unsigned char>(s[i + 3])) &&
                std::isdigit(static_cast<unsigned char>(s[i + 4]))) {
                int h = (s[i] - '0') * 10 + (s[i + 1] - '0');
                int m = (s[i + 3] - '0') * 10 + (s[i + 4] - '0');
                if (h < 24 && m < 60)
                    return s.substr(i, 5);
            }
        }
        // Try bare hour like "1300" or "13"
        for (size_t i = 0; i + 1 < s.size(); i++) {
            if (std::isdigit(static_cast<unsigned char>(s[i])) && std::isdigit(static_cast<unsigned char>(s[i + 1]))) {
                int h = (s[i] - '0') * 10 + (s[i + 1] - '0');
                if (h >= 0 && h <= 23 && (i + 2 >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i + 2])))) {
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
    }
    // else default to today

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
              {"description",
               "Datetime as YYYY-MM-DDTHH:MM. For tasks this is the deadline. Default to 09:00 if no time given."}}},
            {"end_time",
             {{"type", "string"}, {"description", "End datetime as YYYY-MM-DDTHH:MM (optional, events only)"}}},
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
#ifdef _WIN32
                localtime_s(&t, &now);
#else
                localtime_r(&now, &t);
#endif
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
          {"properties", {{"title", {{"type", "string"}, {"description", "Title (or partial match) to delete"}}}}},
          {"required", nlohmann::json::array({"title"})}}},
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
           {{"title", {{"type", "string"}, {"description", "Current title (or partial match) to find"}}},
            {"new_title", {{"type", "string"}, {"description", "New title if renaming"}}},
            {"start_time", {{"type", "string"}, {"description", "New datetime YYYY-MM-DDTHH:MM"}}},
            {"end_time", {{"type", "string"}, {"description", "New end datetime YYYY-MM-DDTHH:MM"}}},
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
          {"required", nlohmann::json::array({"title"})}}},
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
                update_data["end_time"] = args["end_time"];
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
         "Get the current date and time. Call this FIRST whenever you need to resolve relative dates "
         "like 'today', 'tomorrow', 'next week', 'this Friday', etc. Use the returned date to compute "
         "the correct YYYY-MM-DD value before creating or updating events and tasks.",
         {{"type", "object"}, {"properties", nlohmann::json::object()}, {"required", nlohmann::json::array()}}},
        [](const nlohmann::json&) -> ToolResult {
            time_t now = time(nullptr);
            struct tm t{};
#ifdef _WIN32
            localtime_s(&t, &now);
#else
            localtime_r(&now, &t);
#endif
            char iso_buf[32], date_buf[16], day_buf[16];
            strftime(iso_buf, sizeof(iso_buf), "%Y-%m-%dT%H:%M:%S", &t);
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &t);
            strftime(day_buf, sizeof(day_buf), "%A", &t);

            time_t tomorrow_t = now + 24 * 3600;
            struct tm tm_tom{};
#ifdef _WIN32
            localtime_s(&tm_tom, &tomorrow_t);
#else
            localtime_r(&tomorrow_t, &tm_tom);
#endif
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
