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

    registry.register_tool({"create_task",
                            "Create a task",
                            {{"type", "object"},
                             {"properties",
                              {{"title", {{"type", "string"}}},
                               {"priority", {{"type", "string"}, {"enum", {"low", "medium", "high", "urgent"}}}},
                               {"due_date", {{"type", "string"}, {"description", "YYYY-MM-DD"}}}}},
                             {"required", nlohmann::json::array({"title"})}}},
                           [](const nlohmann::json& args) -> ToolResult {
                               auto& db = AgentDatabase::instance();
                               std::string id = db.create_task(args);
                               if (id.empty())
                                   return {false, "", "Failed to create task"};
                               auto task = db.get_task(id);
                               return {true, strip_id(task).dump(), ""};
                           });

    registry.register_tool(
        {"list_tasks",
         "List tasks",
         {{"type", "object"},
          {"properties",
           {{"status", {{"type", "string"}, {"enum", {"pending", "in_progress", "completed", "cancelled"}}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            auto tasks = db.list_tasks(args.value("status", ""), args.value("priority", ""), args.value("limit", 50),
                                       args.value("tags", ""));
            nlohmann::json result = {{"tasks", nlohmann::json::array()}, {"count", tasks.size()}};
            for (auto& t : tasks)
                result["tasks"].push_back(strip_id(t));
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"update_task",
         "Update a task status or details. Use title to find the task.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Task title to find"}}},
            {"new_title", {{"type", "string"}, {"description", "New title for the task"}}},
            {"status", {{"type", "string"}, {"enum", {"pending", "in_progress", "completed", "cancelled"}}}},
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
         "Delete a task by title",
         {{"type", "object"},
          {"properties", {{"title", {{"type", "string"}, {"description", "Task title to search and delete"}}}}},
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
