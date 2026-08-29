#ifndef DELTA_AGENT_POLICY_H
#define DELTA_AGENT_POLICY_H

#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include "json.hpp"
#include "tool_registry.h"

namespace delta {
namespace agent {

enum class Decision {
    Allow, // run it now
    Ask,   // pause the run and ask the user
    Deny,  // refuse, and tell the model why
};

// Decides whether a tool call may run. Deliberately permissive: safe and caution tools go through
// untouched so the model can drive a task end to end, and only destructive tools stop for a human.
class Policy {
  public:
    struct Config {
        bool auto_approve_safe = true;
        bool auto_approve_caution = true;
        bool auto_approve_destructive = false;
        // When false the run refuses destructive tools outright instead of asking. Used for
        // non-streaming requests, which have no channel to ask on.
        bool can_ask = true;
        std::set<std::string> blocked_tools;
    };

    // No default argument here: `Config{}` cannot be formed while Config is still being defined
    // inside this class.
    Policy() = default;
    explicit Policy(Config config);

    // `remembered` comes from MemoryStore::get_policy(); pass "" when the user has not answered.
    Decision decide(const ToolDefinition& def) const;

    // Decisions the user made during this run only ("allow once" / "allow for this conversation").
    void allow_for_run(const std::string& tool);
    void deny_for_run(const std::string& tool);

    const Config& config() const { return config_; }
    void set_config(Config c) { config_ = std::move(c); }

  private:
    Config config_;
    std::set<std::string> run_allowed_;
    std::set<std::string> run_denied_;
};

// The outcome of answering an approval, as an HTTP status plus the body to return.
struct ApprovalHttpResult {
    int status;
    nlohmann::json body;
};

// Validates an approval answer and applies it. The HTTP route and its tests both call this, so
// what the tests cover is the same code the server runs.
ApprovalHttpResult answer_approval(const nlohmann::json& request);

// Parks a run while the UI asks the user about a tool call, and wakes it when the answer arrives
// over HTTP. One broker serves every in-flight request.
class ApprovalBroker {
  public:
    static ApprovalBroker& instance();

    // Registers a pending request and returns its id. The caller then emits that id to the client.
    std::string open(const std::string& tool, const nlohmann::json& arguments);

    // Blocks until resolve() is called for `id` or `timeout_seconds` elapses.
    // Returns the decision: "allow", "always", "deny", or "timeout".
    std::string wait(const std::string& id, int timeout_seconds);

    // Called from the HTTP handler. Returns false when the id is unknown or already answered.
    bool resolve(const std::string& id, const std::string& decision);

    // Abandons a pending request (client disconnected).
    void cancel(const std::string& id);

  private:
    ApprovalBroker() = default;

    struct Pending {
        std::string decision;
        bool answered = false;
    };

    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::string, Pending> pending_;
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_POLICY_H
