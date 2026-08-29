#include "tool_registry.h"
#include <iostream>

namespace delta {
namespace agent {

// Forward declarations for tool registration functions defined in other files
void register_calendar_tools();
void register_note_tools();

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

bool ToolRegistry::validate_arguments(const std::string& tool_name, const nlohmann::json& arguments, std::string& error_message) {
    auto it = definitions_.find(tool_name);
    if (it == definitions_.end()) {
        error_message = "Unknown tool: " + tool_name;
        return false;
    }
    const auto& def = it->second;
    const auto& params = def.parameters;

    // 1. Check required fields
    if (params.contains("required") && params["required"].is_array()) {
        for (const auto& req : params["required"]) {
            if (!req.is_string()) continue;
            std::string req_key = req.get<std::string>();
            if (!arguments.contains(req_key)) {
                error_message = "Missing required argument: '" + req_key + "'";
                return false;
            }
        }
    }

    // 2. Check types of provided fields
    if (params.contains("properties") && params["properties"].is_object()) {
        for (auto& [key, value] : arguments.items()) {
            if (params["properties"].contains(key) && params["properties"][key].contains("type")) {
                std::string expected_type = params["properties"][key]["type"].get<std::string>();
                bool type_match = false;
                
                if (expected_type == "string" && value.is_string()) type_match = true;
                else if (expected_type == "integer" && value.is_number_integer()) type_match = true;
                else if (expected_type == "number" && value.is_number()) type_match = true;
                else if (expected_type == "boolean" && value.is_boolean()) type_match = true;
                else if (expected_type == "array" && value.is_array()) type_match = true;
                else if (expected_type == "object" && value.is_object()) type_match = true;
                
                if (!type_match) {
                    error_message = "Invalid type for argument '" + key + "': expected " + expected_type;
                    return false;
                }
            }
        }
    }
    return true;
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

// IMPLEMENTATION: Register all available tools
void register_all_tools() {
    register_calendar_tools();
    register_note_tools();
}

} // namespace agent
} // namespace delta