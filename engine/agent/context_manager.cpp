#include "context_manager.h"
#include "llm_client.h"
#include <algorithm>
#include <iostream>

namespace delta {
namespace agent {

namespace {
// Head/tail budget for a single tool result before it is truncated. Shell and file tools can
// otherwise return hundreds of kilobytes and evict the whole conversation on their own.
constexpr size_t kMaxToolResultChars = 6000;
// Leave headroom for the chat template's own scaffolding and any tokenizer disagreement.
constexpr int kSafetyMargin = 320;
// Per-message overhead in tokens for role markers and template separators.
constexpr int kMessageOverhead = 5;
// Below this many dropped messages a summary is not worth a model call.
constexpr int kMinDroppedForSummary = 4;

bool is_tool_message(const nlohmann::json& msg) {
    return msg.is_object() && msg.value("role", "") == "tool";
}

bool carries_tool_calls(const nlohmann::json& msg) {
    return msg.is_object() && msg.contains("tool_calls") && msg["tool_calls"].is_array() && !msg["tool_calls"].empty();
}
} // namespace

ContextManager::ContextManager(int n_ctx, int reserve_output_tokens)
    : n_ctx_(n_ctx > 0 ? n_ctx : 4096), reserve_output_(reserve_output_tokens > 0 ? reserve_output_tokens : 512) {
    budget_ = n_ctx_ - reserve_output_ - kSafetyMargin;
    if (budget_ < 512)
        budget_ = 512; // a tiny window still has to carry the system prompt and one turn
}

int ContextManager::estimate_tokens(const std::string& text) {
    // ~3.6 characters per token: deliberately pessimistic so the estimate never under-counts badly.
    return static_cast<int>(text.size() * 10 / 36) + 1;
}

std::string ContextManager::truncate_middle(const std::string& text, size_t max_chars) {
    if (text.size() <= max_chars)
        return text;
    size_t head = max_chars * 3 / 5;
    size_t tail = max_chars - head;
    const size_t omitted = text.size() - head - tail;
    return text.substr(0, head) + "\n\n... [" + std::to_string(omitted) + " characters truncated by Delta] ...\n\n" +
           text.substr(text.size() - tail);
}

int ContextManager::token_cost(const nlohmann::json& message) const {
    std::string text = message_text(message);
    if (carries_tool_calls(message))
        text += message["tool_calls"].dump();
    int tokens = -1;
    if (count_ && !text.empty())
        tokens = count_(text);
    if (tokens < 0)
        tokens = estimate_tokens(text);
    return tokens + kMessageOverhead;
}

nlohmann::json ContextManager::build(const std::string& system_prompt, const nlohmann::json& history) {
    stats_ = ContextStats{};
    stats_.budget_tokens = budget_;

    // Any system message the client sent belongs with the system prompt, not in the evictable
    // history, so a long conversation can never drop the user's persona settings.
    std::string full_system = system_prompt;
    nlohmann::json turns = nlohmann::json::array();
    if (history.is_array()) {
        for (const auto& msg : history) {
            if (!msg.is_object())
                continue;
            if (msg.value("role", "") == "system") {
                std::string extra = message_text(msg);
                if (!extra.empty())
                    full_system += (full_system.empty() ? "" : "\n\n") + extra;
                continue;
            }
            nlohmann::json copy = msg;
            copy["content"] = message_text(msg);
            if (is_tool_message(copy)) {
                std::string content = copy["content"].get<std::string>();
                if (content.size() > kMaxToolResultChars) {
                    copy["content"] = truncate_middle(content, kMaxToolResultChars);
                    stats_.truncated_results++;
                }
            }
            turns.push_back(std::move(copy));
        }
    }

    nlohmann::json system_msg = {{"role", "system"}, {"content", full_system}};
    const int system_cost = token_cost(system_msg);
    int remaining = budget_ - system_cost;
    if (remaining < 256)
        remaining = 256; // an enormous system prompt still leaves room for the live turn

    // Group into atomic blocks so an assistant's tool_calls never gets separated from its results.
    std::vector<std::pair<size_t, size_t>> blocks; // [begin, end)
    for (size_t i = 0; i < turns.size();) {
        size_t end = i + 1;
        if (carries_tool_calls(turns[i])) {
            while (end < turns.size() && is_tool_message(turns[end]))
                end++;
        }
        blocks.emplace_back(i, end);
        i = end;
    }

    auto block_cost = [&](const std::pair<size_t, size_t>& b) {
        int cost = 0;
        for (size_t i = b.first; i < b.second; i++)
            cost += token_cost(turns[i]);
        return cost;
    };

    size_t keep_from = blocks.size();
    int used = 0;
    for (size_t n = blocks.size(); n-- > 0;) {
        const int cost = block_cost(blocks[n]);
        if (used + cost > remaining && keep_from != blocks.size())
            break;
        if (used + cost > remaining && keep_from == blocks.size()) {
            // The most recent block alone overflows the window. Keep it anyway -- dropping the live
            // turn would make the request meaningless -- and let the per-result truncation above plus
            // the server's own handling absorb it.
            keep_from = n;
            used += cost;
            break;
        }
        used += cost;
        keep_from = n;
    }

    // Several chat templates (Gemma's among them) reject anything but a single leading system
    // message followed by strictly alternating user/assistant turns. So the summary of what was
    // dropped is appended to the system prompt rather than added as a second system message, and
    // the kept window is advanced to start on a user turn.
    std::string summary_note;
    if (keep_from > 0) {
        nlohmann::json dropped = nlohmann::json::array();
        for (size_t n = 0; n < keep_from; n++) {
            for (size_t i = blocks[n].first; i < blocks[n].second; i++)
                dropped.push_back(turns[i]);
        }
        stats_.dropped_messages = static_cast<int>(dropped.size());

        std::string summary;
        if (summarize_ && stats_.dropped_messages >= kMinDroppedForSummary)
            summary = summarize_(dropped);
        if (!summary.empty()) {
            stats_.summarized = true;
            summary_note = "\n\nSummary of the earlier part of this conversation, which no longer fits in "
                           "context:\n" +
                           summary;
        } else if (stats_.dropped_messages > 0) {
            summary_note = "\n\n[" + std::to_string(stats_.dropped_messages) +
                           " earlier messages were dropped to fit the context window. Ask the user if you "
                           "need something from them.]";
        }
    }

    // Skip leading blocks until the window opens on a user turn. Whole blocks are skipped so a
    // tool result never loses the assistant turn that asked for it.
    while (keep_from < blocks.size() && turns[blocks[keep_from].first].value("role", "") != "user") {
        for (size_t i = blocks[keep_from].first; i < blocks[keep_from].second; i++) {
            (void)i;
            stats_.dropped_messages++;
        }
        keep_from++;
    }

    if (!summary_note.empty())
        system_msg["content"] = system_msg["content"].get<std::string>() + summary_note;

    nlohmann::json out = nlohmann::json::array();
    out.push_back(system_msg);
    for (size_t n = keep_from; n < blocks.size(); n++) {
        for (size_t i = blocks[n].first; i < blocks[n].second; i++)
            out.push_back(turns[i]);
    }

    stats_.used_tokens = system_cost + used;
    return out;
}

} // namespace agent
} // namespace delta
