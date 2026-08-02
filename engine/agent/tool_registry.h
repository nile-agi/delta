#ifndef DELTA_TOOL_REGISTRY_H
#define DELTA_TOOL_REGISTRY_H

#include <functional>
#include <map>
#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

struct ToolDefinition {
    std::string name;
    std::string description;
    nlohmann::json parameters; // JSON Schema

    nlohmann::json to_openai_schema() const;
};

struct ToolResult {
    bool success;
    std::string content; // JSON string returned to LLM
    std::string error_message;
};

using ToolHandler = std::function<ToolResult(const nlohmann::json& arguments)>;

class ToolRegistry {
  public:
    static ToolRegistry& instance();

    void register_tool(const ToolDefinition& def, ToolHandler handler);
    nlohmann::json get_tools_array() const;
    ToolResult execute(const std::string& name, const nlohmann::json& arguments);
    bool has_tool(const std::string& name) const;
    std::vector<std::string> get_tool_names() const;

  private:
    ToolRegistry() = default;
    std::map<std::string, ToolDefinition> definitions_;
    std::map<std::string, ToolHandler> handlers_;
};

void register_all_tools();

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_REGISTRY_H
