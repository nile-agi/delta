#include "tool_calendar.h"
#include "agent_database.h"
#include "tool_registry.h"

namespace delta {
namespace agent {

void register_calendar_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"create_calendar_event",
         "Create a new calendar event with a title and time",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Event title"}}},
            {"start_time",
             {{"type", "string"}, {"description", "Start time in ISO 8601 format (e.g. 2025-01-15T14:00:00)"}}},
            {"end_time", {{"type", "string"}, {"description", "End time in ISO 8601 format"}}},
            {"description", {{"type", "string"}, {"description", "Event description"}}},
            {"location", {{"type", "string"}, {"description", "Event location"}}},
            {"all_day", {{"type", "boolean"}, {"description", "Whether this is an all-day event"}}}}},
          {"required", {"title", "start_time"}}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = db.create_event(args);
            if (id.empty())
                return {false, "", "Failed to create event"};
            auto event = db.get_event(id);
            return {true, event.dump(), ""};
        });

    registry.register_tool(
        {"list_calendar_events",
         "List calendar events, optionally filtered by date range",
         {{"type", "object"},
          {"properties",
           {{"start_date", {{"type", "string"}, {"description", "Filter events starting from this date (ISO 8601)"}}},
            {"end_date", {{"type", "string"}, {"description", "Filter events up to this date (ISO 8601)"}}},
            {"limit", {{"type", "integer"}, {"description", "Maximum number of events to return (default 50)"}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            auto events =
                db.list_events(args.value("start_date", ""), args.value("end_date", ""), args.value("limit", 50));
            nlohmann::json result = {{"events", nlohmann::json::array()}, {"count", events.size()}};
            for (auto& e : events)
                result["events"].push_back(e);
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"update_calendar_event",
         "Update an existing calendar event",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "The event ID to update"}}},
            {"title", {{"type", "string"}, {"description", "New event title"}}},
            {"start_time", {{"type", "string"}, {"description", "New start time (ISO 8601)"}}},
            {"end_time", {{"type", "string"}, {"description", "New end time (ISO 8601)"}}},
            {"description", {{"type", "string"}, {"description", "New description"}}},
            {"location", {{"type", "string"}, {"description", "New location"}}},
            {"all_day", {{"type", "boolean"}, {"description", "Whether this is an all-day event"}}}}},
          {"required", {"id"}}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (id.empty())
                return {false, "", "Event ID is required"};
            if (!db.update_event(id, args))
                return {false, "", "Event not found or update failed"};
            auto event = db.get_event(id);
            return {true, event.dump(), ""};
        });

    registry.register_tool({"delete_calendar_event",
                            "Delete a calendar event by ID",
                            {{"type", "object"},
                             {"properties", {{"id", {{"type", "string"}, {"description", "The event ID to delete"}}}}},
                             {"required", {"id"}}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               std::string id = args.value("id", "");
                               if (id.empty())
                                   return {false, "", "Event ID is required"};
                               if (!db.delete_event(id))
                                   return {false, "", "Event not found"};
                               return {true, R"({"deleted": true})", ""};
                           });
}

} // namespace agent
} // namespace delta
