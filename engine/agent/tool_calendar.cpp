#include "tool_calendar.h"
#include "agent_database.h"
#include "tool_registry.h"
#include <cctype>
#include <vector>

namespace delta {
namespace agent {

void register_calendar_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool({"create_event",
                            "Create a calendar event",
                            {{"type", "object"},
                             {"properties",
                              {{"title", {{"type", "string"}}},
                               {"start_time", {{"type", "string"}, {"description", "ISO 8601 datetime"}}},
                               {"end_time", {{"type", "string"}}},
                               {"description", {{"type", "string"}}},
                               {"location", {{"type", "string"}}}}},
                             {"required", nlohmann::json::array({"title", "start_time"})}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               std::string start_time = args.value("start_time", "");
                               if (!start_time.empty()) {
                                   auto existing = db.list_events(start_time, start_time, 1);
                                   if (!existing.empty()) {
                                       return {false, "",
                                               "Time conflict: \"" + existing[0].value("title", "") +
                                                   "\" is already scheduled at " + start_time +
                                                   ". Choose a different time or update the existing event."};
                                   }
                               }
                               std::string id = db.create_event(args);
                               if (id.empty())
                                   return {false, "", "Failed to create event"};
                               auto event = db.get_event(id);
                               return {true, event.dump(), ""};
                           });

    registry.register_tool({"list_events",
                            "List calendar events by date range",
                            {{"type", "object"},
                             {"properties",
                              {{"start_date", {{"type", "string"}, {"description", "YYYY-MM-DD"}}},
                               {"end_date", {{"type", "string"}, {"description", "YYYY-MM-DD"}}}}},
                             {"required", nlohmann::json::array()}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               auto events = db.list_events(args.value("start_date", ""), args.value("end_date", ""),
                                                            args.value("limit", 50));
                               nlohmann::json result = {{"events", nlohmann::json::array()}, {"count", events.size()}};
                               for (auto& e : events)
                                   result["events"].push_back(e);
                               return {true, result.dump(), ""};
                           });

    registry.register_tool({"delete_event",
                            "Delete a calendar event by title or id",
                            {{"type", "object"},
                             {"properties",
                              {{"id", {{"type", "string"}}},
                               {"title", {{"type", "string"}, {"description", "Event title to search and delete"}}}}},
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
                                       return {false, "", "No events found matching \"" + title + "\""};

                                   if (matches.size() == 1) {
                                       id = matches[0].value("id", "");
                                   } else {
                                       nlohmann::json result = {{"message", "Multiple events match. Which one?"},
                                                                {"matches", nlohmann::json::array()}};
                                       for (auto& m : matches) {
                                           result["matches"].push_back({{"id", m.value("id", "")},
                                                                        {"title", m.value("title", "")},
                                                                        {"start_time", m.value("start_time", "")}});
                                       }
                                       return {true, result.dump(), ""};
                                   }
                               }

                               if (!db.delete_event(id))
                                   return {false, "", "Event not found"};
                               return {true, R"({"deleted": true})", ""};
                           });

    registry.register_tool({"update_event",
                            "Update a calendar event's time, title, or details. Use title to find the event.",
                            {{"type", "object"},
                             {"properties",
                              {{"id", {{"type", "string"}}},
                               {"title", {{"type", "string"}, {"description", "Event title to find"}}},
                               {"new_title", {{"type", "string"}, {"description", "New title for the event"}}},
                               {"start_time", {{"type", "string"}, {"description", "New start time ISO 8601"}}},
                               {"end_time", {{"type", "string"}, {"description", "New end time ISO 8601"}}},
                               {"description", {{"type", "string"}}},
                               {"location", {{"type", "string"}}}}},
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
                                       return {false, "", "No events found matching \"" + title + "\""};

                                   if (matches.size() == 1) {
                                       id = matches[0].value("id", "");
                                   } else {
                                       nlohmann::json result = {{"message", "Multiple events match. Which one?"},
                                                                {"matches", nlohmann::json::array()}};
                                       for (auto& m : matches) {
                                           result["matches"].push_back({{"id", m.value("id", "")},
                                                                        {"title", m.value("title", "")},
                                                                        {"start_time", m.value("start_time", "")}});
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
                               return {true, event.dump(), ""};
                           });

    registry.register_tool(
        {"get_current_time",
         "Get current date and time",
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
