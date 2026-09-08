#include "tool_notes.h"
#include "tool_registry.h"
#include "agent_database.h"
#include "../vendor/json.hpp"

namespace delta {
namespace agent {

using json = nlohmann::json;

void register_note_tools() {
    // list_notes
    ToolRegistry::instance().register_tool(
        {"list_notes",
         "List the notes the user has written, newest first, optionally filtered. A note is a "
         "document in their own words; use read_file for files on disk and recall for facts you "
         "saved about them.",
         {{"type", "object"},
          {"properties",
           {{"folder", {{"type", "string"}, {"description", "Folder to filter by"}}},
            {"search", {{"type", "string"}, {"description", "Search query"}}},
            {"tags", {{"type", "string"}, {"description", "Comma-separated tags"}}},
            {"limit", {{"type", "integer"}, {"description", "Max results"}}},
            {"pinned_only", {{"type", "boolean"}, {"description", "Only pinned notes"}}}}},
          {"required", json::array()}}},
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string folder = args.value("folder", "");
            std::string search = args.value("search", "");
            std::string tags = args.value("tags", "");
            int limit = args.value("limit", 50);
            bool pinned_only = args.value("pinned_only", false);
            auto notes = db.list_notes(folder, search, tags, limit, pinned_only);
            json notes_array = json::array();
            for (auto& n : notes)
                notes_array.push_back(n);
            return {true, notes_array.dump(), ""};
        });

    // create_note
    ToolRegistry::instance().register_tool(
        {"create_note",
         "Write a new note for the user: minutes, a draft, a list, anything they would want to read "
         "back later. Not for things you need to remember yourself -- use remember for a lasting "
         "fact and note_to_self for working notes on the job in hand.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Note title"}}},
            {"content", {{"type", "string"}, {"description", "Note content"}}},
            {"folder", {{"type", "string"}, {"description", "Folder name"}}},
            {"tags", {{"type", "string"}, {"description", "Comma-separated tags"}}},
            {"pinned", {{"type", "boolean"}, {"description", "Pin the note"}}}}},
          {"required", {"title", "content"}}}},
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = db.create_note(args);
            if (id.empty()) {
                return {false, "", "Failed to create note"};
            }
            auto note = db.get_note(id);
            return {true, note.dump(), ""};
        });

    // get_note
    ToolRegistry::instance().register_tool(
        {"get_note",
         "Read one note in full, by the id that list_notes returned. Use this before updating a note "
         "so you keep what is already in it.",
         {{"type", "object"},
          {"properties", {{"id", {{"type", "string"}, {"description", "Note ID"}}}}},
          {"required", {"id"}}}},
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            auto note = db.get_note(id);
            if (note.is_null()) {
                return {false, "", "Note not found"};
            }
            return {true, note.dump(), ""};
        });

    // update_note
    ToolRegistry::instance().register_tool(
        {"update_note",
         "Change a note the user already has. Replaces whatever fields you pass, so read it with "
         "get_note first when you only mean to change part of it.",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "Note ID"}}},
            {"title", {{"type", "string"}, {"description", "New title"}}},
            {"content", {{"type", "string"}, {"description", "New content"}}},
            {"folder", {{"type", "string"}, {"description", "Folder name"}}},
            {"tags", {{"type", "string"}, {"description", "Comma-separated tags"}}},
            {"pinned", {{"type", "boolean"}, {"description", "Pin status"}}}}},
          {"required", {"id"}}}},
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (!db.update_note(id, args)) {
                return {false, "", "Note not found"};
            }
            auto note = db.get_note(id);
            return {true, note.dump(), ""};
        });

    // delete_note
    ToolRegistry::instance().register_tool(
        {"delete_note",
         "Delete one of the user's notes permanently. There is no undo, so be sure they asked for it.",
         {{"type", "object"},
          {"properties", {{"id", {{"type", "string"}, {"description", "Note ID"}}}}},
          {"required", {"id"}}}},
        [](const json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (!db.delete_note(id)) {
                return {false, "", "Note not found"};
            }
            return {true, R"({"deleted":true})", ""};
        });
}

} // namespace agent
} // namespace delta