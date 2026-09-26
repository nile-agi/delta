#include "tool_memory.h"
#include "memory_store.h"
#include "tool_registry.h"
#include "tool_task.h"
#include <algorithm>

namespace delta {
namespace agent {

void register_memory_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"remember",
         "Save something about the user that should outlive this exchange. Two kinds: scope='general' "
         "for how they like things done, which you will recall in any conversation, and the default "
         "scope='this_task' for details that only make sense within the job you are on now. Prefer "
         "the default -- a detail from one job showing up in an unrelated one is worse than not "
         "recalling it. Use note_to_self for working notes, and do not save anything already stored "
         "as an event or a note.",
         {{"type", "object"},
          {"properties",
           {{"content", {{"type", "string"}, {"description", "The fact, written so it makes sense on its own"}}},
            {"kind",
             {{"type", "string"},
              {"enum", {"fact", "preference", "project", "reference"}},
              {"description", "fact = about the user or world; preference = how they want things done; "
                              "project = ongoing work; reference = a pointer to something external"}}},
            {"tags", {{"type", "string"}, {"description", "Comma-separated tags for retrieval"}}},
            {"scope",
             {{"type", "string"},
              {"enum", {"this_task", "general"}},
              {"description", "this_task (default) keeps it to the job you are on now; general is for a "
                              "standing preference or a way of doing things that applies to any conversation"}}},
            {"importance",
             {{"type", "integer"}, {"description", "1 = recall when relevant (default), 3 = always keep in mind"}}}}},
          {"required", {"content"}}},
         ToolRisk::Caution,
         "memory"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string content = args.value("content", "");
            if (content.empty())
                return {false, "", "content is required"};
            // Scoped by default: a detail from one job has no business surfacing in an unrelated one.
            const std::string scope = args.value("scope", "this_task");
            const std::string conversation = scope == "general" ? std::string() : active_memory_scope();
            if (scope != "general" && conversation.empty()) {
                // Saving it now would make it visible in every conversation, which is the one thing
                // this scope exists to prevent.
                return {false, "",
                        "There is no conversation to attach this to. Pass scope='general' if it "
                        "really applies everywhere."};
            }
            const std::string id =
                MemoryStore::instance().remember(content, args.value("kind", "fact"), args.value("tags", ""),
                                                 args.value("importance", 1), "chat", conversation);
            if (id.empty())
                return {false, "", "Could not save that memory"};
            return {true, nlohmann::json{{"saved", true}, {"id", id}}.dump(), ""};
        });

    registry.register_tool(
        {"recall",
         "Search what you remember about the user. The most relevant memories are already in the context "
         "above, so call this only when you need something that is not there. Use it before "
         "asking the user something they may have told you in an earlier conversation.",
         {{"type", "object"},
          {"properties",
           {{"query",
             {{"type", "string"}, {"description", "What to look for. Leave empty for the most important memories."}}},
            {"limit", {{"type", "integer"}, {"description", "Max results (default 5)"}}}}},
          {"required", nlohmann::json::array()}},
         ToolRisk::Safe,
         "memory"},
        [](const nlohmann::json& args) -> ToolResult {
            const int limit = std::max(1, std::min(20, args.value("limit", 5)));
            auto results = MemoryStore::instance().search(args.value("query", ""), limit, active_memory_scope());
            nlohmann::json out = nlohmann::json::array();
            for (const auto& m : results) {
                MemoryStore::instance().touch(m.id);
                out.push_back({{"id", m.id}, {"kind", m.kind}, {"content", m.content}, {"tags", m.tags}});
            }
            return {true, nlohmann::json{{"count", out.size()}, {"memories", out}}.dump(), ""};
        });

    registry.register_tool({"forget",
                            "Delete a memory by id, for when something you saved is wrong or no longer true. "
                            "Find the id with recall first.",
                            {{"type", "object"},
                             {"properties", {{"id", {{"type", "string"}, {"description", "Memory id from recall"}}}}},
                             {"required", {"id"}}},
                            ToolRisk::Destructive,
                            "memory"},
                           [](const nlohmann::json& args) -> ToolResult {
                               const std::string id = args.value("id", "");
                               if (id.empty())
                                   return {false, "", "id is required"};
                               if (!MemoryStore::instance().forget(id))
                                   return {false, "", "No memory with that id"};
                               return {true, nlohmann::json{{"forgotten", true}, {"id", id}}.dump(), ""};
                           });
}

} // namespace agent
} // namespace delta
