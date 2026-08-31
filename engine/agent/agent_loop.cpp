#include "agent_loop.h"
#include "agent_database.h"
#include "time_compat.h"
#include "tool_calendar.h"
#include "tool_registry.h"
#include <cctype>
#include <ctime>
#include <curl/curl.h>
#include <iostream>
#include <set>
#include <vector>
#include <future>

namespace delta {
namespace agent {

static nlohmann::json filter_tools_by_config(const nlohmann::json& all_tools, bool use_calendar, bool use_notes) {
    if (use_calendar && use_notes) return all_tools;
    nlohmann::json filtered = nlohmann::json::array();
    std::set<std::string> calendar_tools = {"create_event", "list_events", "delete_event", "update_event", "get_current_time"};
    std::set<std::string> notes_tools = {"list_notes", "create_note", "get_note", "update_note", "delete_note"};
    for (const auto& tool : all_tools) {
        if (!tool.is_object() || !tool.contains("function")) continue;
        std::string name = tool["function"].value("name", "");
        bool is_calendar = calendar_tools.count(name) > 0;
        bool is_notes = notes_tools.count(name) > 0;
        if (!is_calendar && !is_notes) filtered.push_back(tool);
        else if (is_calendar && use_calendar) filtered.push_back(tool);
        else if (is_notes && use_notes) filtered.push_back(tool);
    }
    return filtered;
}

static std::string get_text_content(const nlohmann::json& msg, const std::string& key = "content") {
    if (!msg.contains(key)) return "";
    auto& val = msg[key];
    if (val.is_string()) return val.get<std::string>();
    if (val.is_array()) {
        for (auto& part : val) {
            if (part.is_object() && part.value("type", "") == "text") return part.value("text", "");
            if (part.is_string()) return part.get<std::string>();
        }
    }
    return "";
}

// static void sanitize_tool_messages_for_strict_templates(nlohmann::json& messages) {
//     for (auto& msg : messages) {
//         if (!msg.is_object()) continue;
//         if (msg.value("role", "") == "tool") {
//             msg["role"] = "user";
//             std::string content = msg.value("content", "");
//             msg["content"] = "[Tool executed successfully. Result: " + content + "]\n\nNow respond to the user based on this tool result.";
//         }
//         // Remove tool_calls field from assistant messages to avoid confusing strict templates
//         if (msg.value("role", "") == "assistant" && msg.contains("tool_calls")) {
//             msg.erase("tool_calls");
//         }
//     }
//     nlohmann::json cleaned = nlohmann::json::array();
//     for (auto& msg : messages) {
//         if (!cleaned.empty() && cleaned.back().value("role", "") == msg.value("role", "") && msg.value("role", "") != "system") {
//             cleaned.back()["content"] = cleaned.back()["content"].get<std::string>() + "\n" + msg["content"].get<std::string>();
//         } else {
//             cleaned.push_back(msg);
//         }
//     }
//     messages = cleaned;
// }


static void sanitize_tool_messages_for_strict_templates(nlohmann::json& messages) {
    for (auto& msg : messages) {
        if (!msg.is_object()) continue;
        
        if (msg.value("role", "") == "tool") {
            msg["role"] = "user";
            std::string content = msg.value("content", "");
            msg["content"] = "[Tool executed successfully. Result: " + content + "]\n\nNow respond to the user based on this tool result.";
            msg.erase("tool_call_id");
            msg.erase("name");
        }
        
        if (msg.value("role", "") == "assistant" && msg.contains("tool_calls")) {
            msg.erase("tool_calls");
        }
        
        // Safety: ensure user/system never have tool fields
        if (msg.value("role", "") == "user" || msg.value("role", "") == "system") {
            msg.erase("tool_call_id");
            msg.erase("name");
            msg.erase("tool_calls");
        }
    }
    
    nlohmann::json cleaned = nlohmann::json::array();
    for (auto& msg : messages) {
        if (!cleaned.empty() && cleaned.back().value("role", "") == msg.value("role", "") && msg.value("role", "") != "system") {
            cleaned.back()["content"] = cleaned.back()["content"].get<std::string>() + "\n" + msg["content"].get<std::string>();
        } else {
            cleaned.push_back(msg);
        }
    }
    messages = cleaned;
}


static std::string strip_tool_code_blocks(const std::string& text) {
    static const char* markers[] = {"TOOL_CALL", "TOOL_CODE", "create_event(", "update_event(", "delete_event(",
                                    "list_events(", "create_note(", "list_notes(", "get_note(", "update_note(",
                                    "delete_note(", "create_meeting(", "create_task(", nullptr};
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        size_t fence = text.find("```", i);
        if (fence == std::string::npos) { out += text.substr(i); break; }
        size_t close = text.find("```", fence + 3);
        if (close == std::string::npos) { out += text.substr(i); break; }
        close += 3;
        while (close < text.size() && text[close] != '\n') close++;
        if (close < text.size()) close++;
        std::string block = text.substr(fence, close - fence);
        bool has_tool = false;
        for (int m = 0; markers[m]; m++) {
            if (block.find(markers[m]) != std::string::npos) { has_tool = true; break; }
        }
        out += text.substr(i, fence - i);
        if (!has_tool) out += block;
        i = close;
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
    return out;
}

static void sanitize_strict_roles(nlohmann::json& messages) {
    nlohmann::json out = nlohmann::json::array();
    for (auto& msg : messages) {
        if (!msg.is_object()) continue;
        std::string role = msg.value("role", "");
        std::string content = get_text_content(msg);

        if (role == "tool") { role = "user"; content = "[Tool result] " + content; }
        if (role != "system" && role != "user" && role != "assistant") continue;
        
        // Remove tool_calls field to avoid confusing strict templates
        if (role == "assistant" && msg.contains("tool_calls")) {
            msg.erase("tool_calls");
        }

        if (!out.empty() && out.back().value("role", "") == role && role != "system") {
            out.back()["content"] = out.back()["content"].get<std::string>() + "\n" + content;
        } else {
            out.push_back({{"role", role}, {"content", content}});
        }
    }
    for (size_t i = 0; i < out.size(); i++) {
        if (out[i].value("role", "") == "system") continue;
        if (out[i].value("role", "") != "user")
            out.insert(out.begin() + i, {{"role", "user"}, {"content", "(continue)"}});
        break;
    }
    messages = out;
}

static std::string last_user_text(const nlohmann::json& messages) {
    for (int i = static_cast<int>(messages.size()) - 1; i >= 0; i--) {
        if (messages[i].value("role", "") == "user") {
            std::string candidate = get_text_content(messages[i]);
            if (candidate.find("Do it now.") == std::string::npos) return candidate;
        }
    }
    return "";
}

static std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool user_wants_action(const nlohmann::json& messages) {
    const std::string last_user = to_lower(last_user_text(messages));
    static const char* action_words[] = {"push", "move", "reschedule", "change", "update", "delete", "cancel", "mark", "done", "complete", "remove", "postpone", "ahead", "later", "earlier", "forward", "back", "start", "begin", nullptr};
    for (int k = 0; action_words[k]; k++) if (last_user.find(action_words[k]) != std::string::npos) return true;
    return false;
}

static bool user_wants_create(const nlohmann::json& messages) {
    const std::string last_user = to_lower(last_user_text(messages));
    static const char* create_words[] = {"create", "add", "schedule", "book", "remind",
                                         "set up", "setup", "new ", "plan ", "set a",
                                         "set an", "set my", "make a", "arrange", "organize",
                                         "log", "jot", "note down", "write down", nullptr};
    for (int k = 0; create_words[k]; k++) {
        if (last_user.find(create_words[k]) != std::string::npos) return true;
    }
    return false;
}

static std::string normalize_tool_name(const std::string& raw) {
    std::string n = to_lower(raw);
    if (n == "create_meeting" || n == "create_event" || n == "add_event" || n == "schedule_event" ||
        n == "create_appointment" || n == "set_meeting" || n == "create_reminder") return "create_event";
    if (n == "create_task" || n == "add_task" || n == "create_todo") return "create_event";
    if (n == "list_events" || n == "get_events" || n == "show_events" || n == "list_tasks") return "list_events";
    if (n == "update_event" || n == "move_event" || n == "reschedule_event" || n == "update_task") return "update_event";
    if (n == "delete_event" || n == "remove_event" || n == "delete_task") return "delete_event";
    if (n == "create_note" || n == "add_note" || n == "write_note") return "create_note";
    if (n == "list_notes" || n == "get_notes" || n == "show_notes") return "list_notes";
    if (n == "get_note" || n == "read_note") return "get_note";
    if (n == "update_note" || n == "edit_note") return "update_note";
    if (n == "delete_note" || n == "remove_note") return "delete_note";
    if (n == "get_current_time" || n == "current_time") return "get_current_time";
    return "";
}

static nlohmann::json parse_kwargs(const std::string& s) {
    nlohmann::json out = nlohmann::json::object();
    size_t i = 0;
    auto skip_ws = [&]() { while (i < s.size() && std::isspace((unsigned char)s[i])) i++; };
    while (i < s.size()) {
        skip_ws();
        size_t ks = i;
        while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_')) i++;
        if (i == ks) break;
        std::string key = s.substr(ks, i - ks);
        skip_ws();
        if (i >= s.size() || s[i] != '=') break;
        i++; skip_ws();
        if (i < s.size() && (s[i] == '"' || s[i] == '\'')) {
            char q = s[i++]; std::string val;
            while (i < s.size() && s[i] != q) {
                if (s[i] == '\\' && i + 1 < s.size()) { val += s[i + 1]; i += 2; }
                else val += s[i++];
            }
            if (i < s.size()) i++;
            out[key] = val;
        } else if (i < s.size() && (s[i] == '[' || s[i] == '{')) {
            char open = s[i]; char close = (open == '[') ? ']' : '}';
            int depth = 0; size_t vs = i;
            while (i < s.size()) {
                if (s[i] == open) depth++;
                else if (s[i] == close) { depth--; if (depth == 0) { i++; break; } }
                else if (s[i] == '"') { i++; while (i < s.size() && s[i] != '"') i++; }
                i++;
            }
            std::string rawv = s.substr(vs, i - vs);
            try { out[key] = nlohmann::json::parse(rawv); } catch (...) { out[key] = rawv; }
        } else {
            size_t vs = i; int depth = 0;
            while (i < s.size() && !(s[i] == ',' && depth == 0)) {
                if (s[i] == '[' || s[i] == '{') depth++;
                if (s[i] == ']' || s[i] == '}') depth--;
                i++;
            }
            std::string rawv = s.substr(vs, i - vs);
            while (!rawv.empty() && std::isspace((unsigned char)rawv.back())) rawv.pop_back();
            if (rawv == "true") out[key] = true;
            else if (rawv == "false") out[key] = false;
            else { try { out[key] = std::stod(rawv); } catch (...) { out[key] = rawv; } }
        }
        skip_ws();
        if (i < s.size() && s[i] == ',') i++;
    }
    return out;
}

static bool extract_text_tool_call(const std::string& content, std::string& tool_name, nlohmann::json& args) {
    static const char* candidates[] = {
        "create_meeting", "create_event", "add_event", "schedule_event", "create_appointment",
        "create_task", "add_task", "create_todo", "create_reminder",
        "list_events", "get_events", "show_events", "list_tasks",
        "update_event", "move_event", "reschedule_event", "delete_event", "remove_event",
        "create_note", "add_note", "write_note", "list_notes", "get_note", "read_note",
        "update_note", "edit_note", "delete_note", "remove_note", "get_current_time", nullptr };
    std::string lower = to_lower(content);
    for (int c = 0; candidates[c]; c++) {
        std::string pat = std::string(candidates[c]) + "(";
        size_t pos = lower.find(pat);
        if (pos == std::string::npos) continue;
        size_t args_start = pos + pat.size();
        int depth = 1; size_t i = args_start;
        while (i < content.size() && depth > 0) {
            char ch = content[i];
            if (ch == '(') depth++;
            else if (ch == ')') depth--;
            else if (ch == '"') { i++; while (i < content.size() && content[i] != '"') i++; }
            i++;
        }
        tool_name = normalize_tool_name(candidates[c]);
        args = parse_kwargs(content.substr(args_start, (i - 1) - args_start));
        return true;
    }
    return false;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t total = size * nmemb; out->append(static_cast<char*>(contents), total); return total;
}

AgentLoop::AgentLoop(const std::string& llama_server_url, const std::string& model_name, bool supports_tools)
    : server_url_(llama_server_url), model_name_(model_name), supports_tools_(supports_tools),
      use_calendar_tools_(true), use_notes_tools_(true) {}

void AgentLoop::set_max_iterations(int max) { max_iterations_ = max; }
void AgentLoop::set_tool_filters(bool use_calendar, bool use_notes) { use_calendar_tools_ = use_calendar; use_notes_tools_ = use_notes; }
void AgentLoop::set_response_format(const nlohmann::json& fmt) { response_format_ = fmt; }
void AgentLoop::set_event_callback(EventCallback cb) { event_cb_ = std::move(cb); } // PATCH A

std::string AgentLoop::build_system_prompt() {
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

    std::string prompt =
        "You are Delta, an offline AI assistant with unified calendar and task management.\n\n"
        "CURRENT TIME: " + std::string(iso_buf) + " (" + std::string(day_buf) + ")\n"
        "TODAY: " + std::string(today_buf) + "\n"
        "TOMORROW: " + std::string(tom_buf) + " (" + std::string(tom_day_buf) + ")\n\n"
        "RULES:\n"
        "1. DATES: Pass dates to tools naturally ('friday 2pm', 'tomorrow 1300'). "
        "Tools auto-resolve. Default 09:00 if no time given. "
        "today = " + std::string(today_buf) + ", tomorrow = " + std::string(tom_buf) + ".\n"
        "2. NEVER ASK -- always act. Use conversation history and context. "
        "If the user says \"the task\", \"it\", or \"the event\", they mean the most recently mentioned item. "
        "Call tools immediately, never ask for info you can look up.\n"
        "3. TYPE: ALWAYS set type='task' for things the user needs to DO (work on, finish, review, prepare, submit, "
        "fix, build, write, read, buy, clean, call, email, send). "
        "Set type='event' ONLY for meetings, appointments, or scheduled gatherings.\n"
        "4. STATUS: done/complete = \"completed\", cancel = \"cancelled\", start/begin = \"in_progress\".\n"
        "5. Brief and friendly responses. Never show IDs or mention tool names.\n"
        "6. REMINDERS: Events remind 15 min before. Tasks default no reminder.\n"
        "7. PRIORITY: set priority for tasks (low/medium/high/urgent). Default medium.\n"
        "8. TOOLS: ONLY use the exact tools provided (create_event, list_events, update_event, delete_event, "
        "get_current_time, create_note, list_notes, get_note, update_note, delete_note). "
        "NEVER write tool calls as plain text.";

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

    if (!events.empty() || !tasks.empty()) {
        prompt += "\n\n--- User's current context ---\n";
        if (!events.empty()) {
            prompt += "Upcoming events this week:\n";
            for (auto& e : events) {
                prompt += "- [id:" + e.value("id", "") + "] " + e.value("title", "") + " at " + e.value("start_time", "");
                if (!e.value("location", "").empty())
                    prompt += " (" + e["location"].get<std::string>() + ")";
                prompt += "\n";
            }
        }
        if (!tasks.empty()) {
            prompt += "Active tasks:\n";
            for (auto& t : tasks) {
                prompt += "- [id:" + t.value("id", "") + "] [" + t.value("priority", "medium") + "] " + t.value("title", "");
                if (!t.value("start_time", "").empty())
                    prompt += " (due: " + t["start_time"].get<std::string>() + ")";
                prompt += " [" + t.value("status", "") + "]\n";
            }
        }
    }

    return prompt;
}

nlohmann::json AgentLoop::build_request_body(const nlohmann::json& messages, nlohmann::json tools, bool stream) {
    nlohmann::json clean_messages = nlohmann::json::array();
    tools = filter_tools_by_config(tools, use_calendar_tools_, use_notes_tools_);
    
    for (const auto& msg : messages) {
        if (!msg.is_object()) continue;
        nlohmann::json clean_msg;
        clean_msg["role"] = msg.value("role", "user");
        if (msg.contains("content") && msg["content"].is_string()) clean_msg["content"] = msg["content"].get<std::string>();
        else if (msg.contains("content") && msg["content"].is_array()) {
            std::string text;
            for (const auto& part : msg["content"]) {
                if (part.is_object() && part.value("type", "") == "text" && part.contains("text")) {
                    if (!text.empty()) text += "\n"; text += part["text"].get<std::string>();
                }
            }
            clean_msg["content"] = text;
        } else clean_msg["content"] = msg.contains("content") && !msg["content"].is_null() ? msg["content"].dump() : std::string("");
        
        // if (msg.contains("tool_call_id")) clean_msg["tool_call_id"] = msg["tool_call_id"];
        // if (msg.contains("tool_calls")) clean_msg["tool_calls"] = msg["tool_calls"];
        // if (msg.contains("name")) clean_msg["name"] = msg["name"];
        // clean_messages.push_back(clean_msg);

        // Only attach tool-specific fields if the role actually requires them
        if (clean_msg["role"] == "tool") {
            if (msg.contains("tool_call_id")) clean_msg["tool_call_id"] = msg["tool_call_id"];
            if (msg.contains("name")) clean_msg["name"] = msg["name"];
        }
        if (clean_msg["role"] == "assistant" && msg.contains("tool_calls")) {
            clean_msg["tool_calls"] = msg["tool_calls"];
        }
        clean_messages.push_back(clean_msg);
    }

    nlohmann::json request_body = {{"messages", clean_messages}, {"stream", stream}, {"model", model_name_}, {"max_tokens", 1024}};
    
    if (!tools.empty()) {
        request_body["tools"] = tools;
        request_body["tool_choice"] = tool_choice_;
        
        bool is_complex = false;
        for (const auto& msg : messages) {
            if (msg.value("role", "") == "user") {
                std::string text = get_text_content(msg);
                if (text.length() > 150 || text.find("plan") != std::string::npos || text.find("step") != std::string::npos ||
                    text.find("complex") != std::string::npos || text.find("analyze") != std::string::npos) {
                    is_complex = true; break;
                }
            }
        }
        request_body["chat_template_kwargs"] = {{"enable_thinking", is_complex}};
        request_body["parallel_tool_calls"] = true; 
    }

    if (!response_format_.empty()) request_body["response_format"] = response_format_;
    
    return request_body;
}

nlohmann::json AgentLoop::call_llm(const nlohmann::json& messages, const nlohmann::json& tools) {
    nlohmann::json request_body = build_request_body(messages, tools, false);

    std::string response_str;
    CURL* curl = curl_easy_init();
    if (!curl) return {{"error", "Failed to init curl"}};

    std::string url = server_url_ + "/v1/chat/completions";
    std::string body = request_body.dump();
    std::cerr << "[delta-agent] call_llm: " << request_body["messages"].size() << " msgs, "
              << (tools.empty() ? "no tools" : std::to_string(tools.size()) + " tools") << ", model=" << model_name_ << std::endl;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[delta-agent] curl error: " << curl_easy_strerror(res) << std::endl;
        return {{"error", std::string("HTTP request failed: ") + curl_easy_strerror(res)}};
    }

    if (http_code >= 400) {
        std::cerr << "[delta-agent] llama-server HTTP " << http_code << " (tools=" << (!tools.empty() ? "yes" : "no")
                  << "): " << response_str.substr(0, 500) << std::endl;
    }

    try {
        return nlohmann::json::parse(response_str);
    } catch (...) {
        std::cerr << "[delta-agent] JSON parse failed, raw: " << response_str.substr(0, 500) << std::endl;
        return {{"error", "Failed to parse LLM response"}};
    }
}

static constexpr size_t kStreamHoldbackBytes = 128;

namespace {

struct SseContext {
    std::string buffer;
    std::string raw;
    std::string content;
    std::string reasoning;
    nlohmann::json tool_calls = nlohmann::json::array();
    std::string finish_reason = "stop";
    std::string pending;
    bool saw_sse = false;
    bool saw_tool_calls = false;
    bool aborted = false;
    size_t forwarded = 0;
    const delta::agent::TokenCallback* forward = nullptr;
    const delta::agent::EventCallback* events = nullptr; // PATCH B
    bool suppress_forward = false;
};

void merge_tool_call_delta(nlohmann::json& acc, const nlohmann::json& fragment) {
    size_t idx = fragment.value("index", 0);
    while (acc.size() <= idx) acc.push_back({{"id", ""}, {"type", "function"}, {"function", {{"name", ""}, {"arguments", ""}}}});
    auto& slot = acc[idx];
    if (fragment.contains("id") && fragment["id"].is_string() && !fragment["id"].get<std::string>().empty()) slot["id"] = fragment["id"];
    if (!fragment.contains("function") || !fragment["function"].is_object()) return;
    const auto& fn = fragment["function"];
    if (fn.contains("name") && fn["name"].is_string()) slot["function"]["name"] = slot["function"]["name"].get<std::string>() + fn["name"].get<std::string>();
    if (fn.contains("arguments") && fn["arguments"].is_string()) slot["function"]["arguments"] = slot["function"]["arguments"].get<std::string>() + fn["arguments"].get<std::string>();
}

bool flush_pending(SseContext* ctx, bool force) {
    if (!ctx->forward) return true;
    while (ctx->pending.size() > (force ? 0 : kStreamHoldbackBytes)) {
        size_t take = force ? ctx->pending.size() : ctx->pending.size() - kStreamHoldbackBytes;
        std::string chunk = ctx->pending.substr(0, take); ctx->pending.erase(0, take);
        if (!(*ctx->forward)(chunk)) { ctx->aborted = true; return false; }
        ctx->forwarded += chunk.size();
    }
    return true;
}

void handle_sse_line(SseContext* ctx, const std::string& line) {
    if (line.rfind("data: ", 0) != 0) return;
    ctx->saw_sse = true;
    std::string payload = line.substr(6);
    if (payload == "[DONE]") return;

    nlohmann::json chunk;
    try { chunk = nlohmann::json::parse(payload); } catch (...) { return; }
    if (!chunk.is_object() || !chunk.contains("choices") || !chunk["choices"].is_array() || chunk["choices"].empty()) return;

    const auto& choice = chunk["choices"][0];
    if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) ctx->finish_reason = choice["finish_reason"].get<std::string>();
    if (!choice.contains("delta") || !choice["delta"].is_object()) return;

    const auto& delta = choice["delta"];
    
    if (delta.contains("reasoning_content") && delta["reasoning_content"].is_string()) {
        const std::string thought = delta["reasoning_content"].get<std::string>();
        ctx->reasoning += thought;
        if (!thought.empty() && ctx->events) {
            (*ctx->events)("reasoning", {{"content", thought}}); // PATCH B
        }
    }

    if (delta.contains("tool_calls") && delta["tool_calls"].is_array() && !delta["tool_calls"].empty()) {
        ctx->saw_tool_calls = true; ctx->pending.clear();
        for (const auto& fragment : delta["tool_calls"]) if (fragment.is_object()) merge_tool_call_delta(ctx->tool_calls, fragment);
    }
    
    if (delta.contains("content") && delta["content"].is_string()) {
        const std::string text = delta["content"].get<std::string>();
        if (text.empty()) return;
        ctx->content += text;
        if (!ctx->suppress_forward) {
            static const char* markers[] = {"TOOL_CALL", "TOOL_CODE", "create_event(", "update_event(",
                                            "delete_event(", "list_events(", "create_note(", "list_notes(",
                                            "create_meeting(", "create_task(", nullptr};
            for (int m = 0; markers[m]; m++) {
                if (ctx->content.find(markers[m]) != std::string::npos) {
                    ctx->suppress_forward = true;
                    ctx->pending.clear();
                    break;
                }
            }
        }
        if (!ctx->saw_tool_calls && !ctx->suppress_forward && ctx->forward) {
            ctx->pending += text;
            flush_pending(ctx, false);
        }
    }
}

size_t sse_write_callback(void* contents, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb; auto* ctx = static_cast<SseContext*>(userdata); if (ctx->aborted) return 0;
    const char* data = static_cast<char*>(contents);
    if (ctx->raw.size() < 4096) ctx->raw.append(data, std::min(total, 4096 - ctx->raw.size()));
    ctx->buffer.append(data, total); size_t start = 0;
    for (size_t i = 0; i < ctx->buffer.size(); i++) {
        if (ctx->buffer[i] != '\n') continue;
        std::string line = ctx->buffer.substr(start, i - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        handle_sse_line(ctx, line); if (ctx->aborted) return 0; start = i + 1;
    }
    ctx->buffer.erase(0, start); return total;
}
} // namespace

nlohmann::json AgentLoop::call_llm_stream(const nlohmann::json& messages, const nlohmann::json& tools,
                                          const TokenCallback& forward, size_t& out_forwarded, bool& out_client_aborted) {
    out_forwarded = 0;
    out_client_aborted = false;

    nlohmann::json request_body = build_request_body(messages, tools, true);

    CURL* curl = curl_easy_init();
    if (!curl) return {{"error", "Failed to init curl"}};

    SseContext ctx;
    if (forward) ctx.forward = &forward;
    if (event_cb_) ctx.events = &event_cb_; // PATCH B

    std::string url = server_url_ + "/v1/chat/completions";
    std::string body = request_body.dump();
    std::cerr << "[delta-agent] call_llm_stream: " << request_body["messages"].size() << " msgs, "
              << (tools.empty() ? "no tools" : std::to_string(tools.size()) + " tools") << ", model=" << model_name_ << std::endl;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 120L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (ctx.aborted) {
        out_forwarded = ctx.forwarded;
        out_client_aborted = true;
        return {{"error", "client disconnected"}};
    }

    if (!ctx.saw_tool_calls && !ctx.suppress_forward) flush_pending(&ctx, true);
    out_forwarded = ctx.forwarded;
    if (ctx.aborted) {
        out_client_aborted = true;
        return {{"error", "client disconnected"}};
    }

    if (res != CURLE_OK) {
        std::cerr << "[delta-agent] curl error (stream): " << curl_easy_strerror(res) << std::endl;
        return {{"error", std::string("HTTP request failed: ") + curl_easy_strerror(res)}};
    }

    if (!ctx.saw_sse) {
        if (http_code >= 400) {
            std::cerr << "[delta-agent] llama-server HTTP " << http_code
                      << " (tools=" << (!tools.empty() ? "yes" : "no") << "): " << ctx.raw.substr(0, 500) << std::endl;
        }
        try {
            return nlohmann::json::parse(ctx.raw);
        } catch (...) {
            std::cerr << "[delta-agent] JSON parse failed, raw: " << ctx.raw.substr(0, 500) << std::endl;
            return {{"error", "Failed to parse LLM response"}};
        }
    }

    nlohmann::json message = {{"role", "assistant"}, {"content", ctx.content}};
    if (!ctx.tool_calls.empty()) message["tool_calls"] = ctx.tool_calls;
    if (!ctx.reasoning.empty()) message["reasoning_content"] = ctx.reasoning;
    return {{"choices", {{{"index", 0}, {"message", message}, {"finish_reason", ctx.finish_reason}}}}};
}

AgentResponse AgentLoop::process(nlohmann::json messages, TokenCallback on_token) {
    auto& registry = ToolRegistry::instance();
    nlohmann::json tools = registry.get_tools_array();

    // PATCH B: Dedicated event callback
    auto emit_event = [&](const std::string& type, const nlohmann::json& data) {
        if (event_cb_) event_cb_(type, data);
    };

    {
        nlohmann::json clean = nlohmann::json::array();
        for (auto& msg : messages) {
            if (!msg.is_object()) continue;
            if (msg.contains("content") && msg["content"].is_array()) {
                std::string text;
                for (auto& part : msg["content"]) {
                    if (part.is_object() && part.value("type", "") == "text" && part.contains("text")) {
                        if (!text.empty()) text += "\n";
                        text += part["text"].get<std::string>();
                    } else if (part.is_string()) {
                        if (!text.empty()) text += "\n";
                        text += part.get<std::string>();
                    }
                }
                msg["content"] = text;
            } else if (msg.contains("content") && msg["content"].is_object()) {
                auto& obj = msg["content"];
                msg["content"] = (obj.contains("text") && obj["text"].is_string()) ? obj["text"].get<std::string>() : obj.dump();
            } else if (!msg.contains("content") || msg["content"].is_null()) {
                msg["content"] = "";
            } else if (!msg["content"].is_string()) {
                msg["content"] = msg["content"].dump();
            }
            clean.push_back(msg);
        }
        messages = clean;
    }

    if (messages.size() > 8) {
        nlohmann::json trimmed = nlohmann::json::array();
        size_t start = 0;
        if (!messages.empty() && messages[0].is_object() && messages[0].value("role", "") == "system") {
            trimmed.push_back(messages[0]);
            start = 1;
        }
        size_t keep_from = messages.size() > 6 ? messages.size() - 6 : start;
        if (keep_from < start) keep_from = start;
        for (size_t i = keep_from; i < messages.size(); i++) {
            trimmed.push_back(messages[i]);
        }
        messages = trimmed;
    }

    std::string agent_prompt;
    try {
        agent_prompt = build_system_prompt();
    } catch (const std::exception& e) {
        return {false, "", 0, std::string("Failed to build system prompt: ") + e.what(), nlohmann::json::array(), 0, false, ""};
    }
    bool found_system = false;
    for (auto& msg : messages) {
        if (!msg.is_object()) continue;
        if (msg.value("role", "") == "system") {
            std::string existing = get_text_content(msg);
            msg["content"] = existing + "\n\n" + agent_prompt;
            found_system = true;
            break;
        }
    }
    if (!found_system) {
        nlohmann::json system_msg = {{"role", "system"}, {"content", agent_prompt}};
        messages.insert(messages.begin(), system_msg);
    }

    int total_tool_calls = 0;
    std::set<std::string> executed_tool_keys;
    std::vector<std::string> write_summaries;

    bool tools_supported = supports_tools_;
    tool_choice_ = "auto";
    bool action_retry_fired = false;
    nlohmann::json executed_tool_calls = nlohmann::json::array();
    size_t forwarded_total = 0;
    
    auto finish = [&](bool ok, const std::string& content, const std::string& error, const std::string& reasoning = "") {
        return AgentResponse{ok, content, total_tool_calls, error, executed_tool_calls, forwarded_total, false, reasoning};
    };
    
    std::cerr << "[delta-agent] v2 process: " << messages.size()
              << " msgs, tools_supported=" << (tools_supported ? "true" : "false") << ", tools_count=" << tools.size() << std::endl;

    // sanitize_tool_messages_for_strict_templates(messages);

    for (int iteration = 0; iteration < max_iterations_; iteration++) {
        write_summaries.clear();
        nlohmann::json active_tools = tools_supported ? tools : nlohmann::json::array();
        nlohmann::json response;
        
        if (on_token) {
            bool may_forward = action_retry_fired || !(user_wants_action(messages) || user_wants_create(messages));
            size_t forwarded = 0;
            bool client_aborted = false;
            response = call_llm_stream(messages, active_tools, may_forward ? on_token : TokenCallback{}, forwarded, client_aborted);
            forwarded_total += forwarded;
            if (client_aborted) {
                AgentResponse aborted = finish(false, "", "client disconnected");
                aborted.client_aborted = true;
                return aborted;
            }
        } else {
            response = call_llm(messages, active_tools);
        }
        if (tool_choice_ == "required") tool_choice_ = "auto";

        if (response.is_object() && response.contains("error")) {
            auto& err = response["error"];
            std::string err_str = err.is_string() ? err.get<std::string>()
                                : err.is_object() ? err.value("message", err.dump()) : err.dump();

            const bool role_error = err_str.find("roles must alternate") != std::string::npos ||
                                    err_str.find("generate parser") != std::string::npos ||
                                    err_str.find("Jinja") != std::string::npos;
            if (role_error) {
                std::cerr << "[delta-agent] strict-template role error — sanitizing history and retrying" << std::endl;
                sanitize_strict_roles(messages);
                size_t fwd2 = 0; bool aborted2 = false;
                if (on_token) {
                    response = call_llm_stream(messages, active_tools, on_token, fwd2, aborted2);
                    forwarded_total += fwd2;
                    if (aborted2) {
                        AgentResponse a = finish(false, "", "client disconnected");
                        a.client_aborted = true;
                        return a;
                    }
                } else {
                    response = call_llm(messages, active_tools);
                }
            }

            if (response.is_object() && response.contains("error") && tools_supported) {
                std::cerr << "[delta-agent] Error with tools enabled: " << err_str << std::endl;
                std::cerr << "[delta-agent] Retrying without tools..." << std::endl;
                tools_supported = false;
                for (auto& msg : messages) {
                    if (msg.is_object() && msg.value("role", "") == "system") {
                        std::string sys = get_text_content(msg);
                        auto pos = sys.find("You can create");
                        if (pos != std::string::npos)
                            sys = sys.substr(0, pos) + "Answer based on the context provided. Keep responses brief and friendly.";
                        msg["content"] = sys;
                        break;
                    }
                }
                response = call_llm(messages, nlohmann::json::array());
                if (response.is_object() && response.contains("error")) {
                    auto& err2 = response["error"];
                    std::string err2_str = err2.is_string() ? err2.get<std::string>()
                                         : err2.is_object() ? err2.value("message", err2.dump()) : err2.dump();
                    std::cerr << "[delta-agent] Error WITHOUT tools too: " << err2_str << std::endl;
                }
            }
        }

        if (!response.is_object()) {
            return finish(false, "", "Unexpected LLM response format: " + response.dump());
        }

        if (response.contains("error")) {
            auto& err = response["error"];
            std::string error_msg = err.is_string() ? err.get<std::string>() : (err.is_object() ? err.value("message", err.dump()) : err.dump());
            return finish(false, "", error_msg);
        }

        if (!response.contains("choices") || !response["choices"].is_array() || response["choices"].empty()) {
            return finish(false, "", "No choices in LLM response");
        }

        auto& choice = response["choices"][0];
        if (!choice.is_object()) {
            return finish(false, "", "Invalid choice format in LLM response");
        }

        std::string finish_reason = choice.value("finish_reason", "stop");
        if (!choice.contains("message") || !choice["message"].is_object()) {
            return finish(false, "", "No message in LLM choice");
        }
        auto& message = choice["message"];

        bool has_tool_calls = message.contains("tool_calls") && message["tool_calls"].is_array() && !message["tool_calls"].empty();
        
        if (has_tool_calls) {
            messages.push_back(message);

            struct PendingToolCall {
                std::string tool_name;
                std::string tool_call_id;
                std::string args_str;
                nlohmann::json arguments;
                std::string dedup_key;
            };

            std::vector<PendingToolCall> pending_calls;

            for (auto& tool_call : message["tool_calls"]) {
                if (!tool_call.is_object() || !tool_call.contains("function") || !tool_call["function"].is_object()) continue;

                std::string tool_name = tool_call["function"].value("name", "");
                if (tool_name.empty()) continue;
                
                std::string tool_call_id = tool_call.value("id", "call_" + std::to_string(total_tool_calls));
                std::string args_str = tool_call["function"].value("arguments", "{}");
                
                nlohmann::json arguments;
                bool parse_error = false;
                try {
                    arguments = nlohmann::json::parse(args_str);
                } catch (const nlohmann::json::parse_error& e) {
                    parse_error = true;
                    std::cerr << "[delta-agent] MODEL FORMATTING ERROR: Failed to parse tool arguments for " 
                              << tool_name << ". Raw args: " << args_str << ". Error: " << e.what() << std::endl;
                }

                if (parse_error) {
                    messages.push_back({{"role", "tool"}, {"tool_call_id", tool_call_id}, 
                                        {"content", nlohmann::json({{"error", "Invalid JSON arguments from model"}}).dump()}});
                    continue;
                }

                std::string validation_error;
                if (!registry.validate_arguments(tool_name, arguments, validation_error)) {
                    std::cerr << "[delta-agent] MODEL FORMATTING ERROR: Schema validation failed for " 
                              << tool_name << ". Error: " << validation_error << ". Args: " << arguments.dump() << std::endl;
                    messages.push_back({{"role", "tool"}, {"tool_call_id", tool_call_id}, 
                                        {"content", nlohmann::json({{"error", validation_error}}).dump()}});
                    continue;
                }

                std::string dedup_key;
                if (tool_name == "create_event") {
                    std::string t = arguments.value("title", "");
                    std::string s = arguments.value("start_time", "");
                    for (auto& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    dedup_key = "create_event:" + t + ":" + s;
                } else if (tool_name == "delete_event") {
                    std::string dk = arguments.value("id", "");
                    if (dk.empty()) dk = arguments.value("title", "");
                    for (auto& c : dk) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    dedup_key = "delete_event:" + dk;
                } else if (tool_name == "update_event") {
                    std::string uk = arguments.value("title", "");
                    for (auto& c : uk) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    dedup_key = "update_event:" + uk;
                }

                if (!dedup_key.empty() && executed_tool_keys.count(dedup_key)) {
                    std::cerr << "[delta-agent] skipping duplicate: " << tool_name << " key=" << dedup_key << std::endl;
                    messages.push_back({{"role", "tool"}, {"tool_call_id", tool_call_id}, 
                                        {"content", R"({"note":"already executed, skipped duplicate"})"}});
                    continue;
                }
                if (!dedup_key.empty()) {
                    executed_tool_keys.insert(dedup_key);
                }

                pending_calls.push_back({tool_name, tool_call_id, args_str, arguments, dedup_key});
            }

            std::vector<std::future<std::pair<std::string, ToolResult>>> futures;
            for (const auto& pt : pending_calls) {
                emit_event("tool_update", {{"tool_name", pt.tool_name}, {"status", "start"}}); // PATCH B
                
                futures.push_back(std::async(std::launch::async, [&registry, pt]() {
                    return std::make_pair(pt.tool_name, registry.execute(pt.tool_name, pt.arguments));
                }));
            }

            for (size_t i = 0; i < futures.size(); ++i) {
                auto [tool_name, result] = futures[i].get();
                const auto& pt = pending_calls[i];
                
                total_tool_calls++;
                executed_tool_calls.push_back({{"name", tool_name}, {"arguments", pt.args_str}});

                std::string content = result.success ? result.content : nlohmann::json({{"error", result.error_message}}).dump();
                messages.push_back({{"role", "tool"}, {"tool_call_id", pt.tool_call_id}, {"content", content}});

                bool is_write_tool = tool_name.find("create") != std::string::npos ||
                                     tool_name.find("delete") != std::string::npos ||
                                     tool_name.find("update") != std::string::npos;

                if (!result.success) {
                    std::cerr << "[delta-agent] TOOL EXECUTION ERROR: " << tool_name << " failed. Details: " 
                              << result.error_message << std::endl;
                    if (is_write_tool) {
                        return finish(false, "Sorry, I could not do that: " + result.error_message, "");
                    }
                }

                emit_event("tool_update", {{"tool_name", tool_name}, 
                                           {"status", result.success ? "done" : "failed"},
                                           {"success", result.success}}); // PATCH B

                if (result.success && is_write_tool) {
                    std::string summary;
                    try {
                        auto j = nlohmann::json::parse(content);
                        std::string item_type = j.value("type", "event");
                        std::string label = (item_type == "task") ? "task" : "event";
                        if (tool_name == "create_event") {
                            summary = "Created " + label + " \"" + j.value("title", "") + "\"";
                            if (!j.value("start_time", "").empty()) summary += " on " + j.value("start_time", "");
                            summary += ".";
                            if (j.contains("overlap_note")) summary += " " + j.value("overlap_note", "");
                        } else if (tool_name == "delete_event") {
                            if (j.contains("matches") && j["matches"].is_array()) {
                                summary = j.value("message", "Multiple items found") + "\n\n";
                                for (auto& m : j["matches"]) {
                                    summary += "- " + m.value("title", "?");
                                    if (!m.value("start_time", "").empty()) summary += " (" + m.value("start_time", "") + ")";
                                    summary += "\n";
                                }
                                summary += "\nPlease specify which one to delete.";
                            } else {
                                summary = "Done! The item has been deleted.";
                            }
                        } else if (tool_name == "update_event") {
                            if (j.contains("matches") && j["matches"].is_array()) {
                                summary = j.value("message", "Multiple items found") + "\n\n";
                                for (auto& m : j["matches"]) {
                                    summary += "- " + m.value("title", "?");
                                    if (!m.value("start_time", "").empty()) summary += " (" + m.value("start_time", "") + ")";
                                    summary += "\n";
                                }
                                summary += "\nPlease specify which one to update.";
                            } else {
                                std::string st = j.value("status", "");
                                if (st == "completed") summary = "Done! \"" + j.value("title", "") + "\" marked as completed.";
                                else if (st == "cancelled") summary = "Cancelled \"" + j.value("title", "") + "\".";
                                else if (st == "in_progress") summary = "Started working on \"" + j.value("title", "") + "\".";
                                else {
                                    summary = "Updated \"" + j.value("title", "") + "\"";
                                    if (!j.value("start_time", "").empty()) summary += " on " + j.value("start_time", "");
                                    if (!j.value("location", "").empty()) summary += " at " + j.value("location", "");
                                    summary += ".";
                                }
                            }
                        } else if (tool_name == "create_note") { // PATCH C
                            summary = "Saved note \"" + j.value("title", "") + "\".";
                        } else if (tool_name == "update_note") { // PATCH C
                            summary = "Updated note \"" + j.value("title", "") + "\".";
                        } else if (tool_name == "delete_note") { // PATCH C
                            summary = "Done! The note has been deleted.";
                        } else {
                            summary = content;
                        }
                    } catch (...) {
                        summary = content;
                    }
                    write_summaries.push_back(summary);
                    std::cerr << "[delta-agent] write tool done: " << summary << std::endl;
                }
            }

            if (!write_summaries.empty()) {
                std::string combined;
                for (size_t i = 0; i < write_summaries.size(); i++) {
                    if (i > 0) combined += "\n";
                    combined += write_summaries[i];
                }
                return finish(true, combined, "");
            }
            continue;
        }

        std::string content = get_text_content(message);
        std::string reasoning = message.value("reasoning_content", "");

        if (!has_tool_calls && !content.empty()) {
            std::string parsed_name;
            nlohmann::json parsed_args;
            if (extract_text_tool_call(content, parsed_name, parsed_args) && registry.has_tool(parsed_name)) {
                std::cerr << "[delta-agent] recovered text tool call: " << parsed_name
                          << "(" << parsed_args.dump() << ")" << std::endl;

                if (parsed_name == "create_event") {
                    if (parsed_args.value("title", "").empty()) {
                        std::string t = parsed_args.value("description", "");
                        if (t.empty()) t = parsed_args.value("name", "");
                        if (t.empty()) t = "Meeting";
                        parsed_args["title"] = t;
                    }
                    if (parsed_args.value("start_time", "").empty()) {
                        std::string st = parsed_args.value("date", "");
                        std::string tm = parsed_args.value("time", "");
                        if (!st.empty() && !tm.empty()) st += " " + tm;
                        else if (st.empty()) st = tm;
                        if (st.empty()) st = "today";
                        parsed_args["start_time"] = st;
                    }
                    if (!parsed_args.contains("type")) parsed_args["type"] = "event";
                }

                std::string validation_error;
                if (registry.validate_arguments(parsed_name, parsed_args, validation_error)) {
                    messages.push_back(message);
                    std::string tool_call_id = "call_text_" + std::to_string(total_tool_calls);

                    auto result = registry.execute(parsed_name, parsed_args);
                    total_tool_calls++;
                    executed_tool_calls.push_back({{"name", parsed_name}, {"arguments", parsed_args.dump()}});

                    std::string tcontent = result.success ? result.content
                                                          : nlohmann::json({{"error", result.error_message}}).dump();
                    messages.push_back({{"role", "tool"}, {"tool_call_id", tool_call_id}, {"content", tcontent}});
                
                    // sanitize_tool_messages_for_strict_templates(messages);

                    bool is_write = parsed_name.find("create") != std::string::npos ||
                                    parsed_name.find("delete") != std::string::npos ||
                                    parsed_name.find("update") != std::string::npos;

                    if (!result.success && is_write) {
                        std::cerr << "[delta-agent] TOOL EXECUTION ERROR: " << parsed_name
                                  << " failed. Details: " << result.error_message << std::endl;
                        return finish(false, "Sorry, I could not do that: " + result.error_message, "");
                    }
                    if (result.success && is_write) {
                        std::string summary = "Done!";
                        try {
                            auto j = nlohmann::json::parse(tcontent);
                            if (parsed_name == "create_event") {
                                std::string label = (j.value("type", "event") == "task") ? "task" : "event";
                                summary = "Created " + label + " \"" + j.value("title", "") + "\"";
                                if (!j.value("start_time", "").empty()) summary += " on " + j.value("start_time", "");
                                summary += ".";
                            } else if (parsed_name == "create_note") { // PATCH C
                                summary = "Saved note \"" + j.value("title", "") + "\".";
                            } else if (parsed_name == "update_note") { // PATCH C
                                summary = "Updated note \"" + j.value("title", "") + "\".";
                            } else if (parsed_name == "delete_note") { // PATCH C
                                summary = "Done! The note has been deleted.";
                            } else if (parsed_name.find("update") == 0) {
                                summary = "Updated \"" + j.value("title", "") + "\".";
                            } else if (parsed_name.find("delete") == 0) {
                                summary = "Done! The item has been deleted.";
                            }
                        } catch (...) {}
                        std::cerr << "[delta-agent] write tool done: " << summary << std::endl;
                        return finish(true, summary, "");
                    }
                    continue;
                } else {
                    std::cerr << "[delta-agent] recovered call failed validation: " << validation_error << std::endl;
                }
            }
        }

        if (!action_retry_fired) {
            const std::string last_user = last_user_text(messages);
            const std::string user_lower = to_lower(last_user);
            if (user_wants_action(messages)) {
                std::string recent_title, recent_time, recent_id;

                for (int i = static_cast<int>(messages.size()) - 1; i >= 0; i--) {
                    if (messages[i].value("role", "") != "assistant") continue;
                    std::string asst = get_text_content(messages[i]);
                    auto q1 = asst.find('"');
                    if (q1 == std::string::npos) continue;
                    auto q2 = asst.find('"', q1 + 1);
                    if (q2 == std::string::npos) continue;
                    recent_title = asst.substr(q1 + 1, q2 - q1 - 1);
                    auto on_pos = asst.find(" on ");
                    if (on_pos != std::string::npos) {
                        recent_time = asst.substr(on_pos + 4);
                        while (!recent_time.empty() && (recent_time.back() == '.' || recent_time.back() == ' '))
                            recent_time.pop_back();
                    }
                    break;
                }

                if (recent_title.empty()) {
                    auto& db = AgentDatabase::instance();
                    std::vector<nlohmann::json> items;
                    auto upcoming = db.list_events("", "", 10, "", "upcoming");
                    auto in_prog = db.list_events("", "", 5, "", "in_progress");
                    items.insert(items.end(), upcoming.begin(), upcoming.end());
                    items.insert(items.end(), in_prog.begin(), in_prog.end());

                    time_t now_t = time(nullptr);
                    struct tm t_now{};
                    local_time(&now_t, &t_now);
                    char today_s[16], week_s[16];
                    strftime(today_s, sizeof(today_s), "%Y-%m-%d", &t_now);
                    time_t week_t = now_t + 7 * 24 * 3600;
                    struct tm t_week{};
                    local_time(&week_t, &t_week);
                    strftime(week_s, sizeof(week_s), "%Y-%m-%d", &t_week);
                    auto week_events = db.list_events(std::string(today_s), std::string(week_s), 5, "event");
                    items.insert(items.end(), week_events.begin(), week_events.end());

                    int best_score = 0;
                    for (auto& item : items) {
                        std::string title = item.value("title", "");
                        std::string title_lower = title;
                        for (auto& c : title_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                        int score = 0;
                        size_t ws = 0;
                        while (ws < title_lower.size()) {
                            size_t we = title_lower.find(' ', ws);
                            if (we == std::string::npos) we = title_lower.size();
                            if (we - ws >= 3) {
                                std::string word = title_lower.substr(ws, we - ws);
                                if (user_lower.find(word) != std::string::npos) score++;
                            }
                            ws = we + 1;
                        }

                        if (score > best_score) {
                            best_score = score;
                            recent_title = title;
                            recent_time = item.value("start_time", "");
                            recent_id = item.value("id", "");
                        }
                    }

                    if (best_score == 0 && !items.empty()) {
                        bool has_ref = user_lower.find("the task") != std::string::npos ||
                                       user_lower.find("the event") != std::string::npos ||
                                       user_lower.find("this task") != std::string::npos ||
                                       user_lower.find("that task") != std::string::npos ||
                                       user_lower.find("the meeting") != std::string::npos;
                        if (!has_ref) {
                            size_t pos = 0;
                            while ((pos = user_lower.find("it", pos)) != std::string::npos) {
                                bool start_ok = (pos == 0 || !std::isalpha(static_cast<unsigned char>(user_lower[pos - 1])));
                                bool end_ok = (pos + 2 >= user_lower.size() || !std::isalpha(static_cast<unsigned char>(user_lower[pos + 2])));
                                if (start_ok && end_ok) { has_ref = true; break; }
                                pos++;
                            }
                        }
                        if (has_ref) {
                            recent_title = items[0].value("title", "");
                            recent_time = items[0].value("start_time", "");
                            recent_id = items[0].value("id", "");
                        }
                    }
                }

                if (!recent_title.empty()) {
                    std::cerr << "[delta-agent] action retry: forcing tool with context: " << recent_title << " (id=" << recent_id << ")" << std::endl;
                    action_retry_fired = true;
                    messages.push_back(message);
                    std::string hint = "I mean \"" + recent_title + "\"";
                    if (!recent_id.empty()) hint += " (id: " + recent_id + ")";
                    if (!recent_time.empty()) hint += " currently at " + recent_time;

                    bool has_date_ref = user_lower.find("tomorrow") != std::string::npos || user_lower.find("today") != std::string::npos;
                    if (!has_date_ref) {
                        static const char* day_names[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
                        for (int d = 0; d < 7; d++) {
                            if (user_lower.find(day_names[d]) != std::string::npos) { has_date_ref = true; break; }
                        }
                    }
                    if (has_date_ref) {
                        std::string resolved = resolve_datetime(last_user);
                        if (resolved.size() >= 16 && resolved.substr(11, 5) == "09:00" && !recent_time.empty() && recent_time.size() >= 16) {
                            bool user_has_time = false;
                            for (size_t pi = 0; pi < user_lower.size() && !user_has_time; pi++) {
                                if (!std::isdigit(static_cast<unsigned char>(user_lower[pi]))) continue;
                                size_t k = pi + 1;
                                if (k < user_lower.size() && std::isdigit(static_cast<unsigned char>(user_lower[k]))) k++;
                                while (k < user_lower.size() && user_lower[k] == ' ') k++;
                                if (k + 1 < user_lower.size() && ((user_lower[k] == 'a' && user_lower[k + 1] == 'm') || (user_lower[k] == 'p' && user_lower[k + 1] == 'm')))
                                    user_has_time = true;
                            }
                            if (!user_has_time) {
                                for (size_t ci = 0; ci + 4 < last_user.size(); ci++) {
                                    if (std::isdigit(static_cast<unsigned char>(last_user[ci])) && std::isdigit(static_cast<unsigned char>(last_user[ci + 1])) &&
                                        last_user[ci + 2] == ':' && std::isdigit(static_cast<unsigned char>(last_user[ci + 3])) && std::isdigit(static_cast<unsigned char>(last_user[ci + 4]))) {
                                        user_has_time = true; break;
                                    }
                                }
                            }
                            if (!user_has_time) {
                                resolved = resolved.substr(0, 11) + recent_time.substr(11, 5);
                            }
                        }
                        hint += ". Set start_time to \"" + resolved + "\"";
                    }

                    hint += ". Do it now.";
                    messages.push_back({{"role", "user"}, {"content", hint}});
                    tool_choice_ = "required";
                    continue;
                }
            }

            if (total_tool_calls == 0 && user_wants_create(messages)) {
                std::cerr << "[delta-agent] create retry: forcing tool call" << std::endl;
                action_retry_fired = true;
                messages.push_back(message);
                messages.push_back({{"role", "user"}, {"content", "Use the calendar tools to do that now. Never ask for a date -- pass it through as the user phrased it and let the tool resolve it. Do it now."}});
                tool_choice_ = "required";
                continue;
            }
        }

        return finish(true, strip_tool_code_blocks(content), "", reasoning);
    }
    return finish(false, "", "Max tool call iterations reached");
}

} // namespace agent
} // namespace delta