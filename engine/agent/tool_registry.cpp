#include "tool_registry.h"
#include "tool_calendar.h"
#include "tool_files.h"
#include "tool_memory.h"
#include "tool_notes.h"
#include "tool_shell.h"
#include "tool_task.h"
#include "tool_web.h"

namespace delta {
namespace agent {

const char* risk_name(ToolRisk risk) {
    switch (risk) {
    case ToolRisk::Safe:
        return "safe";
    case ToolRisk::Caution:
        return "caution";
    case ToolRisk::Destructive:
        return "destructive";
    }
    return "safe";
}

nlohmann::json ToolDefinition::to_openai_schema() const {
    return {{"type", "function"},
            {"function", {{"name", name}, {"description", description}, {"parameters", parameters}}}};
}

ToolRegistry& ToolRegistry::instance() {
    static ToolRegistry registry;
    return registry;
}

void ToolRegistry::register_tool(const ToolDefinition& def, ToolHandler handler) {
    definitions_[def.name] = def;
    handlers_[def.name] = std::move(handler);
}

void ToolRegistry::set_tool_metadata(const std::string& name, ToolRisk risk, const std::string& category) {
    auto it = definitions_.find(name);
    if (it == definitions_.end())
        return;
    it->second.risk = risk;
    it->second.category = category;
}

nlohmann::json ToolRegistry::get_tools_array() const {
    return get_tools_array({});
}

nlohmann::json ToolRegistry::get_tools_array(const std::set<std::string>& categories) const {
    nlohmann::json tools = nlohmann::json::array();
    for (const auto& [name, def] : definitions_) {
        (void)name;
        if (!categories.empty() && categories.count(def.category) == 0)
            continue;
        tools.push_back(def.to_openai_schema());
    }
    return tools;
}

ToolResult ToolRegistry::execute(const std::string& name, const nlohmann::json& arguments) {
    auto it = handlers_.find(name);
    if (it == handlers_.end()) {
        return {false, "", "Unknown tool: " + name};
    }
    try {
        return it->second(arguments);
    } catch (const std::exception& e) {
        return {false, "", std::string("Tool execution error: ") + e.what()};
    }
}

bool ToolRegistry::has_tool(const std::string& name) const {
    return definitions_.count(name) > 0;
}

const ToolDefinition* ToolRegistry::get_definition(const std::string& name) const {
    auto it = definitions_.find(name);
    return it == definitions_.end() ? nullptr : &it->second;
}

std::vector<std::string> ToolRegistry::get_tool_names() const {
    std::vector<std::string> names;
    for (const auto& [name, def] : definitions_) {
        (void)def;
        names.push_back(name);
    }
    return names;
}

std::set<std::string> ToolRegistry::get_categories() const {
    std::set<std::string> cats;
    for (const auto& [name, def] : definitions_) {
        (void)name;
        cats.insert(def.category);
    }
    return cats;
}

void register_all_tools() {
    static bool done = false;
    if (done)
        return;
    done = true;
    register_calendar_tools();
    register_note_tools();

    // The calendar and notes tools predate the risk model; tag them here so their definitions
    // stay as they are.
    auto& reg = ToolRegistry::instance();
    reg.set_tool_metadata("get_current_time", ToolRisk::Safe, "calendar");
    reg.set_tool_metadata("list_events", ToolRisk::Safe, "calendar");
    reg.set_tool_metadata("create_event", ToolRisk::Caution, "calendar");
    reg.set_tool_metadata("update_event", ToolRisk::Caution, "calendar");
    reg.set_tool_metadata("delete_event", ToolRisk::Destructive, "calendar");
    reg.set_tool_metadata("list_notes", ToolRisk::Safe, "notes");
    reg.set_tool_metadata("get_note", ToolRisk::Safe, "notes");
    reg.set_tool_metadata("create_note", ToolRisk::Caution, "notes");
    reg.set_tool_metadata("update_note", ToolRisk::Caution, "notes");
    reg.set_tool_metadata("delete_note", ToolRisk::Destructive, "notes");

    register_memory_tools();
    register_task_tools();
    register_file_tools();
    register_shell_tools();
    register_web_tools();
}

} // namespace agent
} // namespace delta
