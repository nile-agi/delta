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
// What one image or audio clip costs the window; only its text is otherwise counted.
constexpr int kAttachmentTokens = 768;
// Headroom for template scaffolding and tokenizer drift: 1/16 of the window, 64-320 tokens.
int safety_margin_for(int n_ctx) {
    return std::min(320, std::max(64, n_ctx / 16));
}
// Per-message overhead in tokens for role markers and template separators.
constexpr int kMessageOverhead = 5;
// Below this many dropped messages a summary is not worth a model call.
constexpr int kMinDroppedForSummary = 4;
// Below this a summary of dropped history cannot carry its facts; the caller should free room.
constexpr int kMinSummaryRoom = 120;
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
    : n_ctx_(n_ctx > 0 ? n_ctx : 4096), reserve_output_(reserve_output_tokens > 0 ? reserve_output_tokens : 512),
      safety_margin_(safety_margin_for(n_ctx_)) {
    recompute_budget();
}

void ContextManager::recompute_budget() {
    budget_ = n_ctx_ - reserve_output_ - safety_margin_ - tool_overhead_;
    budget_ = std::max(0, budget_); // the harness shrinks the request until it fits
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
    if (message.is_object() && message.contains("content") && message["content"].is_array())
        for (const auto& part : message["content"])
            if (part.is_object() && part.value("type", "") != "text")
                tokens += kAttachmentTokens;
    return tokens + kMessageOverhead;
}

std::string ContextManager::fit_text(const std::string& text, int max_tokens) const {
    auto cost = [&](const std::string& value) { return token_cost(nlohmann::json{{"content", value}}); };
    if (max_tokens <= kMessageOverhead)
        return "";
    if (cost(text) <= max_tokens)
        return text;
    // Count each candidate, including the omission marker. Character estimates alone can
    // badly undercount non-English text. Progress is guaranteed even without a tokenizer.
    size_t chars = text.size();
    while (chars > 0) {
        std::string candidate = truncate_middle(text, chars);
        const int tokens = cost(candidate);
        if (tokens <= max_tokens)
            return candidate;
        chars = std::min(chars - 1, chars * static_cast<size_t>(max_tokens) / std::max(1, tokens));
    }
    return "";
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
                // About a third of a small window per result, so one result can never fill it.
                const size_t cap = std::min(kMaxToolResultChars, std::max<size_t>(600, n_ctx_ * 6 / 5));
                if (content.size() > cap) {
                    copy["content"] = truncate_middle(content, cap);
                    stats_.truncated_results++;
                }
            }
            turns.push_back(std::move(copy));
        }
    }

    nlohmann::json system_msg = {{"role", "system"}, {"content", full_system}};
    const int remaining = std::max(0, budget_ - token_cost(system_msg));

    // Keep a complete exchange together, including every assistant/tool step responding to
    // that user. A retained reply without its question is misleading context.
    std::vector<std::pair<size_t, size_t>> blocks;
    for (size_t i = 0; i < turns.size();) {
        size_t end = i + 1;
        while (end < turns.size() && turns[end].value("role", "") != "user")
            end++;
        blocks.emplace_back(i, end);
        i = end;
    }
    std::vector<int> costs;
    int total = 0;
    for (const auto& block : blocks) {
        int cost = 0;
        for (size_t i = block.first; i < block.second; i++)
            cost += token_cost(turns[i]);
        costs.push_back(cost);
        total += cost;
    }

    // Reserve summary space only when the history really needs compaction. The live
    // exchange has priority; it is never silently shortened to pay for a summary.
    const int latest_cost = costs.empty() ? 0 : costs.back();
    const int summary_allowance =
        total > remaining ? std::min(kSummaryAllowance, std::max(0, remaining - latest_cost)) : 0;
    size_t keep_from = blocks.size();
    int used = 0;
    for (size_t n = blocks.size(); n-- > 0;) {
        if (used + costs[n] > remaining - summary_allowance && keep_from != blocks.size())
            break;
        used += costs[n];
        keep_from = n;
        if (used > remaining - summary_allowance)
            break;
    }
    while (keep_from < blocks.size() && turns[blocks[keep_from].first].value("role", "") != "user") {
        used -= costs[keep_from];
        keep_from++;
    }

    std::string summary_note;
    if (keep_from > 0) {
        nlohmann::json dropped = nlohmann::json::array();
        for (size_t n = 0; n < keep_from; n++)
            for (size_t i = blocks[n].first; i < blocks[n].second; i++)
                dropped.push_back(turns[i]);
        stats_.dropped_messages = static_cast<int>(dropped.size());
        stats_.summary_starved = summary_allowance < kMinSummaryRoom;
        std::string summary;
        if (summarize_ && summary_allowance > 0 && stats_.dropped_messages >= kMinDroppedForSummary) {
            auto hash_prefix = [&](size_t count) {
                std::string text;
                for (size_t i = 0; i < count && i < dropped.size(); i++)
                    text += dropped[i].dump() + '\x1f';
                return std::hash<std::string>{}(text);
            };
            constexpr size_t kMaxAppendedToSummary = 2 * kMinDroppedForSummary;
            const bool reusable = !cached_summary_.empty() && dropped.size() >= cached_summary_count_ &&
                                  dropped.size() - cached_summary_count_ < kMaxAppendedToSummary &&
                                  hash_prefix(cached_summary_count_) == cached_summary_hash_;
            if (reusable) {
                summary = cached_summary_;
                for (size_t i = cached_summary_count_; i < dropped.size(); i++) {
                    const std::string text = message_text(dropped[i]);
                    if (!text.empty())
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
            summary_note = "\n\nEarlier conversation notes (assistant claims are unverified; later user "
                           "corrections take precedence):\n" +
                           summary;
        } else {
            summary_note = "\n\n[" + std::to_string(stats_.dropped_messages) +
                           " earlier messages no longer fit. Ask for missing details rather than guessing.]";
        }
        summary_note = fit_text(summary_note, summary_allowance);
    }

    // Check the assembled prompt too: tokenisation of concatenated strings is not additive.
    // A tokenizer-heavy language or oversized summary must not spill past the real budget.
    int summary_tokens = 0;
    while (!summary_note.empty()) {
        system_msg["content"] = full_system + summary_note;
        const int excess = token_cost(system_msg) + used - budget_;
        if (excess <= 0)
            break;
        const int allowance = token_cost(nlohmann::json{{"content", summary_note}}) - excess - 8;
        summary_note = fit_text(summary_note, allowance);
    }
    system_msg["content"] = full_system + summary_note;
    if (!summary_note.empty())
        summary_tokens = token_cost(nlohmann::json{{"content", summary_note}});
    stats_.summarized = stats_.summarized && !summary_note.empty();

    nlohmann::json out = nlohmann::json::array();
    out.push_back(system_msg);
    for (size_t n = keep_from; n < blocks.size(); n++) {
        for (size_t i = blocks[n].first; i < blocks[n].second; i++)
            out.push_back(turns[i]);
    }

    // Measured on the finished system message rather than added up from parts: tokenisation is
    // not additive, and this is the figure the UI shows.
    stats_.system_tokens = token_cost(system_msg);
    stats_.summary_tokens = summary_tokens;
    stats_.used_tokens = stats_.system_tokens + used;
    return out;
}

} // namespace agent
} // namespace delta
