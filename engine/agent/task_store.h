#ifndef DELTA_AGENT_TASK_STORE_H
#define DELTA_AGENT_TASK_STORE_H

#include <mutex>
#include <string>
#include <vector>
#include "json.hpp"
#include "sqlite3.h"

namespace delta {
namespace agent {

struct TaskBudget {
    int context_window_tokens = 0;
    int output_reserve_tokens = 0;
    int tool_schema_tokens = 0;
    int system_tokens = 0;
    int summary_tokens = 0;
    int used_input_tokens = 0;
    int available_input_tokens = 0;
};

struct TaskReceipt {
    std::string id;
    std::string step_id;
    std::string tool_name;
    std::string idempotency_key;
    std::string status;
    nlohmann::json arguments = nlohmann::json::object();
    nlohmann::json result = nlohmann::json::object();
};

struct TaskRecord {
    std::string id;
    std::string goal;
    std::string conversation_id;
    std::string status;
    nlohmann::json plan = nlohmann::json::array();
    std::string checkpoint;
    TaskBudget budget;
    std::string created_at;
    std::string updated_at;
};

// Durable model-task state. It shares AgentDatabase's SQLite handle and lock, just like
// MemoryStore, so task checkpoints and tool receipts cannot race calendar or memory writes.
class TaskStore {
  public:
    static TaskStore& instance();

    bool init(sqlite3* db);
    void detach();
    bool ready() const { return db_ != nullptr && mutex_ != nullptr; }

    std::string create_task(const std::string& goal, const std::string& conversation_id);
    TaskRecord get_task(const std::string& task_id) const;
    void set_status(const std::string& task_id, const std::string& status);
    void set_plan(const std::string& task_id, const nlohmann::json& plan);
    void checkpoint(const std::string& task_id, const std::string& summary);
    void record_budget(const std::string& task_id, const TaskBudget& budget);
    std::string record_receipt(const std::string& task_id, const TaskReceipt& receipt);
    TaskReceipt receipt(const std::string& task_id, const std::string& idempotency_key) const;
    std::vector<TaskReceipt> receipts(const std::string& task_id, int limit) const;

  private:
    TaskStore() = default;
    TaskStore(const TaskStore&) = delete;
    TaskStore& operator=(const TaskStore&) = delete;

    sqlite3* db_ = nullptr;
    mutable std::recursive_mutex* mutex_ = nullptr;
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_TASK_STORE_H
