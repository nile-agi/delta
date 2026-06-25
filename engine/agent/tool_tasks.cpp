#include "tool_tasks.h"
#include "agent_database.h"
#include "tool_registry.h"

namespace delta {
namespace agent {

void register_task_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool({"create_task",
                            "Create a new task or to-do item",
                            {{"type", "object"},
                             {"properties",
                              {{"title", {{"type", "string"}, {"description", "Task title"}}},
                               {"description", {{"type", "string"}, {"description", "Task description"}}},
                               {"priority",
                                {{"type", "string"},
                                 {"enum", {"low", "medium", "high", "urgent"}},
                                 {"description", "Task priority level"}}},
                               {"due_date", {{"type", "string"}, {"description", "Due date in ISO 8601 format"}}},
                               {"tags", {{"type", "string"}, {"description", "Comma-separated tags"}}}}},
                             {"required", {"title"}}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               std::string id = db.create_task(args);
                               if (id.empty())
                                   return {false, "", "Failed to create task"};
                               auto task = db.get_task(id);
                               return {true, task.dump(), ""};
                           });

    registry.register_tool(
        {"list_tasks",
         "List tasks, optionally filtered by status, priority, or tags",
         {{"type", "object"},
          {"properties",
           {{"status",
             {{"type", "string"},
              {"enum", {"pending", "in_progress", "completed", "cancelled"}},
              {"description", "Filter by task status"}}},
            {"priority",
             {{"type", "string"},
              {"enum", {"low", "medium", "high", "urgent"}},
              {"description", "Filter by priority"}}},
            {"tags", {{"type", "string"}, {"description", "Filter by tag (partial match)"}}},
            {"limit", {{"type", "integer"}, {"description", "Maximum number of tasks to return (default 50)"}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            auto tasks = db.list_tasks(args.value("status", ""), args.value("priority", ""), args.value("limit", 50),
                                       args.value("tags", ""));
            nlohmann::json result = {{"tasks", nlohmann::json::array()}, {"count", tasks.size()}};
            for (auto& t : tasks)
                result["tasks"].push_back(t);
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"update_task",
         "Update an existing task",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "The task ID to update"}}},
            {"title", {{"type", "string"}, {"description", "New task title"}}},
            {"description", {{"type", "string"}, {"description", "New description"}}},
            {"status",
             {{"type", "string"},
              {"enum", {"pending", "in_progress", "completed", "cancelled"}},
              {"description", "New status"}}},
            {"priority",
             {{"type", "string"}, {"enum", {"low", "medium", "high", "urgent"}}, {"description", "New priority"}}},
            {"due_date", {{"type", "string"}, {"description", "New due date (ISO 8601)"}}},
            {"tags", {{"type", "string"}, {"description", "New tags (comma-separated)"}}}}},
          {"required", {"id"}}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (id.empty())
                return {false, "", "Task ID is required"};
            if (!db.update_task(id, args))
                return {false, "", "Task not found or update failed"};
            auto task = db.get_task(id);
            return {true, task.dump(), ""};
        });

    registry.register_tool({"complete_task",
                            "Mark a task as completed",
                            {{"type", "object"},
                             {"properties", {{"id", {{"type", "string"}, {"description", "The task ID to complete"}}}}},
                             {"required", {"id"}}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               std::string id = args.value("id", "");
                               if (id.empty())
                                   return {false, "", "Task ID is required"};
                               if (!db.complete_task(id))
                                   return {false, "", "Task not found"};
                               auto task = db.get_task(id);
                               return {true, task.dump(), ""};
                           });

    registry.register_tool({"delete_task",
                            "Delete a task by ID",
                            {{"type", "object"},
                             {"properties", {{"id", {{"type", "string"}, {"description", "The task ID to delete"}}}}},
                             {"required", {"id"}}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               std::string id = args.value("id", "");
                               if (id.empty())
                                   return {false, "", "Task ID is required"};
                               if (!db.delete_task(id))
                                   return {false, "", "Task not found"};
                               return {true, R"({"deleted": true})", ""};
                           });
}

} // namespace agent
} // namespace delta
