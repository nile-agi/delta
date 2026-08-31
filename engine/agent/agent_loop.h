#ifndef DELTA_AGENT_LOOP_H
#define DELTA_AGENT_LOOP_H

#include <functional>
#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

// Receives each content delta as it streams; return false to abort (client disconnected).
using TokenCallback = std::function<bool(const std::string& delta)>;

// NEW: Dedicated channel for agent lifecycle events (tool_update, reasoning).
// These are sent as NAMED SSE events and never rendered as chat text.
using EventCallback = std::function<void(const std::string& event_type, const nlohmann::json& data)>;
struct AgentResponse {
    bool success;
    std::string content;
    int tool_calls_made;
    std::string error;
    nlohmann::json tool_calls = nlohmann::json::array();
    size_t streamed_chars = 0;
    bool client_aborted = false;
    std::string reasoning_content; // ENHANCEMENT C: Stores reasoning/thinking content
};

class AgentLoop {
  public:
    AgentLoop(const std::string& llama_server_url, const std::string& model_name = "default",
              bool supports_tools = true);

    AgentResponse process(nlohmann::json messages, TokenCallback on_token = nullptr);
    void set_max_iterations(int max);
    void set_tool_filters(bool use_calendar, bool use_notes);
    void set_response_format(const nlohmann::json& fmt); // ENHANCEMENT A: For JSON Grammar Constraints
    void set_event_callback(EventCallback cb); // NEW

  private:
    std::string server_url_;
    std::string model_name_;
    int max_iterations_ = 5;
    bool supports_tools_ = true;
    std::string tool_choice_ = "required";
    bool use_calendar_tools_ = true;
    bool use_notes_tools_ = true;
    nlohmann::json response_format_; // ENHANCEMENT A
    EventCallback event_cb_; // NEW

    nlohmann::json build_request_body(const nlohmann::json& messages, nlohmann::json tools, bool stream);
    nlohmann::json call_llm(const nlohmann::json& messages, const nlohmann::json& tools);
    nlohmann::json call_llm_stream(const nlohmann::json& messages, const nlohmann::json& tools,
                                   const TokenCallback& forward, size_t& out_forwarded, bool& out_client_aborted);
    std::string build_system_prompt();
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_LOOP_H