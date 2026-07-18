#include "tool_calendar.h"
#include "agent_database.h"
#include "tool_registry.h"
#include <cctype>
#include <ctime>
#include <vector>

namespace delta {
namespace agent {

static nlohmann::json strip_id(nlohmann::json obj) {
    obj.erase("id");
    return obj;
}

void register_calendar_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"create_event",
         "Schedule a new calendar event, meeting, appointment, or reminder.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Name of the event"}}},
            {"start_time",
             {{"type", "string"},
              {"description", "Start datetime as YYYY-MM-DDTHH:MM. Default to 09:00 if no time given."}}},
            {"end_time", {{"type", "string"}, {"description", "End datetime as YYYY-MM-DDTHH:MM (optional)"}}},
            {"description", {{"type", "string"}, {"description", "Additional details (optional)"}}},
            {"location", {{"type", "string"}, {"description", "Where the event takes place (optional)"}}}}},
          {"required", nlohmann::json::array({"title", "start_time"})}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string start_time = args.value("start_time", "");
            if (!start_time.empty()) {
                auto existing = db.list_events(start_time, start_time, 1);
                if (!existing.empty()) {
                    return {false, "",
                            "Time conflict: \"" + existing[0].value("title", "") + "\" is already scheduled at " +
                                start_time + ". Choose a different time or update the existing event."};
                }
            }
            std::string id = db.create_event(args);
            if (id.empty())
                return {false, "", "Failed to create event"};
            auto event = db.get_event(id);
            return {true, strip_id(event).dump(), ""};
        });

    registry.register_tool(
        {"list_events",
         "Show calendar events. Use for 'what's on my calendar', 'what do I have today/this week', 'any events on "
         "Friday', etc.",
         {{"type", "object"},
          {"properties",
           {{"start_date",
             {{"type", "string"}, {"description", "Start of range YYYY-MM-DD. Defaults to today if omitted."}}},
            {"end_date",
             {{"type", "string"},
              {"description", "End of range YYYY-MM-DD. Defaults to same as start_date if omitted."}}}}},
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
            auto events = db.list_events(start, end, args.value("limit", 50));
            nlohmann::json result = {{"events", nlohmann::json::array()}, {"count", events.size()}};
            for (auto& e : events)
                result["events"].push_back(strip_id(e));
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"delete_event",
         "Delete/remove a calendar event permanently. Use when user wants to cancel or remove an event.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Event title (or partial match) to delete"}}}}},
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
                    return {false, "", "No events found matching \"" + title + "\""};

                if (matches.size() == 1) {
                    id = matches[0].value("id", "");
                } else {
                    nlohmann::json result = {{"message", "Multiple events match. Which one?"},
                                             {"matches", nlohmann::json::array()}};
                    for (auto& m : matches) {
                        result["matches"].push_back(
                            {{"title", m.value("title", "")}, {"start_time", m.value("start_time", "")}});
                    }
                    return {true, result.dump(), ""};
                }
            }

            if (!db.delete_event(id))
                return {false, "", "Event not found"};
            return {true, R"({"deleted": true})", ""};
        });

    registry.register_tool(
        {"update_event",
         "Reschedule, move, or edit a calendar event. Use for 'move meeting to 3pm', 'reschedule to tomorrow', 'change "
         "location', etc.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Current event title (or partial match) to find"}}},
            {"new_title", {{"type", "string"}, {"description", "New title if renaming the event"}}},
            {"start_time", {{"type", "string"}, {"description", "New start datetime YYYY-MM-DDTHH:MM"}}},
            {"end_time", {{"type", "string"}, {"description", "New end datetime YYYY-MM-DDTHH:MM"}}},
            {"description", {{"type", "string"}, {"description", "New description"}}},
            {"location", {{"type", "string"}, {"description", "New location"}}}}},
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
                    return {false, "", "No events found matching \"" + title + "\""};

                if (matches.size() == 1) {
                    id = matches[0].value("id", "");
                } else {
                    nlohmann::json result = {{"message", "Multiple events match. Which one?"},
                                             {"matches", nlohmann::json::array()}};
                    for (auto& m : matches) {
                        result["matches"].push_back(
                            {{"title", m.value("title", "")}, {"start_time", m.value("start_time", "")}});
                    }
                    return {true, result.dump(), ""};
                }
            }

            nlohmann::json update_data;
            if (args.contains("new_title"))
                update_data["title"] = args["new_title"];
            if (args.contains("start_time"))
                update_data["start_time"] = args["start_time"];
            else if (args.contains("new_start_time"))
                update_data["start_time"] = args["new_start_time"];
            if (args.contains("end_time"))
                update_data["end_time"] = args["end_time"];
            else if (args.contains("new_end_time"))
                update_data["end_time"] = args["new_end_time"];
            if (args.contains("description"))
                update_data["description"] = args["description"];
            if (args.contains("location"))
                update_data["location"] = args["location"];

            if (!db.update_event(id, update_data))
                return {false, "", "Event not found or update failed"};
            auto event = db.get_event(id);
            return {true, strip_id(event).dump(), ""};
        });

    registry.register_tool(
        {"get_current_time",
         "Get the current date and time. Use when user asks 'what time is it', 'what's the date', etc.",
         {{"type", "object"}, {"properties", nlohmann::json::object()}, {"required", nlohmann::json::array()}}},
        [](const nlohmann::json&) -> ToolResult {
            time_t now = time(nullptr);
            struct tm t{};
#ifdef _WIN32
            localtime_s(&t, &now);
#else
            localtime_r(&now, &t);
#endif
            char iso_buf[32], date_buf[16];
            strftime(iso_buf, sizeof(iso_buf), "%Y-%m-%dT%H:%M:%S", &t);
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &t);
            nlohmann::json result = {{"datetime", std::string(iso_buf)}, {"date", std::string(date_buf)}};
            return {true, result.dump(), ""};
        });
}

} // namespace agent
} // namespace delta
