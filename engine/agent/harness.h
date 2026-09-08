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
    Reasoning,        // {text} -- a chunk of the model's thinking, when it streams it separately
    ToolStart,        // {call_id, name, arguments, risk}
    ToolResult,       // {call_id, name, success, summary, error}
    ApprovalRequired, // {id, call_id, name, arguments, risk, description}
    ApprovalResolved, // {id, decision}
    Compaction,       // {dropped, summarized, truncated_results, used_tokens, tool_tokens, budget_tokens}
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
    // Sampling. The defaults are deliberately not greedy: at temperature 0 small models fall into
    // repetition loops, which Qwen3's own documentation warns about. Negative means "server default".
    double temperature = 0.6;
    double top_p = 0.95;
    // Passed to the chat template. Thinking models route their reply through reasoning_content when
    // this is on; turning it off is how you get tool calls out of Gemma 4 reliably.
    bool enable_thinking = false;
    // Empty means every registered category.
    std::set<std::string> enabled_categories;
    Policy::Config policy;
    // Keys the plan scratchpad. Empty means a fresh scratchpad for this run only; a caller that
    // passes the same id on every turn of a conversation lets the plan carry over between turns.
    std::string scratchpad_id;
    // Polled while the model is generating and before each tool, so a caller can stop a run
    // (Ctrl-C, a closed connection) even when no event is flowing. Empty means never.
    std::function<bool()> abort_requested;
};

struct RunResult {
    bool success = false;
    std::string content; // the final assistant message
    int iterations = 0;
    int tool_calls = 0;
    std::string error;
    std::string stop_reason; // stop | max_iterations | time_budget | client_aborted | error
    nlohmann::json executed_tools = nlohmann::json::array();
    // Every message this run appended after the caller's history: assistant turns with their
    // tool_calls, the tool results, and the final reply. Append it to the stored conversation so
    // the next turn can see what the tools did.
    nlohmann::json transcript_delta = nlohmann::json::array();
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
    // The one-line summary of every tool group the model could load but has not, so it always
    // knows the full extent of what it can do without paying for the schemas.
    std::string deferred_manifest() const;
    std::string summarize(const nlohmann::json& dropped);
    // The key the plan scratchpad lives under: the caller's conversation id when given, else this run.
    std::string scratchpad_key() const { return options_.scratchpad_id.empty() ? run_id_ : options_.scratchpad_id; }

    LlmClient client_;
    bool supports_tools_;
    RunOptions options_;
    std::string run_id_;

    // Memory and calendar context for the current run, built once: the user's last message does
    // not change between iterations, so neither does what is worth recalling for it.
    // How many iterations are left, so the prompt can warn a model that is nearly out of room.
    // -1 before a run starts.
    int steps_remaining_ = -1;

    mutable std::string context_cache_;
    mutable std::string context_cache_key_;
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_HARNESS_H
