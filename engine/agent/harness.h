#ifndef DELTA_AGENT_HARNESS_H
#define DELTA_AGENT_HARNESS_H

#include <functional>
#include <set>
#include <string>
#include "context_manager.h"
#include "json.hpp"
#include "llm_client.h"
#include "policy.h"

namespace delta {
namespace agent {

enum class EventType {
    Content,          // a chunk of assistant text
    ToolStart,        // {name, arguments, risk}
    ToolResult,       // {name, success, summary, error}
    ApprovalRequired, // {id, name, arguments, risk, reason}
    ApprovalResolved, // {id, decision}
    Compaction,       // {dropped, summarized, used_tokens, budget_tokens}
    Status,           // {message} -- iteration and budget notices
    Error,            // {message}
};

struct HarnessEvent {
    EventType type;
    nlohmann::json data;
};

// Returns false when the client has gone away, which aborts the run.
using EventSink = std::function<bool(const HarnessEvent&)>;

struct RunOptions {
    int max_iterations = 25;
    int max_tokens = 2048;
    int n_ctx = 0;                // 0 = ask the server
    int wall_clock_seconds = 900; // whole-run budget
    int approval_timeout_seconds = 180;
    bool tools_enabled = true;
    // Empty means every registered category.
    std::set<std::string> enabled_categories;
    Policy::Config policy;
};

struct RunResult {
    bool success = false;
    std::string content; // the final assistant message
    int iterations = 0;
    int tool_calls = 0;
    std::string error;
    std::string stop_reason; // stop | max_iterations | time_budget | client_aborted | error
    nlohmann::json executed_tools = nlohmann::json::array();
    size_t streamed_chars = 0;
    bool client_aborted = false;
};

// The agent loop. One instance serves one run.
//
// Each iteration: build a context that fits, stream the model, execute whatever tools it asked
// for (subject to the policy gate), feed every result back, and go again until the model stops
// or a budget runs out. Unlike the loop it replaces, nothing here inspects the user's wording to
// guess intent, and no tool result is ever hidden from the model.
class Harness {
  public:
    Harness(const std::string& llama_server_url, const std::string& model_name, bool supports_tools);

    void set_options(const RunOptions& options);
    const RunOptions& options() const { return options_; }

    RunResult run(const nlohmann::json& messages, const EventSink& sink);

  private:
    std::string build_system_prompt(const nlohmann::json& messages) const;
    nlohmann::json active_tools() const;
    std::string summarize(const nlohmann::json& dropped);

    LlmClient client_;
    bool supports_tools_;
    RunOptions options_;
    std::string run_id_;
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_HARNESS_H
