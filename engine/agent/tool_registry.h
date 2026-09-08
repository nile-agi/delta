#ifndef DELTA_TOOL_REGISTRY_H
#define DELTA_TOOL_REGISTRY_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "json.hpp"

namespace delta {
namespace agent {

// How much damage a tool can do if the model gets it wrong. Drives the approval gate in policy.h.
enum class ToolRisk {
    Safe = 0,        // read-only, no side effects outside the process
    Caution = 1,     // creates or modifies user data, recoverable
    Destructive = 2, // deletes data, runs arbitrary commands, touches the outside world
};

const char* risk_name(ToolRisk risk);

struct ToolDefinition {
    std::string name;
    std::string description;
    nlohmann::json parameters; // JSON Schema
    ToolRisk risk = ToolRisk::Safe;
    std::string category = "general"; // calendar | notes | shell | files | web | memory | task

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
    // Attach risk/category to an already-registered tool. Lets tools whose definitions live in
    // other translation units stay untouched while the harness still knows how to gate them.
    void set_tool_metadata(const std::string& name, ToolRisk risk, const std::string& category);

    // All tools, OpenAI schema form.
    nlohmann::json get_tools_array() const;
    // Only tools whose category is in `categories`. An empty set means "everything".
    nlohmann::json get_tools_array(const std::set<std::string>& categories) const;

    ToolResult execute(const std::string& name, const nlohmann::json& arguments);
    bool has_tool(const std::string& name) const;
    // Returns nullptr when the tool is not registered.
    const ToolDefinition* get_definition(const std::string& name) const;
    std::vector<std::string> get_tool_names() const;
    std::set<std::string> get_categories() const;

  private:
    ToolRegistry() = default;
    std::map<std::string, ToolDefinition> definitions_;
    std::map<std::string, ToolHandler> handlers_;
};

// Registers every built-in tool. Safe to call more than once.
void register_all_tools();

// --- deferred tool loading ---
//
// A 2-8B model given 20+ schemas on every turn spends a third of its window on tools it will never
// call. The categories below are held back: the model is told they exist, in one line each, and
// calls load_tools to get their schemas. Everything else is offered from the start.

// True for a category whose schemas wait until the model asks for them.
bool is_deferred_category(const std::string& category);

// One line saying what a category can do, for the manifest the model always sees.
std::string category_summary(const std::string& category);

// Starts a run's tool session: nothing deferred is loaded, and `permitted` (empty means all) caps
// what load_tools may ever reach. Thread-local, like the active run id, because each request runs
// on its own thread and must not change another run's tool set.
void begin_tool_session(const std::set<std::string>& permitted);

// Adds a deferred category to this run. Returns false and sets `error` when the category is
// unknown, not deferred, or not permitted.
bool load_tool_category(const std::string& category, std::string& error);

// The deferred categories this run has loaded so far.
const std::set<std::string>& loaded_tool_categories();

} // namespace agent
} // namespace delta

#endif // DELTA_TOOL_REGISTRY_H
