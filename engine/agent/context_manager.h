#ifndef DELTA_CONTEXT_MANAGER_H
#define DELTA_CONTEXT_MANAGER_H

#include <functional>
#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

// Produces a summary of the messages it is handed. Returns "" if it cannot.
using Summarizer = std::function<std::string(const nlohmann::json& dropped_messages)>;
// Exact token count for a string; return -1 when unavailable so the estimator is used instead.
using TokenCounter = std::function<int(const std::string&)>;

struct ContextStats {
    int budget_tokens = 0;
    int used_tokens = 0;
    int dropped_messages = 0;
    int truncated_results = 0;
    bool summarized = false;
};

// Decides what actually gets sent to the model. Replaces the old "keep the last 6 messages" rule
// with a token budget derived from the model's real context window.
//
// Invariants it maintains:
//   * the system prompt is never dropped;
//   * an assistant message carrying tool_calls always travels with its tool results, so the
//     transcript never contains an orphan tool message (llama-server rejects those);
//   * an oversized single tool result is truncated head+tail rather than evicting real turns.
class ContextManager {
  public:
    ContextManager(int n_ctx, int reserve_output_tokens);

    void set_summarizer(Summarizer fn) { summarize_ = std::move(fn); }
    void set_token_counter(TokenCounter fn) { count_ = std::move(fn); }

    // Builds the message array to send: system prompt first, then as much recent history as fits.
    nlohmann::json build(const std::string& system_prompt, const nlohmann::json& history);

    const ContextStats& stats() const { return stats_; }

    int budget_tokens() const { return budget_; }
    // Characters-per-token estimate used when the tokenizer endpoint is unavailable.
    static int estimate_tokens(const std::string& text);
    int token_cost(const nlohmann::json& message) const;

    // Shortens an oversized tool result, keeping the head and tail. Public so tools that already
    // know their output is huge can pre-trim.
    static std::string truncate_middle(const std::string& text, size_t max_chars);

  private:
    int n_ctx_;
    int reserve_output_;
    int budget_;
    Summarizer summarize_;
    TokenCounter count_;
    ContextStats stats_;

    // The last summary produced and what it covered, so the next build() of the same run can
    // reuse it instead of paying another model round-trip for the same dropped messages.
    std::string cached_summary_;
    size_t cached_summary_count_ = 0; // dropped messages the summary covers
    size_t cached_summary_hash_ = 0;  // of those messages, to notice a different history
};

} // namespace agent
} // namespace delta

#endif // DELTA_CONTEXT_MANAGER_H
