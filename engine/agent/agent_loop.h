#ifndef DELTA_AGENT_LOOP_H
#define DELTA_AGENT_LOOP_H

#include <string>
#include "json.hpp"

namespace delta {
namespace agent {

struct AgentResponse {
    bool success;
    std::string content;
    int tool_calls_made;
    std::string error;
};

class AgentLoop {
  public:
    AgentLoop(const std::string& llama_server_url, const std::string& model_name = "default",
              bool supports_tools = true);

    AgentResponse process(nlohmann::json messages);
    void set_max_iterations(int max);

  private:
    std::string server_url_;
    std::string model_name_;
    int max_iterations_ = 3;
    bool supports_tools_ = true;
    std::string tool_choice_ = "required";

    nlohmann::json call_llm(const nlohmann::json& messages, const nlohmann::json& tools);
    std::string build_system_prompt();
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_LOOP_H
