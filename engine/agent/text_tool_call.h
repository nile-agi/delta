#ifndef DELTA_TEXT_TOOL_CALL_H
#define DELTA_TEXT_TOOL_CALL_H

#include <functional>
#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

// Small models often write a call as text, `create_event(title="Standup", start_time="10:00")`,
// instead of emitting tool_calls. Finds the first `name(...)` in `content` for which `resolve`
// returns a non-empty tool name, and parses its arguments (key=value pairs or one JSON object).
// `resolve` receives the name as written plus the parsed arguments, and may add to them.
bool recover_text_tool_call(const std::string& content,
                            const std::function<std::string(const std::string&, nlohmann::json&)>& resolve,
                            std::string& tool_name, nlohmann::json& arguments);

// `key="value", n=3, flag=true` into a JSON object. Unparseable tails are dropped.
nlohmann::json parse_kwargs(const std::string& text);

} // namespace agent
} // namespace delta

#endif // DELTA_TEXT_TOOL_CALL_H
