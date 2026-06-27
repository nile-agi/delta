#include "agent_loop.h"
#include "agent_database.h"
#include "tool_registry.h"
#include <ctime>
#include <curl/curl.h>
#include <iostream>
#include <sstream>

namespace delta {
namespace agent {

static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t total = size * nmemb;
    out->append(static_cast<char*>(contents), total);
    return total;
}

AgentLoop::AgentLoop(const std::string& llama_server_url) : server_url_(llama_server_url) {}

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
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%A, %B %d, %Y at %H:%M", &t_local);

    std::string prompt =
        "You are Delta, an offline AI assistant with access to tools for managing "
        "the user's calendar and tasks. Today is " +
        std::string(date_buf) +
        ".\n\n"
        "When the user asks about scheduling, events, meetings, appointments, tasks, "
        "to-do items, or reminders, use the appropriate tool.\n"
        "After using a tool, summarize what you did for the user.\n"
        "If the user's request is not related to calendar or tasks, respond normally without using tools.";

    // Inject context summary of upcoming events and pending tasks
    auto& db = AgentDatabase::instance();

    char today_buf[16], week_buf[16];
    strftime(today_buf, sizeof(today_buf), "%Y-%m-%d", &t_local);
    time_t week_later = now + 7 * 24 * 3600;
    struct tm wt_local{};
#ifdef _WIN32
    localtime_s(&wt_local, &week_later);
#else
    localtime_r(&week_later, &wt_local);
#endif
    strftime(week_buf, sizeof(week_buf), "%Y-%m-%d", &wt_local);

    auto events = db.list_events(std::string(today_buf), std::string(week_buf), 10);
    auto tasks = db.list_tasks("pending", "", 10);
    auto in_progress = db.list_tasks("in_progress", "", 10);
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
                if (!t.value("due_date", "").empty())
                    prompt += " (due: " + t["due_date"].get<std::string>() + ")";
                prompt += " [" + t.value("status", "") + "]\n";
            }
        }
    }

    return prompt;
}

nlohmann::json AgentLoop::call_llm(const nlohmann::json& messages, const nlohmann::json& tools) {
    nlohmann::json request_body = {{"messages", messages}, {"stream", false}};
    if (!tools.empty()) {
        request_body["tools"] = tools;
        request_body["tool_choice"] = "auto";
    }

    std::string response_str;
    CURL* curl = curl_easy_init();
    if (!curl)
        return {{"error", "Failed to init curl"}};

    std::string url = server_url_ + "/v1/chat/completions";
    std::string body = request_body.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return {{"error", std::string("HTTP request failed: ") + curl_easy_strerror(res)}};
    }

    try {
        return nlohmann::json::parse(response_str);
    } catch (...) {
        return {{"error", "Failed to parse LLM response"}};
    }
}

AgentResponse AgentLoop::process(nlohmann::json messages) {
    auto& registry = ToolRegistry::instance();
    nlohmann::json tools = registry.get_tools_array();

    // Ensure agent tool instructions are in the system prompt
    std::string agent_prompt = build_system_prompt();
    bool found_system = false;
    for (auto& msg : messages) {
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

    for (int iteration = 0; iteration < max_iterations_; iteration++) {
        auto response = call_llm(messages, tools);

        if (response.contains("error")) {
            return {false, "", total_tool_calls, response["error"].get<std::string>()};
        }

        if (!response.contains("choices") || response["choices"].empty()) {
            return {false, "", total_tool_calls, "No choices in LLM response"};
        }

        auto& choice = response["choices"][0];
        std::string finish_reason = choice.value("finish_reason", "stop");
        auto& message = choice["message"];

        if (finish_reason == "tool_calls" && message.contains("tool_calls")) {
            // Append the assistant message with tool_calls to conversation
            messages.push_back(message);

            for (auto& tool_call : message["tool_calls"]) {
                std::string tool_name = tool_call["function"]["name"].get<std::string>();
                std::string tool_call_id = tool_call.value("id", "call_" + std::to_string(total_tool_calls));

                nlohmann::json arguments;
                try {
                    std::string args_str = tool_call["function"]["arguments"].get<std::string>();
                    arguments = nlohmann::json::parse(args_str);
                } catch (...) {
                    arguments = nlohmann::json::object();
                }

                auto result = registry.execute(tool_name, arguments);
                total_tool_calls++;

                std::string content =
                    result.success ? result.content : nlohmann::json({{"error", result.error_message}}).dump();

                messages.push_back({{"role", "tool"}, {"tool_call_id", tool_call_id}, {"content", content}});
            }
            continue;
        }

        // Normal text response — we're done
        std::string content = message.value("content", "");
        return {true, content, total_tool_calls, ""};
    }

    return {false, "", total_tool_calls, "Max tool call iterations reached"};
}

} // namespace agent
} // namespace delta
