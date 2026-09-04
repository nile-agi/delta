#ifndef DELTA_LLM_CLIENT_H
#define DELTA_LLM_CLIENT_H

#include <functional>
#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

// Receives each content delta as it streams; return false to abort (client disconnected).
using ContentCallback = std::function<bool(const std::string& delta)>;

struct LlmConfig {
    int max_tokens = 2048;
    double temperature = -1.0; // < 0 leaves it to the server default
    bool enable_thinking = false;
    long stall_timeout_seconds = 120; // abort only if the stream truly stalls
    long blocking_timeout_seconds = 300;
};

// Thin wrapper over an OpenAI-compatible /v1/chat/completions endpoint (llama-server).
// Knows nothing about tools beyond passing the schemas through and reassembling streamed
// tool_call fragments.
class LlmClient {
  public:
    LlmClient(std::string server_url, std::string model_name);

    void set_config(const LlmConfig& cfg) { config_ = cfg; }
    const LlmConfig& config() const { return config_; }

    // Polled about once a second while a streaming request is in flight, including before the
    // first byte arrives. Returning true aborts the request as if the client had disconnected.
    void set_abort_check(std::function<bool()> check) { abort_check_ = std::move(check); }

    // Blocking call. Returns the parsed response, or {"error": "..."} on failure.
    nlohmann::json chat(const nlohmann::json& messages, const nlohmann::json& tools,
                        const std::string& tool_choice = "auto");

    // Streaming call. Content deltas go to `on_content` as they arrive (pass an empty callback to
    // stream without forwarding). Returns the same response shape as chat().
    nlohmann::json chat_stream(const nlohmann::json& messages, const nlohmann::json& tools,
                               const std::string& tool_choice, const ContentCallback& on_content, size_t& out_forwarded,
                               bool& out_client_aborted);

    // Model context length from GET /props. Returns 0 when unavailable.
    int probe_context_size();
    // Exact token count via POST /tokenize. Returns -1 when the endpoint is unavailable.
    int count_tokens(const std::string& text);

  private:
    nlohmann::json build_request_body(const nlohmann::json& messages, const nlohmann::json& tools,
                                      const std::string& tool_choice, bool stream) const;

    std::string server_url_;
    std::string model_name_;
    LlmConfig config_;
    std::function<bool()> abort_check_;
    int tokenize_supported_ = -1; // -1 unknown, 0 no, 1 yes
};

// Normalizes any message content shape (string, array of parts, object, null) to a plain string.
std::string message_text(const nlohmann::json& msg, const std::string& key = "content");

} // namespace agent
} // namespace delta

#endif // DELTA_LLM_CLIENT_H
