#include "context_manager.h"
#include "llm_client.h"
#include <algorithm>
#include <functional>
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
// Room set aside for the summary of what was dropped. It is appended to the system prompt after
// packing, so without a reservation it is spent on top of the budget rather than out of it.
constexpr int kSummaryAllowance = 400;

bool is_tool_message(const nlohmann::json& msg) {
    return msg.is_object() && msg.value("role", "") == "tool";
}

bool carries_tool_calls(const nlohmann::json& msg) {
    return msg.is_object() && msg.contains("tool_calls") && msg["tool_calls"].is_array() && !msg["tool_calls"].empty();
}
} // namespace

ContextManager::ContextManager(int n_ctx, int reserve_output_tokens)
    : n_ctx_(n_ctx > 0 ? n_ctx : 4096), reserve_output_(reserve_output_tokens > 0 ? reserve_output_tokens : 512) {
    recompute_budget();
}

void ContextManager::recompute_budget() {
    budget_ = n_ctx_ - reserve_output_ - kSafetyMargin - tool_overhead_;
    if (budget_ < 512)
        budget_ = 512; // a tiny window still has to carry the system prompt and one turn
}

void ContextManager::set_reserve_output(int tokens) {
    reserve_output_ = tokens > 0 ? tokens : 512;
    recompute_budget();
}

void ContextManager::set_tool_overhead(int tokens) {
    tool_overhead_ = tokens > 0 ? tokens : 0;
    recompute_budget();
}

int ContextManager::estimate_tokens(const std::string& text) {
    // ~3.6 characters per token: deliberately pessimistic so the estimate never under-counts badly.
    return static_cast<int>(text.size() * 10 / 36) + 1;
}

size_t ContextManager::utf8_floor(const std::string& text, size_t offset) {
    if (offset >= text.size())
        return text.size();
    // Continuation bytes are 10xxxxxx; step back until we are on a lead byte.
    while (offset > 0 && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
        offset--;
    return offset;
}

size_t ContextManager::utf8_ceil(const std::string& text, size_t offset) {
    while (offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0) == 0x80)
        offset++;
    return offset;
}

std::string ContextManager::truncate_middle(const std::string& text, size_t max_chars) {
    if (text.size() <= max_chars)
        return text;
    size_t head = utf8_floor(text, max_chars * 3 / 5);
    size_t tail_start = utf8_ceil(text, text.size() - (max_chars - (max_chars * 3 / 5)));
    if (tail_start < head)
        tail_start = head;
    const size_t omitted = tail_start - head;
    return text.substr(0, head) + "\n\n... [" + std::to_string(omitted) + " characters truncated by Delta] ...\n\n" +
           text.substr(tail_start);
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
    stats_.tool_tokens = tool_overhead_;

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
            // Keep attachments intact; only flatten content that is really just text. The token
            // cost still comes from the text, which is the best estimate available here.
            bool attachment = false;
            if (msg.contains("content") && msg["content"].is_array()) {
                for (const auto& part : msg["content"]) {
                    if (part.is_object() && part.contains("type") && part["type"].is_string() &&
                        part["type"].get<std::string>() != "text")
                        attachment = true;
                }
            }
            if (!attachment)
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
    // Hold back room for the summary before deciding what fits, so appending it later cannot
    // push the request over the window.
    const bool summary_possible = static_cast<bool>(summarize_);
    if (summary_possible)
        remaining -= kSummaryAllowance;
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
        if (summarize_ && stats_.dropped_messages >= kMinDroppedForSummary) {
            // Within one run the dropped prefix only ever grows by a message or two per
            // iteration, so a summary that already covers all but a few of them is still good.
            auto hash_prefix = [&](size_t count) {
                std::string text;
                for (size_t i = 0; i < count && i < dropped.size(); i++)
                    text += message_text(dropped[i]) + '\x1f';
                return std::hash<std::string>{}(text);
            };
            // Messages dropped since the cached summary was made are appended to it verbatim
            // (shortened) until enough pile up to be worth folding into a fresh summary.
            constexpr size_t kMaxAppendedToSummary = 2 * kMinDroppedForSummary;
            const bool reusable = !cached_summary_.empty() && dropped.size() >= cached_summary_count_ &&
                                  dropped.size() - cached_summary_count_ < kMaxAppendedToSummary &&
                                  hash_prefix(cached_summary_count_) == cached_summary_hash_;
            if (reusable) {
                summary = cached_summary_;
                for (size_t i = cached_summary_count_; i < dropped.size(); i++) {
                    std::string text = message_text(dropped[i]);
                    if (text.empty())
                        continue;
                    if (text.size() > 200)
                        text = text.substr(0, 200) + "...";
                    summary += "\n" + dropped[i].value("role", "") + ": " + text;
                }
            } else {
                summary = summarize_(dropped);
                if (!summary.empty()) {
                    cached_summary_ = summary;
                    cached_summary_count_ = dropped.size();
                    cached_summary_hash_ = hash_prefix(dropped.size());
                }
            }
        }
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
            stats_.dropped_messages++;
            // Refund what this block was costing, or the reported usage counts messages that
            // were never sent.
            used -= token_cost(turns[i]);
        }
        keep_from++;
    }
    if (used < 0)
        used = 0;

    // A summariser that ignores its brief must not be allowed to spend the whole window, so the
    // note is trimmed to the room set aside for it.
    if (!summary_note.empty()) {
        int summary_cost = token_cost(nlohmann::json{{"role", "system"}, {"content", summary_note}});
        if (summary_cost > kSummaryAllowance) {
            const size_t allowed_chars = static_cast<size_t>(kSummaryAllowance) * 36 / 10;
            summary_note = truncate_middle(summary_note, allowed_chars);
            summary_cost = token_cost(nlohmann::json{{"role", "system"}, {"content", summary_note}});
        }
        system_msg["content"] = system_msg["content"].get<std::string>() + summary_note;
    }

    nlohmann::json out = nlohmann::json::array();
    out.push_back(system_msg);
    for (size_t n = keep_from; n < blocks.size(); n++) {
        for (size_t i = blocks[n].first; i < blocks[n].second; i++)
            out.push_back(turns[i]);
    }

    // Measured on the finished system message rather than added up from parts: tokenisation is
    // not additive, and this is the figure the UI shows.
    stats_.used_tokens = token_cost(system_msg) + used;
    return out;
}

} // namespace agent
} // namespace delta
