#include "harness.h"
#include "agent_database.h"
#include "memory_store.h"
#include "time_compat.h"
#include "tool_registry.h"
#include "tool_task.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <ctime>
#include <iostream>
#include <random>

namespace delta {
namespace agent {

namespace {

std::string make_run_id() {
    static std::mutex mtx;
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    std::lock_guard<std::mutex> lock(mtx);
    std::string id = "run_";
    for (int i = 0; i < 16; i++)
        id += hex[dis(gen)];
    return id;
}

std::string last_user_text(const nlohmann::json& messages) {
    for (int i = static_cast<int>(messages.size()) - 1; i >= 0; i--) {
        if (messages[i].is_object() && messages[i].value("role", "") == "user")
            return message_text(messages[i]);
    }
    return "";
}

// One line describing what a tool returned, for the activity feed in the UI. The model always
// gets the full result; this is only what a human sees.
std::string summarize_result(const std::string& tool, const ToolResult& result) {
    if (!result.success)
        return tool + " failed: " + result.error_message;
    nlohmann::json parsed;
    try {
        parsed = nlohmann::json::parse(result.content);
    } catch (...) {
        return tool + " finished";
    }
    if (!parsed.is_object())
        return tool + " finished";
    if (parsed.contains("count") && parsed["count"].is_number())
        return tool + " returned " + std::to_string(parsed["count"].get<int>()) + " result(s)";
    if (parsed.contains("exit_code"))
        return tool + " exited " + std::to_string(parsed.value("exit_code", -1));
    if (parsed.contains("bytes_written"))
        return tool + " wrote " + std::to_string(parsed.value("bytes_written", 0)) + " bytes";
    if (parsed.contains("bytes"))
        return tool + " read " + std::to_string(parsed.value("bytes", 0)) + " bytes";
    return tool + " finished";
}

// Answers a tool call without running it, for calls the harness refuses to execute.
nlohmann::json refused_tool_message(const std::string& call_id, const std::string& name, const std::string& error) {
    return {{"role", "tool"},
            {"tool_call_id", call_id},
            {"name", name},
            {"content", nlohmann::json{{"error", error}}.dump()}};
}

} // namespace

Harness::Harness(const std::string& llama_server_url, const std::string& model_name, bool supports_tools)
    : client_(llama_server_url, model_name), supports_tools_(supports_tools), run_id_(make_run_id()) {}

void Harness::set_options(const RunOptions& options) {
    options_ = options;
    LlmConfig cfg = client_.config();
    cfg.max_tokens = options_.max_tokens;
    cfg.temperature = options_.temperature;
    cfg.top_p = options_.top_p;
    cfg.enable_thinking = options_.enable_thinking;
    cfg.extra_sampling = options_.extra_sampling;
    client_.set_config(cfg);
    client_.set_abort_check(options_.abort_requested);
}

nlohmann::json Harness::active_tools() const {
    if (!supports_tools_ || !options_.tools_enabled)
        return nlohmann::json::array();
    auto& registry = ToolRegistry::instance();
    const auto& loaded = loaded_tool_categories();

    std::set<std::string> offered;
    for (const auto& category : registry.get_categories()) {
        if (!options_.enabled_categories.empty() && options_.enabled_categories.count(category) == 0)
            continue;
        // Deferred groups wait until the model asks for them by name.
        if (is_deferred_category(category) && loaded.count(category) == 0)
            continue;
        offered.insert(category);
    }
    // An empty set means "everything" to get_tools_array, which is the opposite of what we want.
    if (offered.empty())
        return nlohmann::json::array();
    return registry.get_tools_array(offered);
}

std::string Harness::deferred_manifest() const {
    if (!supports_tools_ || !options_.tools_enabled)
        return "";
    auto& registry = ToolRegistry::instance();
    const auto& loaded = loaded_tool_categories();

    std::string lines;
    for (const auto& category : registry.get_categories()) {
        if (!is_deferred_category(category) || loaded.count(category) > 0)
            continue;
        if (!options_.enabled_categories.empty() && options_.enabled_categories.count(category) == 0)
            continue;
        const std::string summary = category_summary(category);
        if (!summary.empty())
            lines += "- " + category + ": " + summary + "\n";
    }
    if (lines.empty())
        return "";
    return "\nWHAT ELSE YOU CAN DO\n"
           "You do not have these tools right now, but you can get them. Call load_tools with the "
           "group name when the job actually needs one.\n" +
           lines;
}

std::string Harness::build_system_prompt(const nlohmann::json& messages) const {
    time_t now = time(nullptr);
    struct tm t_local{};
    local_time(&now, &t_local);
    char today_buf[16], iso_buf[32], day_buf[16];
    strftime(today_buf, sizeof(today_buf), "%Y-%m-%d", &t_local);
    strftime(iso_buf, sizeof(iso_buf), "%Y-%m-%dT%H:%M", &t_local);
    strftime(day_buf, sizeof(day_buf), "%A", &t_local);

    time_t tomorrow_t = now + 24 * 3600;
    struct tm tm_tom{};
    local_time(&tomorrow_t, &tm_tom);
    char tom_buf[16], tom_day_buf[16];
    strftime(tom_buf, sizeof(tom_buf), "%Y-%m-%d", &tm_tom);
    strftime(tom_day_buf, sizeof(tom_day_buf), "%A", &tm_tom);

    const bool has_tools = !active_tools().empty();

    std::string prompt = "You are Delta, an AI assistant that runs entirely on the user's own machine.\n\n";
    prompt += "CURRENT TIME: " + std::string(iso_buf) + " (" + day_buf + ")\n";
    prompt += "TODAY: " + std::string(today_buf) + "\nTOMORROW: " + tom_buf + " (" + tom_day_buf + ")\n";

    if (has_tools) {
        prompt += "\nHOW YOU WORK\n"
                  "You have tools. Use one when the job needs it and answer directly when it does not. "
                  "Work a request through to the end rather than describing what you would do.\n"
                  "- Read the context below before reaching for a tool. If what you need is already "
                  "there, use it.\n"
                  "- If a request is ambiguous, ask. When the user says \"it\" or \"that one\" and more "
                  "than one thing fits, ask which they mean instead of picking one. A wrong guess "
                  "costs them more than a short question.\n"
                  "- After a tool runs, use what it actually returned. Never say something is done "
                  "that a tool did not confirm.\n"
                  "- If a tool fails twice the same way, stop and say what is blocking you.\n"
                  "- For work needing more than two steps, call set_plan first, then update_plan as "
                  "you finish each one. Use note_to_self for anything you will need again later.\n"
                  "- Some tools ask the user's permission first. If one is refused, do not retry it: "
                  "say what you wanted to do and why.\n"
                  "- Answer briefly and plainly. The ids in [id:...] are for you to pass to tools; "
                  "never show them to the user, and do not name the tools you used.\n"
                  "\nDATES: pass them naturally ('friday 2pm', 'tomorrow 1300') -- the calendar tools "
                  "resolve them. Default to 09:00 when no time is given.\n"
                  "TYPE: use type='task' for things the user has to DO, type='event' only for meetings "
                  "and appointments. STATUS: done/complete -> \"completed\", cancel -> \"cancelled\", "
                  "start/begin -> \"in_progress\".\n";
        // Only once the run is genuinely near its end: nagging from the first turn would just
        // make the model rush a job it has plenty of room for.
        if (steps_remaining_ >= 0 && steps_remaining_ <= 3 && steps_remaining_ * 2 <= options_.max_iterations) {
            prompt += "\nYou have " + std::to_string(steps_remaining_) +
                      (steps_remaining_ == 1 ? " step left" : " steps left") +
                      " on this task. Finish what you can and tell the user where you got to.\n";
        }
        prompt += deferred_manifest();
    } else {
        prompt += "\nAnswer from the conversation and the context below. Keep responses brief and friendly.\n";
    }

    // What the harness already knows, so the model does not have to spend a turn asking.
    auto& memory = MemoryStore::instance();
    std::string context_block;

    // The plan and the working notes change as tools run, so they are rendered fresh every
    // iteration; the rest of the context is cached per user turn.
    std::string plan_block;
    if (memory.ready()) {
        auto notes = memory.get_notes(scratchpad_key());
        if (notes.is_array() && !notes.empty()) {
            plan_block += "\nWhat you have worked out so far on this job:\n";
            for (const auto& note : notes) {
                if (note.is_string())
                    plan_block += "- " + note.get<std::string>() + "\n";
            }
        }

        auto plan = memory.get_plan(scratchpad_key());
        if (plan.is_object() && plan.contains("steps") && plan["steps"].is_array() && !plan["steps"].empty()) {
            plan_block += "\nYour current plan (goal: " + plan.value("goal", "") + "):\n";
            int idx = 0;
            for (const auto& step : plan["steps"]) {
                plan_block += "  " + std::to_string(idx++) + ". [" + step.value("status", "pending") + "] " +
                              step.value("step", "") + "\n";
            }
        }
    }

    const std::string cache_key = std::string(today_buf) + "|" + last_user_text(messages);
    if (cache_key == context_cache_key_) {
        if (!context_cache_.empty() || !plan_block.empty())
            prompt += "\n--- Context ---" + context_cache_ + plan_block;
        return prompt;
    }

    if (memory.ready()) {
        // Scoped to this conversation plus the general ones, so an unrelated job's details never
        // turn up in the prompt.
        auto pinned_memories = memory.pinned(5, memory_key());
        auto relevant = memory.search(last_user_text(messages), 5, memory_key());
        std::set<std::string> seen;
        std::string lines;
        for (const auto& m : pinned_memories) {
            if (seen.insert(m.id).second)
                lines += "- " + m.content + "\n";
        }
        for (const auto& m : relevant) {
            if (seen.insert(m.id).second)
                lines += "- " + m.content + "\n";
        }
        if (!lines.empty())
            context_block += "\nWhat you remember about this user:\n" + lines;
    }

    const bool calendar_on = options_.enabled_categories.empty() || options_.enabled_categories.count("calendar") > 0;
    if (calendar_on) {
        auto& db = AgentDatabase::instance();
        char week_buf[16];
        time_t week_later = now + 7 * 24 * 3600;
        struct tm wt_local{};
        local_time(&week_later, &wt_local);
        strftime(week_buf, sizeof(week_buf), "%Y-%m-%d", &wt_local);

        auto events = db.list_events(std::string(today_buf), std::string(week_buf), 5, "event");
        auto tasks = db.list_events("", "", 5, "task", "upcoming");
        auto in_progress = db.list_events("", "", 5, "task", "in_progress");
        tasks.insert(tasks.end(), in_progress.begin(), in_progress.end());

        if (!events.empty()) {
            context_block += "\nUpcoming events this week:\n";
            for (auto& e : events) {
                context_block +=
                    "- [id:" + e.value("id", "") + "] " + e.value("title", "") + " at " + e.value("start_time", "");
                if (!e.value("location", "").empty())
                    context_block += " (" + e["location"].get<std::string>() + ")";
                context_block += "\n";
            }
        }
        if (!tasks.empty()) {
            context_block += "\nActive tasks:\n";
            for (auto& t : tasks) {
                context_block +=
                    "- [id:" + t.value("id", "") + "] [" + t.value("priority", "medium") + "] " + t.value("title", "");
                if (!t.value("start_time", "").empty())
                    context_block += " (due: " + t["start_time"].get<std::string>() + ")";
                context_block += " [" + t.value("status", "") + "]\n";
            }
        }
    }

    context_cache_ = context_block;
    context_cache_key_ = cache_key;

    if (!context_block.empty() || !plan_block.empty())
        prompt += "\n--- Context ---" + context_block + plan_block;

    return prompt;
}

std::string Harness::summarize(const nlohmann::json& dropped) {
    if (dropped.empty())
        return "";

    std::string transcript;
    for (const auto& msg : dropped) {
        const std::string role = msg.value("role", "");
        std::string text = message_text(msg);
        if (text.size() > 500)
            text = text.substr(0, ContextManager::utf8_floor(text, 500)) + "...";
        if (text.empty())
            continue;
        transcript += role + ": " + text + "\n";
    }
    if (transcript.empty())
        return "";

    nlohmann::json request = nlohmann::json::array();
    request.push_back({{"role", "system"},
                       {"content", "Summarize the conversation excerpt the user gives you in at most 120 words. "
                                   "Keep decisions, facts, names, numbers and anything still unresolved. "
                                   "Write it as notes for yourself, not as a reply."}});
    request.push_back({{"role", "user"}, {"content", transcript}});

    // The summary itself must not run away with the context window.
    LlmConfig saved = client_.config();
    LlmConfig small = saved;
    small.max_tokens = 220;
    client_.set_config(small);
    nlohmann::json response = client_.chat(request, nlohmann::json::array());
    client_.set_config(saved);

    if (!response.is_object() || response.contains("error") || !response.contains("choices") ||
        !response["choices"].is_array() || response["choices"].empty())
        return "";
    const auto& choice = response["choices"][0];
    if (!choice.contains("message"))
        return "";
    return message_text(choice["message"]);
}

RunResult Harness::run(const nlohmann::json& messages, const EventSink& sink) {
    RunResult result;
    auto& registry = ToolRegistry::instance();

    const std::string pad = scratchpad_key();
    set_active_run_id(pad);
    set_active_memory_scope(memory_key());
    // Nothing deferred is loaded yet, and load_tools may never reach past what the client allows.
    begin_tool_session(options_.enabled_categories);
    MemoryStore::instance().prune_plans();
    MemoryStore::instance().prune_notes();

    auto emit = [&](EventType type, nlohmann::json data) -> bool {
        if (!sink)
            return true;
        return sink(HarnessEvent{type, std::move(data)});
    };

    // Resolve the real context window once per run.
    int n_ctx = options_.n_ctx;
    if (n_ctx <= 0)
        n_ctx = client_.probe_context_size();
    if (n_ctx <= 0)
        n_ctx = 4096;

    ContextManager context(n_ctx, options_.max_tokens);
    context.set_token_counter([this](const std::string& text) { return client_.count_tokens(text); });
    context.set_summarizer([this](const nlohmann::json& dropped) { return summarize(dropped); });

    Policy policy(options_.policy);

    // The schemas go out with every request but are not part of `transcript`, so unless the budget
    // is told what they cost it happily fills the window and the request overflows on the server.
    // Counting is an HTTP round trip, so the result is cached until the tool set actually changes.
    int cached_tool_tokens = -1;
    size_t cached_tool_hash = 0;
    auto charge_tools = [&](const nlohmann::json& active_tools_json) {
        const std::string dumped = active_tools_json.empty() ? std::string() : active_tools_json.dump();
        const size_t hash = std::hash<std::string>{}(dumped);
        if (cached_tool_tokens < 0 || hash != cached_tool_hash) {
            int cost = 0;
            if (!dumped.empty()) {
                cost = client_.count_tokens(dumped);
                if (cost < 0)
                    cost = ContextManager::estimate_tokens(dumped);
            }
            cached_tool_tokens = cost;
            cached_tool_hash = hash;
        }
        context.set_tool_overhead(cached_tool_tokens);

        // A small window plus a large tool set can leave no room for the conversation at all. The
        // reply length is the one thing the harness can give up, so trim it rather than send a
        // request the server will reject.
        constexpr int kMinRoomForHistory = 768;
        constexpr int kMinReplyTokens = 256;
        const int spare = n_ctx - cached_tool_tokens - kMinRoomForHistory;
        if (spare < options_.max_tokens) {
            const int trimmed = std::max(kMinReplyTokens, spare);
            if (trimmed < options_.max_tokens) {
                LlmConfig cfg = client_.config();
                cfg.max_tokens = trimmed;
                client_.set_config(cfg);
                context.set_reserve_output(trimmed);
            }
        }
    };

    // Working transcript: the client's history plus everything this run adds.
    nlohmann::json transcript = nlohmann::json::array();
    for (const auto& msg : messages) {
        if (msg.is_object())
            transcript.push_back(msg);
    }

    // The model's thinking, when it streams on its own channel, goes out as its own event so a
    // client can show or hide it without it ever being mistaken for the answer.
    client_.set_reasoning_sink(
        [&](const std::string& delta) -> bool { return emit(EventType::Reasoning, {{"text", delta}}); });

    // Everything after this index was added by this run and goes back to the caller.
    const size_t history_size = transcript.size();

    // Every exit passes through here so the transcript delta and the plan are handled the same
    // way regardless of why the run ended. A plan keyed by the conversation is kept when the run
    // was cut short (budget hit, abort, error) so "keep going" resumes it, and cleared when the
    // model finished its turn: it rarely marks steps done, and a stale plan in the next prompt
    // would read as unfinished work. A per-run plan has no next turn, so it is always cleared.
    auto finish = [&](RunResult& r) -> RunResult {
        r.transcript_delta = nlohmann::json::array();
        for (size_t i = history_size; i < transcript.size(); i++)
            r.transcript_delta.push_back(transcript[i]);
        if (options_.scratchpad_id.empty() || r.stop_reason == "stop") {
            MemoryStore::instance().clear_plan(pad);
            MemoryStore::instance().clear_notes(pad);
        }
        return r;
    };

    // One last turn with no tools, so a run that ran out of room reports what it actually did
    // instead of printing an apology. Streamed like any other answer, so the user watches it arrive.
    auto wrap_up = [&](const std::string& why) -> std::string {
        nlohmann::json closing = transcript;
        closing.push_back({{"role", "user"},
                           {"content", why + " Do not call any tools now. In a few sentences, tell me what you "
                                             "did, what you found, and what is still left to do."}});

        LlmConfig saved = client_.config();
        LlmConfig brief = saved;
        brief.max_tokens = std::min(saved.max_tokens, 400);
        client_.set_config(brief);
        // No tools go out on this turn, so the budget must not still reserve room for them --
        // otherwise the closing summary drops history it did not need to.
        context.set_tool_overhead(0);
        nlohmann::json request_messages = context.build(build_system_prompt(transcript), closing);
        size_t forwarded = 0;
        bool aborted = false;
        nlohmann::json response = client_.chat_stream(
            request_messages, nlohmann::json::array(), "auto",
            [&](const std::string& delta) -> bool { return emit(EventType::Content, {{"text", delta}}); }, forwarded,
            aborted);
        client_.set_config(saved);
        result.streamed_chars += forwarded;

        if (aborted || !response.is_object() || !response.contains("choices") || !response["choices"].is_array() ||
            response["choices"].empty())
            return "";
        const auto& choice = response["choices"][0];
        if (!choice.contains("message") || !choice["message"].is_object())
            return "";
        return message_text(choice["message"]);
    };

    const auto started = std::chrono::steady_clock::now();
    auto out_of_time = [&] {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count();
        return elapsed >= options_.wall_clock_seconds;
    };

    std::string last_content;
    bool tools_disabled_by_error = false;

    // Going in circles is the characteristic failure of a small model with tools: it calls the same
    // thing, gets the same answer, and calls it again. Warn on the third time, stop on the fourth.
    // Keyed by tool name and arguments, so a different question to the same tool starts over.
    constexpr int kWarnAfterRepeats = 3;
    constexpr int kStopAfterRepeats = 4;
    std::map<std::string, int> identical_calls;
    std::map<std::string, std::string> previous_payload;
    std::string failing_tool;
    int consecutive_failures = 0;
    bool going_in_circles = false;
    int empty_replies = 0;

    for (int iteration = 0; iteration < options_.max_iterations; iteration++) {
        result.iterations = iteration + 1;
        steps_remaining_ = options_.max_iterations - iteration - 1;

        if (out_of_time()) {
            result.stop_reason = "time_budget";
            emit(EventType::Status, {{"message", "Stopped: this run hit its time budget."}});
            break;
        }

        // Recomputed every pass because load_tools can add a group part-way through a run.
        nlohmann::json active = tools_disabled_by_error ? nlohmann::json::array() : active_tools();
        charge_tools(active);

        const std::string system_prompt = build_system_prompt(transcript);
        nlohmann::json request_messages = context.build(system_prompt, transcript);
        const auto& stats = context.stats();
        if (stats.dropped_messages > 0 || stats.truncated_results > 0) {
            emit(EventType::Compaction, {{"dropped", stats.dropped_messages},
                                         {"summarized", stats.summarized},
                                         {"truncated_results", stats.truncated_results},
                                         {"used_tokens", stats.used_tokens},
                                         {"tool_tokens", stats.tool_tokens},
                                         {"budget_tokens", stats.budget_tokens}});
        }

        size_t forwarded = 0;
        bool client_aborted = false;
        nlohmann::json response = client_.chat_stream(
            request_messages, active, "auto",
            [&](const std::string& delta) -> bool { return emit(EventType::Content, {{"text", delta}}); }, forwarded,
            client_aborted);
        result.streamed_chars += forwarded;

        if (client_aborted) {
            result.client_aborted = true;
            result.stop_reason = "client_aborted";
            result.error = "client disconnected";
            return finish(result);
        }

        // A model advertised as tool-capable can still reject the schemas. Retry once without them
        // rather than failing the whole turn. Only a complaint about the request counts: a transport
        // failure would fail again without tools, and a 5xx means the server is broken, not the
        // schemas -- dropping tools there would mislead the user and cripple the rest of the run.
        bool server_rejected = false;
        if (response.is_object() && response.contains("error")) {
            const auto& err = response["error"];
            const std::string message = err.is_string()   ? err.get<std::string>()
                                        : err.is_object() ? err.value("message", std::string())
                                                          : std::string();
            const int status = response.value("http_status", 0);
            const bool about_tools =
                message.find("tool") != std::string::npos || message.find("template") != std::string::npos;
            server_rejected = (status >= 400 && status < 500) || (status >= 500 && about_tools);
        }
        if (server_rejected && !active.empty() && !tools_disabled_by_error) {
            std::cerr << "[delta-harness] tools rejected, retrying without them: " << response["error"].dump()
                      << std::endl;
            tools_disabled_by_error = true;
            emit(EventType::Status, {{"message", "This model rejected the tool schemas; answering without tools."}});
            continue;
        }

        if (!response.is_object() || response.contains("error")) {
            std::string message = "Unexpected response from the model";
            if (response.is_object() && response.contains("error")) {
                const auto& err = response["error"];
                message = err.is_string()   ? err.get<std::string>()
                          : err.is_object() ? err.value("message", err.dump())
                                            : err.dump();
            }
            result.error = message;
            result.stop_reason = "error";
            emit(EventType::Error, {{"message", message}});
            return finish(result);
        }

        if (!response.contains("choices") || !response["choices"].is_array() || response["choices"].empty()) {
            result.error = "No choices in the model response";
            result.stop_reason = "error";
            emit(EventType::Error, {{"message", result.error}});
            return finish(result);
        }

        const auto& choice = response["choices"][0];
        if (!choice.contains("message") || !choice["message"].is_object()) {
            result.error = "No message in the model response";
            result.stop_reason = "error";
            emit(EventType::Error, {{"message", result.error}});
            return finish(result);
        }
        const bool cut_off = choice.value("finish_reason", "") == "length";

        nlohmann::json assistant = choice["message"];
        const std::string content = message_text(assistant);
        if (!content.empty())
            last_content = content;

        const bool has_tool_calls =
            assistant.contains("tool_calls") && assistant["tool_calls"].is_array() && !assistant["tool_calls"].empty();

        if (!has_tool_calls) {
            // Nothing at all: no answer, no action. Ending the turn here would leave the user
            // staring at an empty reply, so take one more sample before giving up.
            if (content.empty() && empty_replies == 0) {
                empty_replies++;
                emit(EventType::Status, {{"message", "The model replied with nothing; asking again."}});
                continue;
            }
            transcript.push_back(assistant);
            result.success = true;
            result.content = content;
            result.stop_reason = "stop";
            return finish(result);
        }

        // Record the assistant turn exactly as the model produced it, then answer every call.
        transcript.push_back(assistant);

        // A refused call still gets a tool message, so the transcript never carries a tool_call
        // without its answer. Returns false when the client has gone away.
        // `announce` emits the tool_start the UI needs to attach the failure to, for calls refused
        // before they were ever announced.
        auto refuse = [&](const std::string& call_id, const std::string& name, const std::string& why,
                          bool announce) -> bool {
            if (announce && !emit(EventType::ToolStart, {{"call_id", call_id},
                                                         {"name", name},
                                                         {"arguments", nlohmann::json::object()},
                                                         {"risk", "unknown"}}))
                return false;
            transcript.push_back(refused_tool_message(call_id, name, why));
            return emit(EventType::ToolResult,
                        {{"call_id", call_id}, {"name", name}, {"success", false}, {"error", why}});
        };
        auto aborted = [&]() -> RunResult {
            result.client_aborted = true;
            result.stop_reason = "client_aborted";
            return finish(result);
        };

        for (const auto& call : assistant["tool_calls"]) {
            if (!call.is_object() || !call.contains("function"))
                continue;
            const std::string name = call["function"].value("name", "");
            const std::string call_id = call.value("id", name);

            // The model ran out of output tokens part-way through its call. The arguments are
            // whatever fragment survived, so running the tool would act on the wrong input.
            if (cut_off) {
                if (!refuse(call_id, name,
                            "This tool call was cut off by the output token limit before it was complete. "
                            "Make the call again with shorter arguments.",
                            true))
                    return aborted();
                continue;
            }

            nlohmann::json arguments = nlohmann::json::object();
            bool arguments_ok = true;
            if (call["function"].contains("arguments")) {
                const auto& raw = call["function"]["arguments"];
                if (raw.is_string()) {
                    try {
                        arguments = nlohmann::json::parse(raw.get<std::string>());
                    } catch (...) {
                        arguments_ok = false;
                    }
                } else if (raw.is_object()) {
                    arguments = raw;
                }
            }
            if (!arguments_ok) {
                if (!refuse(call_id, name,
                            "The arguments for this tool call were not valid JSON, so it was not run. "
                            "Make the call again with well-formed arguments.",
                            true))
                    return aborted();
                continue;
            }

            const ToolDefinition* def = registry.get_definition(name);
            if (!def) {
                // Shown as a step like any other call, so the UI has something to attach the
                // failure to.
                if (!emit(EventType::ToolStart,
                          {{"call_id", call_id}, {"name", name}, {"arguments", arguments}, {"risk", "unknown"}}))
                    return aborted();
                if (!refuse(call_id, name, "No tool called " + name, false))
                    return aborted();
                continue;
            }

            if (options_.abort_requested && options_.abort_requested())
                return aborted();
            if (!emit(EventType::ToolStart,
                      {{"call_id", call_id}, {"name", name}, {"arguments", arguments}, {"risk", risk_name(def->risk)}}))
                return aborted();

            Decision decision = policy.decide(*def);

            if (decision == Decision::Ask) {
                const std::string approval_id = ApprovalBroker::instance().open(name, arguments);
                if (!emit(EventType::ApprovalRequired, {{"id", approval_id},
                                                        {"call_id", call_id},
                                                        {"name", name},
                                                        {"arguments", arguments},
                                                        {"risk", risk_name(def->risk)},
                                                        {"description", def->description}})) {
                    ApprovalBroker::instance().cancel(approval_id);
                    return aborted();
                }
                const std::string answer =
                    ApprovalBroker::instance().wait(approval_id, options_.approval_timeout_seconds);
                if (!emit(EventType::ApprovalResolved, {{"id", approval_id}, {"decision", answer}}))
                    return aborted();

                if (answer == "allow") {
                    decision = Decision::Allow;
                } else if (answer == "always") {
                    MemoryStore::instance().set_policy(name, "allow");
                    policy.allow_for_run(name);
                    decision = Decision::Allow;
                } else if (answer == "never") {
                    MemoryStore::instance().set_policy(name, "deny");
                    policy.deny_for_run(name);
                    decision = Decision::Deny;
                } else {
                    decision = Decision::Deny;
                }

                if (decision == Decision::Deny) {
                    const std::string why = (answer == "timeout")
                                                ? "The user did not answer the approval request in time."
                                                : "The user declined this action.";
                    transcript.push_back({{"role", "tool"},
                                          {"tool_call_id", call_id},
                                          {"name", name},
                                          {"content", nlohmann::json{{"denied", true}, {"reason", why}}.dump()}});
                    if (!emit(EventType::ToolResult,
                              {{"call_id", call_id}, {"name", name}, {"success", false}, {"error", why}}))
                        return aborted();
                    continue;
                }
            } else if (decision == Decision::Deny) {
                const std::string why = "This tool is not permitted right now.";
                transcript.push_back({{"role", "tool"},
                                      {"tool_call_id", call_id},
                                      {"name", name},
                                      {"content", nlohmann::json{{"denied", true}, {"reason", why}}.dump()}});
                if (!emit(EventType::ToolResult,
                          {{"call_id", call_id}, {"name", name}, {"success", false}, {"error", why}}))
                    return aborted();
                continue;
            }

            const ToolResult tool_result = registry.execute(name, arguments);
            result.tool_calls++;
            result.executed_tools.push_back(
                {{"name", name}, {"arguments", arguments}, {"success", tool_result.success}});

            // Every result goes back to the model, success or failure. This is the core difference
            // from the loop this replaces, which answered write tools with a templated sentence and
            // never let the model see what happened.
            std::string payload =
                tool_result.success ? tool_result.content : nlohmann::json{{"error", tool_result.error_message}}.dump();

            // Same call, same answer, again. The warning rides along with the real result rather
            // than replacing it, so the model still has what it asked for.
            const std::string signature = name + "|" + arguments.dump();
            auto& seen = identical_calls[signature];
            seen = (previous_payload.count(signature) && previous_payload[signature] == payload) ? seen + 1 : 1;
            previous_payload[signature] = payload;
            if (seen >= kStopAfterRepeats) {
                going_in_circles = true;
            } else if (seen >= kWarnAfterRepeats) {
                payload += "\n\n[Delta: you have now called " + name + " with these arguments " + std::to_string(seen) +
                           " times and got the same result each time. Calling it again will not change anything. "
                           "Use what you already have, try something different, or tell the user what is blocking "
                           "you.]";
            }

            if (!tool_result.success) {
                consecutive_failures = (failing_tool == name) ? consecutive_failures + 1 : 1;
                failing_tool = name;
                if (consecutive_failures >= kStopAfterRepeats) {
                    going_in_circles = true;
                } else if (consecutive_failures >= kWarnAfterRepeats) {
                    payload += "\n\n[Delta: " + name + " has now failed " + std::to_string(consecutive_failures) +
                               " times in a row. Read the error, then either fix the arguments or take a different "
                               "approach -- repeating the call will not help.]";
                }
            } else {
                consecutive_failures = 0;
                failing_tool.clear();
            }
            transcript.push_back({{"role", "tool"}, {"tool_call_id", call_id}, {"name", name}, {"content", payload}});

            if (!emit(EventType::ToolResult, {{"call_id", call_id},
                                              {"name", name},
                                              {"success", tool_result.success},
                                              {"summary", summarize_result(name, tool_result)},
                                              {"error", tool_result.error_message}}))
                return aborted();
        }

        if (going_in_circles) {
            result.stop_reason = "stuck";
            break;
        }
    }

    // Fell out of the loop: the model kept calling tools until a budget ran out.
    if (result.stop_reason.empty())
        result.stop_reason = "max_iterations";
    result.success = true;
    std::string why = "You have run out of steps for this task.";
    std::string notice = "Out of steps; asking for a summary of what was done.";
    if (result.stop_reason == "time_budget") {
        why = "You have run out of time for this task.";
        notice = "Out of time; asking for a summary of what was done.";
    } else if (result.stop_reason == "stuck") {
        why = "You have repeated the same action several times without making progress, so I stopped you.";
        notice = "Stopped: the same step was repeating without progress.";
    }
    emit(EventType::Status, {{"message", notice}});
    std::string closing = wrap_up(why);
    if (closing.empty())
        closing = last_content.empty() ? "I worked through several steps but ran out of room before finishing. "
                                         "Tell me to keep going and I will pick up where I stopped."
                                       : last_content;
    result.content = closing;
    // Close the transcript on an assistant turn so the stored conversation never ends on a tool
    // result, which strict chat templates reject on the next turn.
    transcript.push_back({{"role", "assistant"}, {"content", result.content}});
    emit(EventType::Status, {{"message", "Stopped after " + std::to_string(result.iterations) + " steps."}});
    return finish(result);
}

} // namespace agent
} // namespace delta
