#include "policy.h"
#include "memory_store.h"
#include <chrono>
#include <random>

namespace delta {
namespace agent {

namespace {
std::string random_id() {
    static std::mutex mtx;
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    std::lock_guard<std::mutex> lock(mtx);
    std::string id = "apr_";
    for (int i = 0; i < 16; i++)
        id += hex[dis(gen)];
    return id;
}
} // namespace

Policy::Policy(Config config) : config_(std::move(config)) {}

void Policy::allow_for_run(const std::string& tool) {
    run_denied_.erase(tool);
    run_allowed_.insert(tool);
}

void Policy::deny_for_run(const std::string& tool) {
    run_allowed_.erase(tool);
    run_denied_.insert(tool);
}

Decision Policy::decide(const ToolDefinition& def) const {
    if (config_.blocked_tools.count(def.name))
        return Decision::Deny;
    if (run_denied_.count(def.name))
        return Decision::Deny;
    if (run_allowed_.count(def.name))
        return Decision::Allow;

    // A remembered "always allow" from an earlier conversation outranks the risk default;
    // a remembered "never" is honoured the same way.
    const std::string remembered = MemoryStore::instance().get_policy(def.name);
    if (remembered == "allow")
        return Decision::Allow;
    if (remembered == "deny")
        return Decision::Deny;

    switch (def.risk) {
    case ToolRisk::Safe:
        return config_.auto_approve_safe ? Decision::Allow : (config_.can_ask ? Decision::Ask : Decision::Deny);
    case ToolRisk::Caution:
        return config_.auto_approve_caution ? Decision::Allow : (config_.can_ask ? Decision::Ask : Decision::Deny);
    case ToolRisk::Destructive:
        if (config_.auto_approve_destructive)
            return Decision::Allow;
        return config_.can_ask ? Decision::Ask : Decision::Deny;
    }
    return Decision::Ask;
}

ApprovalHttpResult answer_approval(const nlohmann::json& request) {
    auto bad_request = [](const std::string& message) {
        return ApprovalHttpResult{400, {{"error", {{"message", message}, {"type", "invalid_request_error"}}}}};
    };

    if (!request.is_object())
        return bad_request("Request body must be a JSON object");

    const std::string id = request.value("id", "");
    const std::string decision = request.value("decision", "");
    static const std::set<std::string> valid = {"allow", "always", "deny", "never"};
    if (id.empty() || !valid.count(decision))
        return bad_request("id and decision (allow|always|deny|never) are required");

    if (!ApprovalBroker::instance().resolve(id, decision)) {
        return ApprovalHttpResult{
            404,
            {{"error", {{"message", "That approval request is unknown or already answered"}, {"type", "not_found"}}}}};
    }
    return ApprovalHttpResult{200, {{"ok", true}, {"id", id}, {"decision", decision}}};
}

ApprovalBroker& ApprovalBroker::instance() {
    static ApprovalBroker broker;
    return broker;
}

std::string ApprovalBroker::open(const std::string& tool, const nlohmann::json& arguments) {
    (void)tool;
    (void)arguments;
    const std::string id = random_id();
    std::lock_guard<std::mutex> lock(mutex_);
    pending_[id] = Pending{};
    return id;
}

std::string ApprovalBroker::wait(const std::string& id, int timeout_seconds) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = pending_.find(id);
    if (it == pending_.end())
        return "timeout";

    const bool answered = cv_.wait_for(lock, std::chrono::seconds(timeout_seconds), [&] {
        auto cur = pending_.find(id);
        return cur == pending_.end() || cur->second.answered;
    });

    std::string decision = "timeout";
    auto cur = pending_.find(id);
    if (answered && cur != pending_.end() && cur->second.answered)
        decision = cur->second.decision;
    if (cur != pending_.end())
        pending_.erase(cur);
    return decision;
}

bool ApprovalBroker::resolve(const std::string& id, const std::string& decision) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_.find(id);
        if (it == pending_.end() || it->second.answered)
            return false;
        it->second.decision = decision;
        it->second.answered = true;
    }
    cv_.notify_all();
    return true;
}

void ApprovalBroker::cancel(const std::string& id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.erase(id);
    }
    cv_.notify_all();
}

} // namespace agent
} // namespace delta
