#include "agent_loop.h"
#include "agent_database.h"
#include "tool_registry.h"
#include <cctype>
#include <ctime>
#include <curl/curl.h>
#include <iostream>
#include <set>
#include <vector>

namespace delta {
namespace agent {

static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t total = size * nmemb;
    out->append(static_cast<char*>(contents), total);
    return total;
}

AgentLoop::AgentLoop(const std::string& llama_server_url, const std::string& model_name, bool supports_tools)
    : server_url_(llama_server_url), model_name_(model_name), supports_tools_(supports_tools) {}

void AgentLoop::set_max_iterations(int max) {
    max_iterations_ = max;
}

std::string AgentLoop::build_system_prompt() {
    time_t now = time(nullptr);
    struct tm t_local{};
#ifdef _WIN32
    localtime_s(&t_local, &now);
#else
    localtime_r(&now, &t_local);
#endif
    char today_buf[16], iso_buf[32], day_buf[16];
    strftime(today_buf, sizeof(today_buf), "%Y-%m-%d", &t_local);
    strftime(iso_buf, sizeof(iso_buf), "%Y-%m-%dT%H:%M", &t_local);
    strftime(day_buf, sizeof(day_buf), "%A", &t_local);

    time_t tomorrow_t = now + 24 * 3600;
    struct tm tm_tom{};
#ifdef _WIN32
    localtime_s(&tm_tom, &tomorrow_t);
#else
    localtime_r(&tomorrow_t, &tm_tom);
#endif
    char tom_buf[16], tom_day_buf[16];
    strftime(tom_buf, sizeof(tom_buf), "%Y-%m-%d", &tm_tom);
    strftime(tom_day_buf, sizeof(tom_day_buf), "%A", &tm_tom);

    std::string prompt =
        "You are Delta, an offline AI assistant with unified calendar and task management.\n\n"
        "CURRENT TIME: " +
        std::string(iso_buf) + " (" + std::string(day_buf) +
        ")\n"
        "TODAY: " +
        std::string(today_buf) +
        "\n"
        "TOMORROW: " +
        std::string(tom_buf) + " (" + std::string(tom_day_buf) +
        ")\n\n"
        "RULES:\n"
        "1. DATES: Use the CURRENT TIME above to compute dates. "
        "\"today\" = " +
        std::string(today_buf) + ", \"tomorrow\" = " + std::string(tom_buf) +
        ". "
        "Format as YYYY-MM-DDTHH:MM. Default to 09:00 if no time is given.\n"
        "2. NEVER ASK -- always act. Use conversation history and context. "
        "If the user says \"the task\", \"it\", or \"the event\", they mean the most recently mentioned item. "
        "Call tools immediately, never ask for info you can look up.\n"
        "3. TYPE: ALWAYS set type='task' for things the user needs to DO (work on, finish, review, prepare, submit, "
        "fix, build, write, read, buy, clean, call, email, send). "
        "Set type='event' ONLY for meetings, appointments, or scheduled gatherings.\n"
        "4. STATUS: done/complete = \"completed\", cancel = \"cancelled\", start/begin = \"in_progress\".\n"
        "5. Brief and friendly responses. Never show IDs or mention tool names.\n"
        "6. REMINDERS: Events remind 15 min before. Tasks default no reminder.\n"
        "7. PRIORITY: set priority for tasks (low/medium/high/urgent). Default medium.";

    // Inject context summary of upcoming events and pending tasks
    auto& db = AgentDatabase::instance();

    char week_buf[16];
    time_t week_later = now + 7 * 24 * 3600;
    struct tm wt_local{};
#ifdef _WIN32
    localtime_s(&wt_local, &week_later);
#else
    localtime_r(&week_later, &wt_local);
#endif
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
                prompt += "- " + e.value("title", "") + " at " + e.value("start_time", "");
                if (!e.value("location", "").empty())
                    prompt += " (" + e["location"].get<std::string>() + ")";
                prompt += "\n";
            }
        }
        if (!tasks.empty()) {
            prompt += "Active tasks:\n";
            for (auto& t : tasks) {
                prompt += "- [" + t.value("priority", "medium") + "] " + t.value("title", "");
                if (!t.value("start_time", "").empty())
                    prompt += " (due: " + t["start_time"].get<std::string>() + ")";
                prompt += " [" + t.value("status", "") + "]\n";
            }
        }
    }

    return prompt;
}

nlohmann::json AgentLoop::call_llm(const nlohmann::json& messages, const nlohmann::json& tools) {
    // Ensure every message has a string content field for llama-server
    nlohmann::json clean_messages = nlohmann::json::array();
    for (const auto& msg : messages) {
        if (!msg.is_object())
            continue;
        nlohmann::json clean_msg;
        clean_msg["role"] = msg.value("role", "user");
        // Normalize content to string
        if (msg.contains("content") && msg["content"].is_string()) {
            clean_msg["content"] = msg["content"].get<std::string>();
        } else if (msg.contains("content") && msg["content"].is_array()) {
            std::string text;
            for (const auto& part : msg["content"]) {
                if (part.is_object() && part.value("type", "") == "text" && part.contains("text")) {
                    if (!text.empty())
                        text += "\n";
                    text += part["text"].get<std::string>();
                }
            }
            clean_msg["content"] = text;
        } else {
            clean_msg["content"] =
                msg.contains("content") && !msg["content"].is_null() ? msg["content"].dump() : std::string("");
        }
        // Preserve tool-related fields for multi-turn tool conversations
        if (msg.contains("tool_call_id"))
            clean_msg["tool_call_id"] = msg["tool_call_id"];
        if (msg.contains("tool_calls"))
            clean_msg["tool_calls"] = msg["tool_calls"];
        if (msg.contains("name"))
            clean_msg["name"] = msg["name"];
        clean_messages.push_back(clean_msg);
    }

    nlohmann::json request_body = {
        {"messages", clean_messages}, {"stream", false}, {"model", model_name_}, {"max_tokens", 1024}};
    if (!tools.empty()) {
        request_body["tools"] = tools;
        request_body["tool_choice"] = tool_choice_;
        request_body["chat_template_kwargs"] = {{"enable_thinking", false}};
    }

    std::string response_str;
    CURL* curl = curl_easy_init();
    if (!curl)
        return {{"error", "Failed to init curl"}};

    std::string url = server_url_ + "/v1/chat/completions";
    std::string body = request_body.dump();
    std::cerr << "[delta-agent] call_llm: " << clean_messages.size() << " msgs, "
              << (tools.empty() ? "no tools" : std::to_string(tools.size()) + " tools") << ", model=" << model_name_
              << std::endl;

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

AgentResponse AgentLoop::process(nlohmann::json messages) {
    auto& registry = ToolRegistry::instance();
    nlohmann::json tools = registry.get_tools_array();

    // Normalize content fields: ensure every message has string content
    for (auto& msg : messages) {
        if (!msg.is_object())
            continue;
        if (msg.contains("content") && msg["content"].is_string())
            continue;
        if (!msg.contains("content") || msg["content"].is_null()) {
            msg["content"] = "";
            continue;
        }
        if (msg["content"].is_array()) {
            std::string text;
            for (auto& part : msg["content"]) {
                if (part.is_object() && part.value("type", "") == "text" && part.contains("text")) {
                    if (!text.empty())
                        text += "\n";
                    text += part["text"].get<std::string>();
                } else if (part.is_string()) {
                    if (!text.empty())
                        text += "\n";
                    text += part.get<std::string>();
                }
            }
            msg["content"] = text;
        } else if (msg["content"].is_object()) {
            auto& obj = msg["content"];
            if (obj.contains("text") && obj["text"].is_string()) {
                msg["content"] = obj["text"].get<std::string>();
            } else {
                msg["content"] = obj.dump();
            }
        } else if (msg["content"].is_null()) {
            msg["content"] = "";
        } else {
            msg["content"] = msg["content"].dump();
        }
    }

    // Trim conversation history to avoid slow processing on long conversations.
    // Keep system message (first) + last 6 user/assistant messages.
    if (messages.size() > 8) {
        nlohmann::json trimmed = nlohmann::json::array();
        // Keep first message if it's system
        size_t start = 0;
        if (!messages.empty() && messages[0].is_object() && messages[0].value("role", "") == "system") {
            trimmed.push_back(messages[0]);
            start = 1;
        }
        // Keep last 6 messages
        size_t keep_from = messages.size() > 6 ? messages.size() - 6 : start;
        if (keep_from < start)
            keep_from = start;
        for (size_t i = keep_from; i < messages.size(); i++) {
            trimmed.push_back(messages[i]);
        }
        messages = trimmed;
    }

    // Ensure agent tool instructions are in the system prompt
    std::string agent_prompt;
    try {
        agent_prompt = build_system_prompt();
    } catch (const std::exception& e) {
        return {false, "", 0, std::string("Failed to build system prompt: ") + e.what()};
    }
    bool found_system = false;
    for (auto& msg : messages) {
        if (!msg.is_object())
            continue;
        if (msg.value("role", "") == "system") {
            std::string existing = msg.value("content", "");
            msg["content"] = existing + "\n\n" + agent_prompt;
            found_system = true;
            break;
        }
    }
    if (!found_system) {
        messages.insert(messages.begin(), {{"role", "system"}, {"content", agent_prompt}});
    }

    int total_tool_calls = 0;
    // Track executed tool calls to prevent duplicates across iterations
    std::set<std::string> executed_tool_keys;
    // Collect summaries from write tools within a single batch
    std::vector<std::string> write_summaries;

    bool tools_supported = supports_tools_;
    tool_choice_ = "auto";
    std::cerr << "[delta-agent] v2 process: " << messages.size()
              << " msgs, tools_supported=" << (tools_supported ? "true" : "false") << ", tools_count=" << tools.size()
              << std::endl;

    for (int iteration = 0; iteration < max_iterations_; iteration++) {
        write_summaries.clear();
        auto response = call_llm(messages, tools_supported ? tools : nlohmann::json::array());

        // If llama-server returns any error and tools were sent, retry without tools
        if (response.is_object() && response.contains("error") && tools_supported) {
            auto& err = response["error"];
            std::string err_str = err.is_string()   ? err.get<std::string>()
                                  : err.is_object() ? err.value("message", err.dump())
                                                    : err.dump();
            std::cerr << "[delta-agent] Error with tools enabled: " << err_str << std::endl;
            std::cerr << "[delta-agent] Retrying without tools..." << std::endl;
            tools_supported = false;
            // Update system prompt to remove tool instructions
            for (auto& msg : messages) {
                if (msg.is_object() && msg.value("role", "") == "system") {
                    std::string sys = msg.value("content", "");
                    auto pos = sys.find("You can create");
                    if (pos != std::string::npos) {
                        sys = sys.substr(0, pos) +
                              "Answer based on the context provided. Keep responses brief and friendly.";
                    }
                    msg["content"] = sys;
                    break;
                }
            }
            response = call_llm(messages, nlohmann::json::array());
            if (response.is_object() && response.contains("error")) {
                auto& err2 = response["error"];
                std::string err2_str = err2.is_string()   ? err2.get<std::string>()
                                       : err2.is_object() ? err2.value("message", err2.dump())
                                                          : err2.dump();
                std::cerr << "[delta-agent] Error WITHOUT tools too: " << err2_str << std::endl;
            }
        }

        if (!response.is_object()) {
            return {false, "", total_tool_calls, "Unexpected LLM response format: " + response.dump()};
        }

        if (response.contains("error")) {
            auto& err = response["error"];
            std::string error_msg;
            if (err.is_string()) {
                error_msg = err.get<std::string>();
            } else if (err.is_object()) {
                error_msg = err.value("message", err.dump());
            } else {
                error_msg = err.dump();
            }
            return {false, "", total_tool_calls, error_msg};
        }

        if (!response.contains("choices") || !response["choices"].is_array() || response["choices"].empty()) {
            return {false, "", total_tool_calls, "No choices in LLM response"};
        }

        auto& choice = response["choices"][0];
        if (!choice.is_object()) {
            return {false, "", total_tool_calls, "Invalid choice format in LLM response"};
        }

        std::string finish_reason = choice.value("finish_reason", "stop");

        if (!choice.contains("message") || !choice["message"].is_object()) {
            return {false, "", total_tool_calls, "No message in LLM choice"};
        }
        auto& message = choice["message"];

        bool has_tool_calls =
            message.contains("tool_calls") && message["tool_calls"].is_array() && !message["tool_calls"].empty();
        std::cerr << "[delta-agent] response: finish_reason=" << finish_reason
                  << ", has_tool_calls=" << (has_tool_calls ? "yes" : "no")
                  << ", content_len=" << message.value("content", "").size() << std::endl;

        if (has_tool_calls) {
            messages.push_back(message);

            for (auto& tool_call : message["tool_calls"]) {
                if (!tool_call.is_object())
                    continue;
                if (!tool_call.contains("function") || !tool_call["function"].is_object())
                    continue;

                std::string tool_name = tool_call["function"].value("name", "");
                if (tool_name.empty())
                    continue;
                std::cerr << "[delta-agent] tool_call: " << tool_name << "("
                          << tool_call["function"].value("arguments", "{}") << ")" << std::endl;

                std::string tool_call_id = tool_call.value("id", "call_" + std::to_string(total_tool_calls));

                nlohmann::json arguments;
                std::string args_str;
                try {
                    args_str = tool_call["function"].value("arguments", "{}");
                    arguments = nlohmann::json::parse(args_str);
                } catch (...) {
                    arguments = nlohmann::json::object();
                    args_str = "{}";
                }

                // Build a normalized dedup key for write operations
                // Blocks duplicate calls to the same tool with similar intent
                std::string dedup_key;
                if (tool_name == "create_event") {
                    std::string t = arguments.value("title", "");
                    std::string s = arguments.value("start_time", "");
                    for (auto& c : t)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    dedup_key = "create_event:" + t + ":" + s;
                } else if (tool_name == "delete_event") {
                    std::string dk = arguments.value("id", "");
                    if (dk.empty())
                        dk = arguments.value("title", "");
                    for (auto& c : dk)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    dedup_key = "delete_event:" + dk;
                } else if (tool_name == "update_event") {
                    std::string uk = arguments.value("title", "");
                    for (auto& c : uk)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    dedup_key = "update_event:" + uk;
                } else {
                    dedup_key = "";
                }

                if (!dedup_key.empty() && executed_tool_keys.count(dedup_key)) {
                    std::cerr << "[delta-agent] skipping duplicate: " << tool_name << " key=" << dedup_key << std::endl;
                    messages.push_back({{"role", "tool"},
                                        {"tool_call_id", tool_call_id},
                                        {"content", R"({"note":"already executed, skipped duplicate"})"}});
                    continue;
                }
                if (!dedup_key.empty()) {
                    executed_tool_keys.insert(dedup_key);
                }

                auto result = registry.execute(tool_name, arguments);
                total_tool_calls++;

                std::string content =
                    result.success ? result.content : nlohmann::json({{"error", result.error_message}}).dump();

                messages.push_back({{"role", "tool"}, {"tool_call_id", tool_call_id}, {"content", content}});

                bool is_write_tool = tool_name.find("create") != std::string::npos ||
                                     tool_name.find("delete") != std::string::npos ||
                                     tool_name.find("update") != std::string::npos;

                // If a write tool failed, return error directly instead of model hallucinating success
                if (!result.success && is_write_tool) {
                    std::cerr << "[delta-agent] write tool FAILED: " << result.error_message << std::endl;
                    return {false, "Sorry, I could not do that: " + result.error_message, total_tool_calls, ""};
                }

                // Build a friendly summary for write tools
                if (result.success && is_write_tool) {
                    std::string summary;
                    try {
                        auto j = nlohmann::json::parse(content);
                        std::string item_type = j.value("type", "event");
                        std::string label = (item_type == "task") ? "task" : "event";
                        if (tool_name == "create_event") {
                            summary = "Created " + label + " \"" + j.value("title", "") + "\"";
                            if (!j.value("start_time", "").empty())
                                summary += " on " + j.value("start_time", "");
                            summary += ".";
                            if (j.contains("overlap_note"))
                                summary += " " + j.value("overlap_note", "");
                        } else if (tool_name == "delete_event") {
                            if (j.contains("matches") && j["matches"].is_array()) {
                                summary = j.value("message", "Multiple items found") + "\n\n";
                                for (auto& m : j["matches"]) {
                                    summary += "- " + m.value("title", "?");
                                    if (!m.value("start_time", "").empty())
                                        summary += " (" + m.value("start_time", "") + ")";
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
                                    if (!m.value("start_time", "").empty())
                                        summary += " (" + m.value("start_time", "") + ")";
                                    summary += "\n";
                                }
                                summary += "\nPlease specify which one to update.";
                            } else {
                                std::string st = j.value("status", "");
                                if (st == "completed")
                                    summary = "Done! \"" + j.value("title", "") + "\" marked as completed.";
                                else if (st == "cancelled")
                                    summary = "Cancelled \"" + j.value("title", "") + "\".";
                                else if (st == "in_progress")
                                    summary = "Started working on \"" + j.value("title", "") + "\".";
                                else {
                                    summary = "Updated \"" + j.value("title", "") + "\"";
                                    if (!j.value("start_time", "").empty())
                                        summary += " on " + j.value("start_time", "");
                                    if (!j.value("location", "").empty())
                                        summary += " at " + j.value("location", "");
                                    summary += ".";
                                }
                            }
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

            // If we executed write tools, return combined summary instead of another LLM call
            if (!write_summaries.empty()) {
                std::string combined;
                for (size_t i = 0; i < write_summaries.size(); i++) {
                    if (i > 0)
                        combined += "\n";
                    combined += write_summaries[i];
                }
                return {true, combined, total_tool_calls, ""};
            }
            continue;
        }

        std::string content = message.value("content", "");

        // If model responded with text but user wanted an action, retry with forced context
        if (iteration == 0 && total_tool_calls == 0) {
            std::string last_user;
            for (int i = static_cast<int>(messages.size()) - 1; i >= 0; i--) {
                if (messages[i].value("role", "") == "user") {
                    last_user = messages[i].value("content", "");
                    break;
                }
            }
            std::string user_lower = last_user;
            for (auto& c : user_lower)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            static const char* action_words[] = {"push",    "move",     "reschedule", "change", "update",
                                                 "delete",  "cancel",   "mark",       "done",   "complete",
                                                 "remove",  "postpone", "ahead",      "later",  "earlier",
                                                 "forward", "back",     "start",      nullptr};
            bool wants_action = false;
            for (int k = 0; action_words[k]; k++) {
                if (user_lower.find(action_words[k]) != std::string::npos) {
                    wants_action = true;
                    break;
                }
            }

            if (wants_action) {
                // Find most recent item title from conversation history
                std::string recent_title, recent_time;
                for (int i = static_cast<int>(messages.size()) - 1; i >= 0; i--) {
                    if (messages[i].value("role", "") != "assistant")
                        continue;
                    std::string asst = messages[i].value("content", "");
                    auto q1 = asst.find('"');
                    if (q1 == std::string::npos)
                        continue;
                    auto q2 = asst.find('"', q1 + 1);
                    if (q2 == std::string::npos)
                        continue;
                    recent_title = asst.substr(q1 + 1, q2 - q1 - 1);
                    auto on_pos = asst.find(" on ");
                    if (on_pos != std::string::npos) {
                        recent_time = asst.substr(on_pos + 4);
                        while (!recent_time.empty() && (recent_time.back() == '.' || recent_time.back() == ' '))
                            recent_time.pop_back();
                    }
                    break;
                }

                if (!recent_title.empty()) {
                    std::cerr << "[delta-agent] model dodged action, retrying with context: " << recent_title
                              << std::endl;
                    messages.push_back(message);
                    std::string hint = "I mean \"" + recent_title + "\"";
                    if (!recent_time.empty())
                        hint += " currently at " + recent_time;
                    hint += ". Do it now.";
                    messages.push_back({{"role", "user"}, {"content", hint}});
                    tool_choice_ = "required";
                    continue;
                }
            }
        }

        return {true, content, total_tool_calls, ""};
    }

    return {false, "", total_tool_calls, "Max tool call iterations reached"};
}

} // namespace agent
} // namespace delta
