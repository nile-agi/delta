#include "llm_client.h"
#include <algorithm>
#include <curl/curl.h>
#include <iostream>

namespace delta {
namespace agent {

std::string message_text(const nlohmann::json& msg, const std::string& key) {
    if (!msg.is_object() || !msg.contains(key))
        return "";
    const auto& val = msg.at(key);
    if (val.is_string())
        return val.get<std::string>();
    if (val.is_null())
        return "";
    if (val.is_array()) {
        std::string text;
        for (const auto& part : val) {
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
        return text;
    }
    if (val.is_object() && val.contains("text") && val["text"].is_string())
        return val["text"].get<std::string>();
    return val.dump();
}

namespace {

size_t collect_body(void* contents, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userdata)->append(static_cast<char*>(contents), total);
    return total;
}

struct SseContext {
    std::string buffer; // partial line carried across curl callbacks
    std::string raw;    // bounded copy of the body, for non-SSE error responses
    std::string content;
    nlohmann::json tool_calls = nlohmann::json::array();
    std::string finish_reason = "stop";
    bool saw_sse = false;
    bool aborted = false;
    size_t forwarded = 0;
    const ContentCallback* forward = nullptr;
};

// Merge an OpenAI streaming tool_call fragment into the accumulating array (fragments arrive by index).
void merge_tool_call_delta(nlohmann::json& acc, const nlohmann::json& fragment) {
    size_t idx = fragment.value("index", 0);
    while (acc.size() <= idx) {
        acc.push_back({{"id", ""}, {"type", "function"}, {"function", {{"name", ""}, {"arguments", ""}}}});
    }
    auto& slot = acc[idx];
    if (fragment.contains("id") && fragment["id"].is_string() && !fragment["id"].get<std::string>().empty())
        slot["id"] = fragment["id"];
    if (!fragment.contains("function") || !fragment["function"].is_object())
        return;
    const auto& fn = fragment["function"];
    if (fn.contains("name") && fn["name"].is_string())
        slot["function"]["name"] = slot["function"]["name"].get<std::string>() + fn["name"].get<std::string>();
    if (fn.contains("arguments") && fn["arguments"].is_string())
        slot["function"]["arguments"] =
            slot["function"]["arguments"].get<std::string>() + fn["arguments"].get<std::string>();
}

void handle_sse_line(SseContext* ctx, const std::string& line) {
    if (line.rfind("data: ", 0) != 0)
        return;
    ctx->saw_sse = true;
    std::string payload = line.substr(6);
    if (payload == "[DONE]")
        return;

    nlohmann::json chunk;
    try {
        chunk = nlohmann::json::parse(payload);
    } catch (...) {
        return;
    }
    if (!chunk.is_object() || !chunk.contains("choices") || !chunk["choices"].is_array() || chunk["choices"].empty())
        return;

    const auto& choice = chunk["choices"][0];
    if (choice.contains("finish_reason") && choice["finish_reason"].is_string())
        ctx->finish_reason = choice["finish_reason"].get<std::string>();
    if (!choice.contains("delta") || !choice["delta"].is_object())
        return;

    const auto& delta = choice["delta"];
    if (delta.contains("tool_calls") && delta["tool_calls"].is_array() && !delta["tool_calls"].empty()) {
        for (const auto& fragment : delta["tool_calls"]) {
            if (fragment.is_object())
                merge_tool_call_delta(ctx->tool_calls, fragment);
        }
    }
    if (delta.contains("content") && delta["content"].is_string()) {
        const std::string text = delta["content"].get<std::string>();
        if (text.empty())
            return;
        ctx->content += text;
        // Unlike the old agent loop, text is forwarded even when the same turn also produces a tool
        // call: the harness shows the model's reasoning before it acts instead of discarding it.
        if (ctx->forward) {
            if (!(*ctx->forward)(text)) {
                ctx->aborted = true;
                return;
            }
            ctx->forwarded += text.size();
        }
    }
}

size_t sse_write_callback(void* contents, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* ctx = static_cast<SseContext*>(userdata);
    if (ctx->aborted)
        return 0; // makes curl fail with CURLE_WRITE_ERROR

    const char* data = static_cast<char*>(contents);
    if (ctx->raw.size() < 4096)
        ctx->raw.append(data, std::min(total, 4096 - ctx->raw.size()));

    ctx->buffer.append(data, total);
    size_t start = 0;
    for (size_t i = 0; i < ctx->buffer.size(); i++) {
        if (ctx->buffer[i] != '\n')
            continue;
        std::string line = ctx->buffer.substr(start, i - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        handle_sse_line(ctx, line);
        if (ctx->aborted)
            return 0;
        start = i + 1;
    }
    ctx->buffer.erase(0, start);
    return total;
}

} // namespace

LlmClient::LlmClient(std::string server_url, std::string model_name)
    : server_url_(std::move(server_url)), model_name_(std::move(model_name)) {}

nlohmann::json LlmClient::build_request_body(const nlohmann::json& messages, const nlohmann::json& tools,
                                             const std::string& tool_choice, bool stream) const {
    // llama-server wants a plain string content on every message.
    nlohmann::json clean_messages = nlohmann::json::array();
    for (const auto& msg : messages) {
        if (!msg.is_object())
            continue;
        nlohmann::json clean;
        clean["role"] = msg.value("role", "user");
        clean["content"] = message_text(msg);
        // Preserve the fields that make multi-turn tool conversations work.
        if (msg.contains("tool_call_id"))
            clean["tool_call_id"] = msg["tool_call_id"];
        if (msg.contains("tool_calls"))
            clean["tool_calls"] = msg["tool_calls"];
        if (msg.contains("name"))
            clean["name"] = msg["name"];
        clean_messages.push_back(std::move(clean));
    }

    nlohmann::json body = {
        {"messages", clean_messages}, {"stream", stream}, {"model", model_name_}, {"max_tokens", config_.max_tokens}};
    if (config_.temperature >= 0.0)
        body["temperature"] = config_.temperature;
    if (!tools.empty()) {
        body["tools"] = tools;
        body["tool_choice"] = tool_choice;
        body["chat_template_kwargs"] = {{"enable_thinking", config_.enable_thinking}};
    }
    return body;
}

nlohmann::json LlmClient::chat(const nlohmann::json& messages, const nlohmann::json& tools,
                               const std::string& tool_choice) {
    nlohmann::json request_body = build_request_body(messages, tools, tool_choice, false);

    CURL* curl = curl_easy_init();
    if (!curl)
        return {{"error", "Failed to init curl"}};

    std::string response_str;
    std::string url = server_url_ + "/v1/chat/completions";
    std::string body = request_body.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.blocking_timeout_seconds);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return {{"error", std::string("HTTP request failed: ") + curl_easy_strerror(res)}};
    if (http_code >= 400) {
        std::cerr << "[delta-harness] llama-server HTTP " << http_code << ": " << response_str.substr(0, 500)
                  << std::endl;
    }
    try {
        return nlohmann::json::parse(response_str);
    } catch (...) {
        return {{"error", "Failed to parse LLM response"}};
    }
}

nlohmann::json LlmClient::chat_stream(const nlohmann::json& messages, const nlohmann::json& tools,
                                      const std::string& tool_choice, const ContentCallback& on_content,
                                      size_t& out_forwarded, bool& out_client_aborted) {
    out_forwarded = 0;
    out_client_aborted = false;

    nlohmann::json request_body = build_request_body(messages, tools, tool_choice, true);

    CURL* curl = curl_easy_init();
    if (!curl)
        return {{"error", "Failed to init curl"}};

    SseContext ctx;
    if (on_content)
        ctx.forward = &on_content;

    std::string url = server_url_ + "/v1/chat/completions";
    std::string body = request_body.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    // No overall timeout: a long generation is legitimate. Bail only if the stream truly stalls.
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, config_.stall_timeout_seconds);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    out_forwarded = ctx.forwarded;
    if (ctx.aborted) {
        out_client_aborted = true;
        return {{"error", "client disconnected"}};
    }
    if (res != CURLE_OK)
        return {{"error", std::string("HTTP request failed: ") + curl_easy_strerror(res)}};

    // Not an SSE body (e.g. an HTTP error) -- parse it as-is so callers can inspect the error.
    if (!ctx.saw_sse) {
        if (http_code >= 400) {
            std::cerr << "[delta-harness] llama-server HTTP " << http_code << ": " << ctx.raw.substr(0, 500)
                      << std::endl;
        }
        try {
            return nlohmann::json::parse(ctx.raw);
        } catch (...) {
            return {{"error", "Failed to parse LLM response"}};
        }
    }

    // Drop tool-call slots the stream never named -- llama-server rejects a tool_calls entry with an
    // empty function name on the next turn.
    nlohmann::json named_calls = nlohmann::json::array();
    for (const auto& call : ctx.tool_calls) {
        if (call.is_object() && call.contains("function") && call["function"].is_object() &&
            !call["function"].value("name", "").empty()) {
            named_calls.push_back(call);
        }
    }

    nlohmann::json message = {{"role", "assistant"}, {"content", ctx.content}};
    if (!named_calls.empty())
        message["tool_calls"] = named_calls;
    return {{"choices", {{{"index", 0}, {"message", message}, {"finish_reason", ctx.finish_reason}}}}};
}

int LlmClient::probe_context_size() {
    CURL* curl = curl_easy_init();
    if (!curl)
        return 0;
    std::string out;
    std::string url = server_url_ + "/props";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK)
        return 0;
    try {
        auto props = nlohmann::json::parse(out);
        // llama-server exposes it as n_ctx at the top level, or nested under default_generation_settings.
        if (props.contains("n_ctx") && props["n_ctx"].is_number_integer())
            return props["n_ctx"].get<int>();
        if (props.contains("default_generation_settings") && props["default_generation_settings"].is_object()) {
            const auto& gen = props["default_generation_settings"];
            if (gen.contains("n_ctx") && gen["n_ctx"].is_number_integer())
                return gen["n_ctx"].get<int>();
        }
    } catch (...) {
    }
    return 0;
}

int LlmClient::count_tokens(const std::string& text) {
    if (tokenize_supported_ == 0 || text.empty())
        return tokenize_supported_ == 0 ? -1 : 0;

    CURL* curl = curl_easy_init();
    if (!curl)
        return -1;
    std::string out;
    std::string url = server_url_ + "/tokenize";
    std::string body = nlohmann::json{{"content", text}}.dump();
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code >= 400) {
        tokenize_supported_ = 0;
        return -1;
    }
    try {
        auto parsed = nlohmann::json::parse(out);
        if (parsed.contains("tokens") && parsed["tokens"].is_array()) {
            tokenize_supported_ = 1;
            return static_cast<int>(parsed["tokens"].size());
        }
    } catch (...) {
    }
    tokenize_supported_ = 0;
    return -1;
}

} // namespace agent
} // namespace delta
