#include "tool_registry.h"
#include "tool_calendar.h"
#include <iostream>

namespace delta {
namespace agent {

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

nlohmann::json ToolRegistry::get_tools_array() const {
    nlohmann::json tools = nlohmann::json::array();
    for (const auto& [name, def] : definitions_) {
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

std::vector<std::string> ToolRegistry::get_tool_names() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : definitions_) {
        names.push_back(name);
    }
    return names;
}

void register_all_tools() {
    register_calendar_tools();
}

} // namespace agent
} // namespace delta
