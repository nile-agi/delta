#include "tool_notes.h"
#include "tool_registry.h"
#include "agent_database.h"
#include <nlohmann/json.hpp>

namespace delta {
namespace agent {

using json = nlohmann::json;

void register_note_tools() {
    // list_notes
    ToolRegistry::instance().register_tool(
        {
            "list_notes",
            "List notes with optional filters",
            {
                {"type", "object"},
                {"properties", {
                    {"folder", {{"type", "string"}, {"description", "Folder to filter by"}}},
                    {"search", {{"type", "string"}, {"description", "Search query"}}},
                    {"tags", {{"type", "string"}, {"description", "Comma-separated tags"}}},
                    {"limit", {{"type", "integer"}, {"description", "Max results"}}},
                    {"pinned_only", {{"type", "boolean"}, {"description", "Only pinned notes"}}}
                }},
                {"required", json::array()}
            }
        },
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string folder = args.value("folder", "");
            std::string search = args.value("search", "");
            std::string tags   = args.value("tags", "");
            int limit          = args.value("limit", 50);
            bool pinned_only   = args.value("pinned_only", false);
            auto notes = db.list_notes(folder, search, tags, limit, pinned_only);
            json notes_array = json::array();
            for (auto& n : notes) notes_array.push_back(n);
            return {true, notes_array.dump(), ""};
        }
    );

    // create_note
    ToolRegistry::instance().register_tool(
        {
            "create_note",
            "Create a new note",
            {
                {"type", "object"},
                {"properties", {
                    {"title",   {{"type", "string"}, {"description", "Note title"}}},
                    {"content", {{"type", "string"}, {"description", "Note content"}}},
                    {"folder",  {{"type", "string"}, {"description", "Folder name"}}},
                    {"tags",    {{"type", "string"}, {"description", "Comma-separated tags"}}},
                    {"pinned",  {{"type", "boolean"}, {"description", "Pin the note"}}}
                }},
                {"required", {"title", "content"}}
            }
        },
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = db.create_note(args);
            if (id.empty()) {
                return {false, "", "Failed to create note"};
            }
            auto note = db.get_note(id);
            return {true, note.dump(), ""};
        }
    );

    // get_note
    ToolRegistry::instance().register_tool(
        {
            "get_note",
            "Get a single note by ID",
            {
                {"type", "object"},
                {"properties", {
                    {"id", {{"type", "string"}, {"description", "Note ID"}}}
                }},
                {"required", {"id"}}
            }
        },
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            auto note = db.get_note(id);
            if (note.is_null()) {
                return {false, "", "Note not found"};
            }
            return {true, note.dump(), ""};
        }
    );

    // update_note
    ToolRegistry::instance().register_tool(
        {
            "update_note",
            "Update an existing note",
            {
                {"type", "object"},
                {"properties", {
                    {"id",      {{"type", "string"}, {"description", "Note ID"}}},
                    {"title",   {{"type", "string"}, {"description", "New title"}}},
                    {"content", {{"type", "string"}, {"description", "New content"}}},
                    {"folder",  {{"type", "string"}, {"description", "Folder name"}}},
                    {"tags",    {{"type", "string"}, {"description", "Comma-separated tags"}}},
                    {"pinned",  {{"type", "boolean"}, {"description", "Pin status"}}}
                }},
                {"required", {"id"}}
            }
        },
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (!db.update_note(id, args)) {
                return {false, "", "Note not found"};
            }
            auto note = db.get_note(id);
            return {true, note.dump(), ""};
        }
    );

    // delete_note
    ToolRegistry::instance().register_tool(
        {
            "delete_note",
            "Delete a note by ID",
            {
                {"type", "object"},
                {"properties", {
                    {"id", {{"type", "string"}, {"description", "Note ID"}}}
                }},
                {"required", {"id"}}
            }
        },
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (!db.delete_note(id)) {
                return {false, "", "Note not found"};
            }
            return {true, R"({"deleted":true})", ""};
        }
    );
}

} // namespace agent
} // namespace delta