#include "tool_task.h"
#include "memory_store.h"
#include "tool_registry.h"

namespace delta {
namespace agent {

namespace {
// Each request is served on its own thread, so the active run id is thread-local rather than
// global: two conversations running at once must not write to each other's scratchpad.
thread_local std::string g_run_id;
} // namespace

void set_active_run_id(const std::string& run_id) {
    g_run_id = run_id;
}

std::string active_run_id() {
    return g_run_id;
}

void register_task_tools() {
    auto& registry = ToolRegistry::instance();

    registry.register_tool(
        {"note_to_self",
         "Write down something you have worked out that you will need later in this same job: a value "
         "you read, a path you found, a decision you made, a dead end not to try again. Long "
         "conversations get trimmed, and anything you note here stays in front of you when the "
         "earlier messages are gone. Use remember instead for things that should outlive this job.",
         {{"type", "object"},
          {"properties",
           {{"note", {{"type", "string"}, {"description", "One short line, written so it makes sense on its own"}}}}},
          {"required", {"note"}}},
         ToolRisk::Safe,
         "task"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string run_id = active_run_id();
            if (run_id.empty())
                return {false, "", "No active run"};
            const std::string note = args.value("note", "");
            if (note.empty())
                return {false, "", "note is required"};
            auto& memory = MemoryStore::instance();
            memory.add_note(run_id, note);
            return {true, nlohmann::json{{"noted", true}, {"notes", memory.get_notes(run_id).size()}}.dump(), ""};
        });

    registry.register_tool({"load_tools",
                            "Get hold of a group of tools you do not have yet. The system prompt lists the groups that "
                            "are available but not loaded. Call this once with the group you need, then call the tools "
                            "in it as normal. Do not call it for tools you already have.",
                            {{"type", "object"},
                             {"properties",
                              {{"category",
                                {{"type", "string"},
                                 {"enum", {"notes", "files", "shell", "web"}},
                                 {"description", "The group of tools to load"}}}}},
                             {"required", {"category"}}},
                            ToolRisk::Safe,
                            "task"},
                           [](const nlohmann::json& args) -> ToolResult {
                               const std::string category = args.value("category", "");
                               std::string error;
                               if (!load_tool_category(category, error))
                                   return {false, "", error};
                               nlohmann::json names = nlohmann::json::array();
                               auto& reg = ToolRegistry::instance();
                               for (const auto& name : reg.get_tool_names()) {
                                   const auto* def = reg.get_definition(name);
                                   if (def && def->category == category)
                                       names.push_back(name);
                               }
                               return {true, nlohmann::json{{"loaded", category}, {"tools", names}}.dump(), ""};
                           });

    registry.register_tool(
        {"set_plan",
         "Write down a short plan before working through a request that needs several steps. "
         "Call this first for multi-step work, then call update_plan as you finish each step. "
         "The plan is your own working memory -- it keeps you on track when the conversation gets long. "
         "Skip it for anything you can finish in one or two tool calls.",
         {{"type", "object"},
          {"properties",
           {{"goal", {{"type", "string"}, {"description", "What the user actually wants, in one sentence"}}},
            {"steps",
             {{"type", "array"},
              {"items", {{"type", "string"}}},
              {"description", "The steps you intend to take, in order"}}}}},
          {"required", {"goal", "steps"}}},
         ToolRisk::Safe,
         "task"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string run_id = active_run_id();
            if (run_id.empty())
                return {false, "", "No active run"};
            nlohmann::json steps = nlohmann::json::array();
            if (args.contains("steps") && args["steps"].is_array()) {
                for (const auto& step : args["steps"]) {
                    steps.push_back(
                        {{"step", step.is_string() ? step.get<std::string>() : step.dump()}, {"status", "pending"}});
                }
            }
            MemoryStore::instance().set_plan(run_id, args.value("goal", ""), steps);
            return {true,
                    nlohmann::json{
                        {"plan_set", true}, {"steps", steps.size()}, {"next", steps.empty() ? "" : steps[0]["step"]}}
                        .dump(),
                    ""};
        });

    registry.register_tool(
        {"update_plan",
         "Mark a plan step done (or blocked) and see what is left. Call this after each step so you "
         "do not repeat work or lose track of what remains.",
         {{"type", "object"},
          {"properties",
           {{"step_index", {{"type", "integer"}, {"description", "0-based index of the step to update"}}},
            {"status",
             {{"type", "string"},
              {"enum", {"pending", "in_progress", "done", "blocked"}},
              {"description", "New status for that step"}}},
            {"note", {{"type", "string"}, {"description", "Optional short note about what happened"}}}}},
          {"required", {"step_index", "status"}}},
         ToolRisk::Safe,
         "task"},
        [](const nlohmann::json& args) -> ToolResult {
            const std::string run_id = active_run_id();
            if (run_id.empty())
                return {false, "", "No active run"};
            auto plan = MemoryStore::instance().get_plan(run_id);
            if (!plan.is_object() || !plan.contains("steps") || !plan["steps"].is_array())
                return {false, "", "No plan set. Call set_plan first."};

            auto steps = plan["steps"];
            const int idx = args.value("step_index", -1);
            if (idx < 0 || idx >= static_cast<int>(steps.size()))
                return {false, "", "step_index out of range"};
            steps[idx]["status"] = args.value("status", "done");
            if (args.contains("note"))
                steps[idx]["note"] = args["note"];
            MemoryStore::instance().set_plan(run_id, plan.value("goal", ""), steps);

            nlohmann::json remaining = nlohmann::json::array();
            for (const auto& step : steps) {
                const std::string status = step.value("status", "pending");
                if (status == "pending" || status == "in_progress")
                    remaining.push_back(step["step"]);
            }
            return {true,
                    nlohmann::json{{"updated", true}, {"remaining", remaining}, {"all_done", remaining.empty()}}.dump(),
                    ""};
        });
}

} // namespace agent
} // namespace delta
