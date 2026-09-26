#include "tool_registry.h"
#include "tool_calendar.h"
#include "tool_files.h"
#include "tool_memory.h"
#include "tool_notes.h"
#include "tool_shell.h"
#include "tool_task.h"
#include "tool_web.h"
#include <algorithm>
#include <map>

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

// True when `value` already has JSON type `expected`, or was converted to it in place.
static bool coerce_argument(nlohmann::json& value, const std::string& expected) {
    if (expected == "string") {
        if (value.is_string())
            return true;
        if (value.is_number() || value.is_boolean()) {
            value = value.dump();
            return true;
        }
        return false;
    }
    if (expected == "integer" || expected == "number") {
        if (expected == "integer" ? value.is_number_integer() : value.is_number())
            return true;
        if (!value.is_string())
            return false;
        const std::string text = value.get<std::string>();
        try {
            size_t used = 0;
            if (expected == "integer") {
                const long long n = std::stoll(text, &used);
                if (used != text.size())
                    return false;
                value = n;
            } else {
                const double d = std::stod(text, &used);
                if (used != text.size())
                    return false;
                value = d;
            }
            return true;
        } catch (...) {
            return false;
        }
    }
    if (expected == "boolean") {
        if (value.is_boolean())
            return true;
        if (value.is_string() && (value == "true" || value == "false")) {
            value = value == "true";
            return true;
        }
        return false;
    }
    if (expected == "array")
        return value.is_array();
    if (expected == "object")
        return value.is_object();
    return true;
}

std::string ToolRegistry::resolve_alias(const std::string& raw, nlohmann::json& arguments) const {
    if (definitions_.count(raw))
        return raw;
    std::string n = raw;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    static const std::map<std::string, std::string> kAliases = {
        {"create_meeting", "create_event"},  {"add_event", "create_event"},
        {"schedule_event", "create_event"},  {"create_appointment", "create_event"},
        {"set_meeting", "create_event"},     {"create_reminder", "create_event"},
        {"create_task", "create_event"},     {"add_task", "create_event"},
        {"create_todo", "create_event"},     {"get_events", "list_events"},
        {"show_events", "list_events"},      {"list_tasks", "list_events"},
        {"move_event", "update_event"},      {"reschedule_event", "update_event"},
        {"update_task", "update_event"},     {"remove_event", "delete_event"},
        {"delete_task", "delete_event"},     {"add_note", "create_note"},
        {"write_note", "create_note"},       {"get_notes", "list_notes"},
        {"show_notes", "list_notes"},        {"read_note", "get_note"},
        {"edit_note", "update_note"},        {"remove_note", "delete_note"},
        {"current_time", "get_current_time"}};
    if (definitions_.count(n))
        return n;
    auto it = kAliases.find(n);
    if (it == kAliases.end() || !definitions_.count(it->second))
        return raw;
    const bool task_alias = n == "create_task" || n == "add_task" || n == "create_todo" || n == "list_tasks";
    if (task_alias && arguments.is_object() && !arguments.contains("type"))
        arguments["type"] = "task";
    return it->second;
}

ToolResult ToolRegistry::execute(const std::string& name, const nlohmann::json& arguments) {
    auto it = handlers_.find(name);
    if (it == handlers_.end()) {
        return {false, "", "Unknown tool: " + name};
    }

    // Check the schema's own required list before running anything. Small models leave arguments
    // out, and a tool that then sees a default-constructed value does the wrong thing quietly; an
    // error naming the argument is something the model can actually act on.
    auto def = definitions_.find(name);
    if (def != definitions_.end()) {
        const auto& params = def->second.parameters;
        if (params.is_object() && params.contains("required") && params["required"].is_array()) {
            for (const auto& required : params["required"]) {
                if (!required.is_string())
                    continue;
                const std::string key = required.get<std::string>();
                if (!arguments.is_object() || !arguments.contains(key) || arguments[key].is_null())
                    return {false, "", name + " needs a '" + key + "' argument. Call it again with one."};
            }
        }
    }

    // Then the declared types. A value a small model quoted ("15", "true") is converted rather
    // than refused; anything else is refused with the expected type, which the model can fix.
    nlohmann::json checked = arguments;
    if (def != definitions_.end() && checked.is_object()) {
        const auto& params = def->second.parameters;
        if (params.is_object() && params.contains("properties") && params["properties"].is_object()) {
            const auto& props = params["properties"];
            for (auto& [key, value] : checked.items()) {
                if (!props.contains(key) || !props[key].is_object() || !props[key].contains("type") ||
                    !props[key]["type"].is_string() || value.is_null())
                    continue;
                const std::string expected = props[key]["type"].get<std::string>();
                if (!coerce_argument(value, expected))
                    return {false, "", name + ": '" + key + "' must be " + expected + ". Call it again with that."};
            }
        }
    }

    try {
        return it->second(checked);
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

namespace {

// Held back until the model asks. Chosen because they are powerful but occasional: a personal
// assistant reaches for the calendar every day and for the shell once a week.
const std::set<std::string>& deferred_categories() {
    static const std::set<std::string> deferred = {"notes", "files", "shell", "web"};
    return deferred;
}

// Per-run tool session. Thread-local for the same reason the active run id is (see tool_task.h):
// two conversations served at once must not change each other's tool set.
thread_local std::set<std::string> g_loaded_categories;
thread_local std::set<std::string> g_permitted_categories;

} // namespace

bool is_deferred_category(const std::string& category) {
    return deferred_categories().count(category) > 0;
}

std::string category_summary(const std::string& category) {
    if (category == "calendar")
        return "the user's events, tasks and reminders";
    if (category == "notes")
        return "read, write and organise the notes the user has written";
    if (category == "memory")
        return "what you remember about the user between conversations";
    if (category == "task")
        return "your plan and working notes for the job in hand";
    if (category == "files")
        return "read, write, list and delete files under the home directory";
    if (category == "shell")
        return "run a command on this machine and read its output";
    if (category == "web")
        return "fetch a web page, or open one in the user's browser";
    return "";
}

void begin_tool_session(const std::set<std::string>& permitted) {
    g_loaded_categories.clear();
    g_permitted_categories = permitted;
}

bool load_tool_category(const std::string& category, std::string& error) {
    if (category.empty()) {
        error = "category is required";
        return false;
    }
    if (!is_deferred_category(category)) {
        if (ToolRegistry::instance().get_categories().count(category) > 0) {
            error = "The " + category + " tools are already available; just call them.";
        } else {
            error = "There is no tool group called " + category + ".";
        }
        return false;
    }
    if (!g_permitted_categories.empty() && g_permitted_categories.count(category) == 0) {
        error = "The user has turned the " + category + " tools off for this conversation.";
        return false;
    }
    g_loaded_categories.insert(category);
    return true;
}

const std::set<std::string>& loaded_tool_categories() {
    return g_loaded_categories;
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
