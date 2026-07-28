#ifndef DELTA_AGENT_LOOP_H
#define DELTA_AGENT_LOOP_H

#include <functional>
#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

// Receives each content delta as it streams; return false to abort (client disconnected).
using TokenCallback = std::function<bool(const std::string& delta)>;

struct AgentResponse {
    bool success;
    std::string content;
    int tool_calls_made;
    std::string error;
    nlohmann::json tool_calls = nlohmann::json::array(); // [{name, arguments}]
    size_t streamed_chars = 0;                           // bytes already handed to the TokenCallback
    bool client_aborted = false;
};

class AgentLoop {
  public:
    AgentLoop(const std::string& llama_server_url, const std::string& model_name = "default",
              bool supports_tools = true);

    AgentResponse process(nlohmann::json messages, TokenCallback on_token = nullptr);
    void set_max_iterations(int max);

  private:
    std::string server_url_;
    std::string model_name_;
    int max_iterations_ = 5;
    bool supports_tools_ = true;
    std::string tool_choice_ = "required";

    nlohmann::json build_request_body(const nlohmann::json& messages, const nlohmann::json& tools, bool stream);
    nlohmann::json call_llm(const nlohmann::json& messages, const nlohmann::json& tools);
    // Streams the reply, forwarding content deltas through `forward` (may be null to stream but not
    // forward). Returns the same response shape as call_llm().
    nlohmann::json call_llm_stream(const nlohmann::json& messages, const nlohmann::json& tools,
                                   const TokenCallback& forward, size_t& out_forwarded, bool& out_client_aborted);
    std::string build_system_prompt();
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_LOOP_H
