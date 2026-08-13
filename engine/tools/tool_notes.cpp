#include "tool_notes.h"
#include "agent_database.h"
#include "tool_registry.h"

namespace delta {
namespace agent {

static nlohmann::json strip_id(nlohmann::json obj) {
    obj.erase("id");
    return obj;
}

void register_note_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"create_note",
         "Create a new note, memo, or document.",
         {{"type", "object"},
          {"properties",
           {{"title", {{"type", "string"}, {"description", "Title of the note"}}},
            {"content", {{"type", "string"}, {"description", "Body text of the note"}}},
            {"folder", {{"type", "string"}, {"description", "Folder/category. Default 'General'."}}},
            {"tags", {{"type", "string"}, {"description", "Comma-separated tags"}}},
            {"pinned", {{"type", "boolean"}, {"description", "Pin to top. Default false."}}}}},
          {"required", nlohmann::json::array({"title", "content"})}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = db.create_note(args);
            if (id.empty()) return {false, "", "Failed to create note."};
            auto note = db.get_note(id);
            return {true, strip_id(note).dump(), ""};
        });

    registry.register_tool(
        {"list_notes",
         "List, search, or browse saved notes.",
         {{"type", "object"},
          {"properties",
           {{"folder", {{"type", "string"}, {"description", "Filter by folder name"}}},
            {"search", {{"type", "string"}, {"description", "Search in title and content"}}},
            {"tags", {{"type", "string"}, {"description", "Filter by tag"}}},
            {"pinned_only", {{"type", "boolean"}, {"description", "Show only pinned notes"}}},
            {"limit", {{"type", "integer"}, {"description", "Max results. Default 20."}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            auto notes = db.list_notes(
                args.value("folder", ""),
                args.value("search", ""),
                args.value("tags", ""),
                args.value("limit", 20),
                args.value("pinned_only", false));
            nlohmann::json result = {{"notes", nlohmann::json::array()}, {"count", notes.size()}};
            for (auto& n : notes) result["notes"].push_back(strip_id(n));
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"get_note",
         "Retrieve a single note by ID or title.",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "Exact note ID"}}},
            {"title", {{"type", "string"}, {"description", "Title to search for"}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (!id.empty()) {
                auto note = db.get_note(id);
                if (note.is_null()) return {false, "", "Note not found."};
                return {true, strip_id(note).dump(), ""};
            }
            std::string title = args.value("title", "");
            if (title.empty()) return {false, "", "Provide id or title."};
            auto notes = db.list_notes("", title, "", 10, false);
            if (notes.empty()) return {false, "", "No notes found matching '" + title + "'"};
            if (notes.size() == 1) return {true, strip_id(notes[0]).dump(), ""};
            nlohmann::json result = {{"message", "Multiple matches. Which one?"}, {"matches", nlohmann::json::array()}};
            for (auto& n : notes) {
                result["matches"].push_back({{"title", n["title"]}, {"folder", n["folder"]}, {"updated_at", n["updated_at"]}});
            }
            return {true, result.dump(), ""};
        });

    registry.register_tool(
        {"update_note",
         "Edit, rename, move, pin/unpin, or append to a note.",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "Exact note ID"}}},
            {"title", {{"type", "string"}, {"description", "Current title to find the note"}}},
            {"new_title", {{"type", "string"}, {"description", "New title if renaming"}}},
            {"content", {{"type", "string"}, {"description", "New full content"}}},
            {"append", {{"type", "string"}, {"description", "Text to append"}}},
            {"folder", {{"type", "string"}, {"description", "Move to folder"}}},
            {"tags", {{"type", "string"}, {"description", "Replace tags"}}},
            {"pinned", {{"type", "boolean"}, {"description", "Pin or unpin"}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (id.empty()) {
                std::string title = args.value("title", "");
                if (title.empty()) return {false, "", "Provide id or title."};
                auto notes = db.list_notes("", title, "", 10, false);
                if (notes.empty()) return {false, "", "No notes found."};
                if (notes.size() > 1) {
                    nlohmann::json result = {{"message", "Multiple matches."}, {"matches", nlohmann::json::array()}};
                    for (auto& n : notes) result["matches"].push_back({{"title", n["title"]}, {"id", n["id"]}});
                    return {true, result.dump(), ""};
                }
                id = notes[0]["id"];
            }
            nlohmann::json update_data;
            if (args.contains("new_title")) update_data["title"] = args["new_title"];
            if (args.contains("folder")) update_data["folder"] = args["folder"];
            if (args.contains("tags")) update_data["tags"] = args["tags"];
            if (args.contains("pinned")) update_data["pinned"] = args["pinned"];
            if (args.contains("append")) {
                auto existing = db.get_note(id);
                if (existing.is_null()) return {false, "", "Note not found."};
                std::string current = existing.value("content", "");
                std::string addition = args["append"].get<std::string>();
                if (!current.empty() && current.back() != '\n') current += "\n";
                update_data["content"] = current + addition;
            } else if (args.contains("content")) {
                update_data["content"] = args["content"];
            }
            if (!db.update_note(id, update_data)) return {false, "", "Note not found or update failed."};
            auto note = db.get_note(id);
            return {true, strip_id(note).dump(), ""};
        });

    registry.register_tool(
        {"delete_note",
         "Permanently delete a note by ID or title.",
         {{"type", "object"},
          {"properties",
           {{"id", {{"type", "string"}, {"description", "Exact note ID"}}},
            {"title", {{"type", "string"}, {"description", "Title to find and delete"}}}}},
          {"required", nlohmann::json::array()}}},
        [](const nlohmann::json& args) -> ToolResult {
            auto& db = AgentDatabase::instance();
            std::string id = args.value("id", "");
            if (id.empty()) {
                std::string title = args.value("title", "");
                if (title.empty()) return {false, "", "Provide id or title."};
                auto notes = db.list_notes("", title, "", 10, false);
                if (notes.empty()) return {false, "", "No notes found."};
                if (notes.size() > 1) {
                    nlohmann::json result = {{"message", "Multiple matches."}, {"matches", nlohmann::json::array()}};
                    for (auto& n : notes) result["matches"].push_back({{"title", n["title"]}, {"id", n["id"]}});
                    return {true, result.dump(), ""};
                }
                id = notes[0]["id"];
            }
            if (!db.delete_note(id)) return {false, "", "Note not found."};
            return {true, R"({"deleted": true})", ""};
        });
}

} // namespace agent
} // namespace delta