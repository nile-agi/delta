#include "task_store.h"
#include "agent_database.h"
#include "time_compat.h"
#include <algorithm>
#include <ctime>
#include <random>

namespace delta {
namespace agent {

namespace {

std::string new_id(const char* prefix) {
    static std::mutex id_mutex;
    static std::mt19937 generator(std::random_device{}());
    static std::uniform_int_distribution<> distribution(0, 15);
    static const char* hex = "0123456789abcdef";
    std::lock_guard<std::mutex> lock(id_mutex);
    std::string id(prefix);
    for (int i = 0; i < 16; i++)
        id += hex[distribution(generator)];
    return id;
}

std::string utc_now() {
    time_t now = time(nullptr);
    struct tm utc{};
    utc_time(&now, &utc);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

std::string column_text(sqlite3_stmt* statement, int column) {
    const unsigned char* text = sqlite3_column_text(statement, column);
    return text ? std::string(reinterpret_cast<const char*>(text)) : "";
}

nlohmann::json parse_json(const std::string& text, nlohmann::json fallback) {
    try {
        return nlohmann::json::parse(text);
    } catch (...) {
        return fallback;
    }
}

} // namespace

TaskStore& TaskStore::instance() {
    static TaskStore store;
    return store;
}

void TaskStore::detach() {
    if (!mutex_) {
        db_ = nullptr;
        return;
    }
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    db_ = nullptr;
}

bool TaskStore::init(sqlite3* db) {
    if (!db)
        return false;
    mutex_ = &AgentDatabase::instance().connection_mutex();
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    db_ = db;

    const char* schema = R"(
      CREATE TABLE IF NOT EXISTS agent_tasks (
        id TEXT PRIMARY KEY,
        goal TEXT NOT NULL,
        conversation_id TEXT NOT NULL DEFAULT '',
        status TEXT NOT NULL DEFAULT 'active',
        plan TEXT NOT NULL DEFAULT '[]',
        checkpoint TEXT NOT NULL DEFAULT '',
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS idx_agent_tasks_status_updated ON agent_tasks(status, updated_at DESC);
      CREATE INDEX IF NOT EXISTS idx_agent_tasks_conversation ON agent_tasks(conversation_id, updated_at DESC);

      CREATE TABLE IF NOT EXISTS agent_task_budgets (
        task_id TEXT PRIMARY KEY REFERENCES agent_tasks(id) ON DELETE CASCADE,
        context_window_tokens INTEGER NOT NULL,
        output_reserve_tokens INTEGER NOT NULL,
        tool_schema_tokens INTEGER NOT NULL,
        system_tokens INTEGER NOT NULL,
        summary_tokens INTEGER NOT NULL,
        used_input_tokens INTEGER NOT NULL,
        available_input_tokens INTEGER NOT NULL,
        updated_at TEXT NOT NULL
      );

      CREATE TABLE IF NOT EXISTS agent_task_receipts (
        id TEXT PRIMARY KEY,
        task_id TEXT NOT NULL REFERENCES agent_tasks(id) ON DELETE CASCADE,
        step_id TEXT NOT NULL DEFAULT '',
        tool_name TEXT NOT NULL,
        idempotency_key TEXT NOT NULL,
        status TEXT NOT NULL,
        arguments TEXT NOT NULL DEFAULT '{}',
        result TEXT NOT NULL DEFAULT '{}',
        created_at TEXT NOT NULL
      );
      CREATE UNIQUE INDEX IF NOT EXISTS idx_agent_task_receipts_idempotency
        ON agent_task_receipts(task_id, idempotency_key);
      CREATE INDEX IF NOT EXISTS idx_agent_task_receipts_task_created
        ON agent_task_receipts(task_id, created_at DESC);
    )";
    char* error = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &error) != SQLITE_OK) {
        sqlite3_free(error);
        db_ = nullptr;
        return false;
    }
    return true;
}

std::string TaskStore::create_task(const std::string& goal, const std::string& conversation_id) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || goal.empty())
        return "";
    const std::string id = new_id("task_");
    const std::string now = utc_now();
    sqlite3_stmt* statement = nullptr;
    const char* sql =
        "INSERT INTO agent_tasks (id, goal, conversation_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return "";
    sqlite3_bind_text(statement, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, goal.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, conversation_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, now.c_str(), -1, SQLITE_TRANSIENT);
    const bool created = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return created ? id : "";
}

TaskRecord TaskStore::get_task(const std::string& task_id) const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    TaskRecord task;
    if (!db_ || task_id.empty())
        return task;
    sqlite3_stmt* statement = nullptr;
    const char* sql = "SELECT id, goal, conversation_id, status, plan, checkpoint, created_at, updated_at "
                      "FROM agent_tasks WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return task;
    sqlite3_bind_text(statement, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW) {
        task.id = column_text(statement, 0);
        task.goal = column_text(statement, 1);
        task.conversation_id = column_text(statement, 2);
        task.status = column_text(statement, 3);
        task.plan = parse_json(column_text(statement, 4), nlohmann::json::array());
        task.checkpoint = column_text(statement, 5);
        task.created_at = column_text(statement, 6);
        task.updated_at = column_text(statement, 7);
    }
    sqlite3_finalize(statement);
    if (task.id.empty())
        return task;

    const char* budget_sql = "SELECT context_window_tokens, output_reserve_tokens, tool_schema_tokens, system_tokens, "
                             "summary_tokens, used_input_tokens, available_input_tokens FROM agent_task_budgets "
                             "WHERE task_id = ?";
    if (sqlite3_prepare_v2(db_, budget_sql, -1, &statement, nullptr) != SQLITE_OK)
        return task;
    sqlite3_bind_text(statement, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(statement) == SQLITE_ROW) {
        task.budget.context_window_tokens = sqlite3_column_int(statement, 0);
        task.budget.output_reserve_tokens = sqlite3_column_int(statement, 1);
        task.budget.tool_schema_tokens = sqlite3_column_int(statement, 2);
        task.budget.system_tokens = sqlite3_column_int(statement, 3);
        task.budget.summary_tokens = sqlite3_column_int(statement, 4);
        task.budget.used_input_tokens = sqlite3_column_int(statement, 5);
        task.budget.available_input_tokens = sqlite3_column_int(statement, 6);
    }
    sqlite3_finalize(statement);
    return task;
}

void TaskStore::set_plan(const std::string& task_id, const nlohmann::json& plan) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || task_id.empty() || !plan.is_array())
        return;
    sqlite3_stmt* statement = nullptr;
    const char* sql = "UPDATE agent_tasks SET plan = ?, updated_at = ? WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return;
    const std::string now = utc_now();
    const std::string encoded = plan.dump();
    sqlite3_bind_text(statement, 1, encoded.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
}

void TaskStore::checkpoint(const std::string& task_id, const std::string& summary) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || task_id.empty())
        return;
    sqlite3_stmt* statement = nullptr;
    const char* sql = "UPDATE agent_tasks SET checkpoint = ?, updated_at = ? WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return;
    const std::string now = utc_now();
    sqlite3_bind_text(statement, 1, summary.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
}

void TaskStore::record_budget(const std::string& task_id, const TaskBudget& budget) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || task_id.empty())
        return;
    sqlite3_stmt* statement = nullptr;
    const char* sql = "INSERT INTO agent_task_budgets "
                      "(task_id, context_window_tokens, output_reserve_tokens, tool_schema_tokens, system_tokens, "
                      "summary_tokens, used_input_tokens, available_input_tokens, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) ON CONFLICT(task_id) DO UPDATE SET "
                      "context_window_tokens=excluded.context_window_tokens, "
                      "output_reserve_tokens=excluded.output_reserve_tokens, "
                      "tool_schema_tokens=excluded.tool_schema_tokens, system_tokens=excluded.system_tokens, "
                      "summary_tokens=excluded.summary_tokens, used_input_tokens=excluded.used_input_tokens, "
                      "available_input_tokens=excluded.available_input_tokens, updated_at=excluded.updated_at";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return;
    const std::string now = utc_now();
    sqlite3_bind_text(statement, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, budget.context_window_tokens);
    sqlite3_bind_int(statement, 3, budget.output_reserve_tokens);
    sqlite3_bind_int(statement, 4, budget.tool_schema_tokens);
    sqlite3_bind_int(statement, 5, budget.system_tokens);
    sqlite3_bind_int(statement, 6, budget.summary_tokens);
    sqlite3_bind_int(statement, 7, budget.used_input_tokens);
    sqlite3_bind_int(statement, 8, budget.available_input_tokens);
    sqlite3_bind_text(statement, 9, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
}

std::string TaskStore::record_receipt(const std::string& task_id, const TaskReceipt& receipt) {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    if (!db_ || task_id.empty() || receipt.tool_name.empty() || receipt.idempotency_key.empty())
        return "";
    sqlite3_stmt* statement = nullptr;
    const char* existing_sql = "SELECT id FROM agent_task_receipts WHERE task_id = ? AND idempotency_key = ?";
    if (sqlite3_prepare_v2(db_, existing_sql, -1, &statement, nullptr) != SQLITE_OK)
        return "";
    sqlite3_bind_text(statement, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, receipt.idempotency_key.c_str(), -1, SQLITE_TRANSIENT);
    std::string existing;
    if (sqlite3_step(statement) == SQLITE_ROW)
        existing = column_text(statement, 0);
    sqlite3_finalize(statement);
    if (!existing.empty())
        return existing;

    const std::string id = receipt.id.empty() ? new_id("receipt_") : receipt.id;
    const char* sql = "INSERT INTO agent_task_receipts "
                      "(id, task_id, step_id, tool_name, idempotency_key, status, arguments, result, created_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return "";
    const std::string arguments = receipt.arguments.dump();
    const std::string result = receipt.result.dump();
    const std::string now = utc_now();
    sqlite3_bind_text(statement, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 2, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, receipt.step_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 4, receipt.tool_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 5, receipt.idempotency_key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 6, receipt.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 7, arguments.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 8, result.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 9, now.c_str(), -1, SQLITE_TRANSIENT);
    const bool recorded = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    return recorded ? id : "";
}

std::vector<TaskReceipt> TaskStore::receipts(const std::string& task_id, int limit) const {
    std::lock_guard<std::recursive_mutex> lock(*mutex_);
    std::vector<TaskReceipt> out;
    if (!db_ || task_id.empty() || limit <= 0)
        return out;
    sqlite3_stmt* statement = nullptr;
    const char* sql = "SELECT id, step_id, tool_name, idempotency_key, status, arguments, result "
                      "FROM agent_task_receipts WHERE task_id = ? ORDER BY created_at ASC, id ASC LIMIT ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
        return out;
    sqlite3_bind_text(statement, 1, task_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 2, std::min(limit, 100));
    while (sqlite3_step(statement) == SQLITE_ROW) {
        TaskReceipt receipt;
        receipt.id = column_text(statement, 0);
        receipt.step_id = column_text(statement, 1);
        receipt.tool_name = column_text(statement, 2);
        receipt.idempotency_key = column_text(statement, 3);
        receipt.status = column_text(statement, 4);
        receipt.arguments = parse_json(column_text(statement, 5), nlohmann::json::object());
        receipt.result = parse_json(column_text(statement, 6), nlohmann::json::object());
        out.push_back(std::move(receipt));
    }
    sqlite3_finalize(statement);
    return out;
}

} // namespace agent
} // namespace delta
