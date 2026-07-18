#include "tool_tasks.h"
#include "agent_database.h"
#include "tool_registry.h"
#include <cctype>
#include <vector>

namespace delta {
namespace agent {

static nlohmann::json strip_id(nlohmann::json obj) {
    obj.erase("id");
    return obj;
}

void register_task_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"create_task",
         "Create a new task or to-do item. Use when the user wants to add, remember, or track something.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "What the task is about"}}},
            {"priority",
             {{"type", "string"},
              {"enum", {"low", "medium", "high", "urgent"}},
              {"description", "Defaults to medium if not specified"}}},
            {"due_date", {{"type", "string"}, {"description", "Due date in YYYY-MM-DD format"}}}}},
          {"required", nlohmann::json::array({"title"})}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = db.create_task(args);
            if (id.empty())
                return {false, "", "Failed to create task"};
            auto task = db.get_task(id);
            return {true, strip_id(task).dump(), ""};
        });

    registry.register_tool({"list_tasks",
                            "List the user's tasks. Use when user asks to see, show, or check their tasks or to-dos.",
                            {{"type", "object"},
                             {"properties",
                              {{"status",
                                {{"type", "string"},
                                 {"enum", {"pending", "in_progress", "completed", "cancelled"}},
                                 {"description", "Filter by status. Omit to show all active tasks."}}}}},
                             {"required", nlohmann::json::array()}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               auto tasks = db.list_tasks(args.value("status", ""), args.value("priority", ""),
                                                          args.value("limit", 50), args.value("tags", ""));
                               nlohmann::json result = {{"tasks", nlohmann::json::array()}, {"count", tasks.size()}};
                               for (auto& t : tasks)
                                   result["tasks"].push_back(strip_id(t));
                               return {true, result.dump(), ""};
                           });

    registry.register_tool(
        {"update_task",
         "Update a task: mark as done/completed, cancel, change priority, rename, or set due date. Use title to find "
         "it.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Current task title (or partial match) to find"}}},
            {"new_title", {{"type", "string"}, {"description", "New title if renaming the task"}}},
            {"status",
             {{"type", "string"},
              {"enum", {"pending", "in_progress", "completed", "cancelled"}},
              {"description", "done/finish = completed, cancel = cancelled, start = in_progress"}}},
            {"priority", {{"type", "string"}, {"enum", {"low", "medium", "high", "urgent"}}}},
            {"due_date", {{"type", "string"}, {"description", "YYYY-MM-DD"}}}}},
          {"required", nlohmann::json::array({"title"})}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");

            if (id.empty()) {
                std::string title = args.value("title", "");
                if (title.empty())
                    return {false, "", "Provide either id or title"};

                auto tasks = db.list_tasks("", "", 50, "");
                std::vector<nlohmann::json> matches;
                std::string title_lower = title;
                for (auto& c : title_lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                for (auto& t : tasks) {
                    std::string tt = t.value("title", "");
                    std::string tt_lower = tt;
                    for (auto& c : tt_lower)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (tt_lower.find(title_lower) != std::string::npos) {
                        matches.push_back(t);
                    }
                }

                if (matches.empty())
                    return {false, "", "No tasks found matching \"" + title + "\""};

                if (matches.size() == 1) {
                    id = matches[0].value("id", "");
                } else {
                    nlohmann::json result = {{"message", "Multiple tasks match. Which one?"},
                                             {"matches", nlohmann::json::array()}};
                    for (auto& m : matches) {
                        result["matches"].push_back(
                            {{"title", m.value("title", "")}, {"status", m.value("status", "")}});
                    }
                    return {true, result.dump(), ""};
                }
            }

            nlohmann::json update_data;
            if (args.contains("new_title"))
                update_data["title"] = args["new_title"];
            if (args.contains("status"))
                update_data["status"] = args["status"];
            if (args.contains("priority"))
                update_data["priority"] = args["priority"];
            if (args.contains("due_date"))
                update_data["due_date"] = args["due_date"];

            if (!db.update_task(id, update_data))
                return {false, "", "Task not found or update failed"};
            auto task = db.get_task(id);
            return {true, strip_id(task).dump(), ""};
        });

    registry.register_tool(
        {"delete_task",
         "Permanently delete/remove a task. Use when user wants to get rid of a task entirely (not just mark "
         "complete).",
         {{"type", "object"},
          {"properties", {{"title", {{"type", "string"}, {"description", "Task title (or partial match) to delete"}}}}},
          {"required", nlohmann::json::array({"title"})}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");

            if (id.empty()) {
                std::string title = args.value("title", "");
                if (title.empty())
                    return {false, "", "Provide either id or title"};

                auto tasks = db.list_tasks("", "", 50, "");
                std::vector<nlohmann::json> matches;
                std::string title_lower = title;
                for (auto& c : title_lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                for (auto& t : tasks) {
                    std::string tt = t.value("title", "");
                    std::string tt_lower = tt;
                    for (auto& c : tt_lower)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (tt_lower.find(title_lower) != std::string::npos) {
                        matches.push_back(t);
                    }
                }

                if (matches.empty())
                    return {false, "", "No tasks found matching \"" + title + "\""};

                if (matches.size() == 1) {
                    id = matches[0].value("id", "");
                } else {
                    nlohmann::json result = {{"message", "Multiple tasks match. Which one?"},
                                             {"matches", nlohmann::json::array()}};
                    for (auto& m : matches) {
                        result["matches"].push_back(
                            {{"title", m.value("title", "")}, {"status", m.value("status", "")}});
                    }
                    return {true, result.dump(), ""};
                }
            }

            if (!db.delete_task(id))
                return {false, "", "Task not found"};
            return {true, R"({"deleted": true})", ""};
        });
}

} // namespace agent
} // namespace delta
