#include "agent_database.h"
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <random>

namespace delta {
namespace agent {

AgentDatabase& AgentDatabase::instance() {
    static AgentDatabase db;
    return db;
}

AgentDatabase::~AgentDatabase() {
    close();
}

bool AgentDatabase::init(const std::string& db_path) {
    if (db_)
        return true;

    if (!db_path.empty()) {
        db_path_ = db_path;
    } else {
        const char* home = std::getenv("HOME");
        if (!home)
            home = std::getenv("USERPROFILE");
        if (!home)
            return false;
        std::string dir = std::string(home) + "/.delta-cli";
        std::filesystem::create_directories(dir);
        db_path_ = dir + "/agent.db";
    }

    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open agent database: " << sqlite3_errmsg(db_) << std::endl;
        db_ = nullptr;
        return false;
    }

    exec_sql("PRAGMA journal_mode=WAL;");
    exec_sql("PRAGMA foreign_keys=ON;");

    return run_migrations();
}

void AgentDatabase::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool AgentDatabase::exec_sql(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << (err ? err : "unknown") << std::endl;
        sqlite3_free(err);
        return false;
    }
    return true;
}

int AgentDatabase::get_schema_version() {
    sqlite3_stmt* stmt;
    int rc =
        sqlite3_prepare_v2(db_, "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1", -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        return 0;

    int version = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        version = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return version;
}

void AgentDatabase::set_schema_version(int version) {
    exec_sql("DELETE FROM schema_version;");
    std::string sql = "INSERT INTO schema_version (version) VALUES (" + std::to_string(version) + ");";
    exec_sql(sql);
}

bool AgentDatabase::run_migrations() {
    exec_sql("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL);");

    int current = get_schema_version();

    if (current < 1) {
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS calendar_events (
                id          TEXT PRIMARY KEY,
                title       TEXT NOT NULL,
                description TEXT DEFAULT '',
                start_time  TEXT NOT NULL,
                end_time    TEXT,
                location    TEXT DEFAULT '',
                all_day     INTEGER DEFAULT 0,
                created_at  TEXT NOT NULL,
                updated_at  TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS tasks (
                id          TEXT PRIMARY KEY,
                title       TEXT NOT NULL,
                description TEXT DEFAULT '',
                status      TEXT DEFAULT 'pending',
                priority    TEXT DEFAULT 'medium',
                due_date    TEXT,
                tags        TEXT DEFAULT '',
                created_at  TEXT NOT NULL,
                updated_at  TEXT NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_events_start ON calendar_events(start_time);
            CREATE INDEX IF NOT EXISTS idx_tasks_status ON tasks(status);
            CREATE INDEX IF NOT EXISTS idx_tasks_due ON tasks(due_date);
        )";
        if (!exec_sql(schema))
            return false;
        set_schema_version(1);
    }

    return true;
}

std::string AgentDatabase::generate_uuid() {
    static std::mutex mtx;
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::lock_guard<std::mutex> lock(mtx);
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (auto& c : uuid) {
        if (c == 'x') {
            c = hex[dis(gen)];
        } else if (c == 'y') {
            c = hex[(dis(gen) & 0x3) | 0x8];
        }
    }
    return uuid;
}

std::string AgentDatabase::get_current_timestamp() {
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", t);
    return std::string(buf);
}

// --- Calendar CRUD ---

std::string AgentDatabase::create_event(const nlohmann::json& data) {
    std::string title = data.value("title", "");
    std::string start_time = data.value("start_time", "");

    // Prevent overlapping events: no two events at the same start_time
    if (!start_time.empty()) {
        sqlite3_stmt* check;
        const char* check_sql = "SELECT id, title FROM calendar_events WHERE start_time = ?";
        if (sqlite3_prepare_v2(db_, check_sql, -1, &check, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(check, 1, start_time.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(check) == SQLITE_ROW) {
                std::string existing_title = reinterpret_cast<const char*>(sqlite3_column_text(check, 1));
                sqlite3_finalize(check);
                std::cerr << "[delta-db] time conflict: \"" << existing_title << "\" already at " << start_time
                          << std::endl;
                return "";
            }
            sqlite3_finalize(check);
        }
    }

    std::string id = generate_uuid();
    std::string now = get_current_timestamp();

    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO calendar_events (id, title, description, start_time, end_time, location, all_day, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return "";

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, data.value("description", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, start_time.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, data.value("end_time", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, data.value("location", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, data.value("all_day", false) ? 1 : 0);
    sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? id : "";
}

nlohmann::json AgentDatabase::row_to_event(sqlite3_stmt* stmt) {
    auto col = [&](int i) -> std::string {
        const unsigned char* txt = sqlite3_column_text(stmt, i);
        return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
    };
    return {{"id", col(0)},
            {"title", col(1)},
            {"description", col(2)},
            {"start_time", col(3)},
            {"end_time", col(4)},
            {"location", col(5)},
            {"all_day", sqlite3_column_int(stmt, 6) != 0},
            {"created_at", col(7)},
            {"updated_at", col(8)}};
}

nlohmann::json AgentDatabase::get_event(const std::string& id) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, title, description, start_time, end_time, location, all_day, created_at, updated_at "
                      "FROM calendar_events WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return nullptr;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    nlohmann::json result = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_event(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<nlohmann::json> AgentDatabase::list_events(const std::string& start, const std::string& end, int limit) {
    std::string sql = "SELECT id, title, description, start_time, end_time, location, all_day, created_at, updated_at "
                      "FROM calendar_events";
    std::vector<std::string> conditions;
    if (!start.empty())
        conditions.push_back("date(start_time) >= date(?)");
    if (!end.empty())
        conditions.push_back("date(start_time) <= date(?)");
    if (!conditions.empty()) {
        sql += " WHERE ";
        for (size_t i = 0; i < conditions.size(); i++) {
            if (i > 0)
                sql += " AND ";
            sql += conditions[i];
        }
    }
    sql += " ORDER BY start_time ASC LIMIT ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    int idx = 1;
    if (!start.empty())
        sqlite3_bind_text(stmt, idx++, start.c_str(), -1, SQLITE_TRANSIENT);
    if (!end.empty())
        sqlite3_bind_text(stmt, idx++, end.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx, limit);

    std::vector<nlohmann::json> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(row_to_event(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

bool AgentDatabase::update_event(const std::string& id, const nlohmann::json& data) {
    auto existing = get_event(id);
    if (existing.is_null())
        return false;

    std::string now = get_current_timestamp();
    sqlite3_stmt* stmt;
    const char* sql = R"(
        UPDATE calendar_events SET title=?, description=?, start_time=?, end_time=?, location=?, all_day=?, updated_at=?
        WHERE id=?
    )";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, data.value("title", existing["title"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, data.value("description", existing["description"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, data.value("start_time", existing["start_time"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, data.value("end_time", existing["end_time"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, data.value("location", existing["location"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    bool all_day = data.contains("all_day") ? data["all_day"].get<bool>() : existing["all_day"].get<bool>();
    sqlite3_bind_int(stmt, 6, all_day ? 1 : 0);
    sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool AgentDatabase::delete_event(const std::string& id) {
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM calendar_events WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

// --- Task CRUD ---

std::string AgentDatabase::create_task(const nlohmann::json& data) {
    std::string title = data.value("title", "");

    // Check for duplicate: same title created within last 60 seconds (retry/duplicate)
    if (!title.empty()) {
        sqlite3_stmt* check;
        const char* check_sql = "SELECT id FROM tasks WHERE LOWER(title) = LOWER(?) "
                                "AND created_at > datetime('now', '-60 seconds')";
        if (sqlite3_prepare_v2(db_, check_sql, -1, &check, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(check, 1, title.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(check) == SQLITE_ROW) {
                std::string existing_id = reinterpret_cast<const char*>(sqlite3_column_text(check, 0));
                sqlite3_finalize(check);
                std::cerr << "[delta-db] duplicate task detected (within 60s): " << title << std::endl;
                return existing_id;
            }
            sqlite3_finalize(check);
        }
    }

    std::string id = generate_uuid();
    std::string now = get_current_timestamp();

    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO tasks (id, title, description, status, priority, due_date, tags, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return "";

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, data.value("description", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, data.value("status", "pending").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, data.value("priority", "medium").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, data.value("due_date", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, data.value("tags", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? id : "";
}

nlohmann::json AgentDatabase::row_to_task(sqlite3_stmt* stmt) {
    auto col = [&](int i) -> std::string {
        const unsigned char* txt = sqlite3_column_text(stmt, i);
        return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
    };
    return {{"id", col(0)},     {"title", col(1)},      {"description", col(2)},
            {"status", col(3)}, {"priority", col(4)},   {"due_date", col(5)},
            {"tags", col(6)},   {"created_at", col(7)}, {"updated_at", col(8)}};
}

nlohmann::json AgentDatabase::get_task(const std::string& id) {
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, title, description, status, priority, due_date, tags, created_at, updated_at FROM "
                      "tasks WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return nullptr;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    nlohmann::json result = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_task(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<nlohmann::json> AgentDatabase::list_tasks(const std::string& status, const std::string& priority, int limit,
                                                      const std::string& tags) {
    std::string sql =
        "SELECT id, title, description, status, priority, due_date, tags, created_at, updated_at FROM tasks";
    std::vector<std::string> conditions;
    if (!status.empty())
        conditions.push_back("status = ?");
    if (!priority.empty())
        conditions.push_back("priority = ?");
    if (!tags.empty())
        conditions.push_back("tags LIKE ?");
    if (!conditions.empty()) {
        sql += " WHERE ";
        for (size_t i = 0; i < conditions.size(); i++) {
            if (i > 0)
                sql += " AND ";
            sql += conditions[i];
        }
    }
    sql += " ORDER BY CASE priority WHEN 'urgent' THEN 0 WHEN 'high' THEN 1 WHEN 'medium' THEN 2 WHEN 'low' THEN 3 "
           "END, due_date ASC LIMIT ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    int idx = 1;
    if (!status.empty())
        sqlite3_bind_text(stmt, idx++, status.c_str(), -1, SQLITE_TRANSIENT);
    if (!priority.empty())
        sqlite3_bind_text(stmt, idx++, priority.c_str(), -1, SQLITE_TRANSIENT);
    if (!tags.empty()) {
        std::string pattern = "%" + tags + "%";
        sqlite3_bind_text(stmt, idx++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, idx, limit);

    std::vector<nlohmann::json> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(row_to_task(stmt));
    }
    sqlite3_finalize(stmt);
    return results;
}

bool AgentDatabase::update_task(const std::string& id, const nlohmann::json& data) {
    auto existing = get_task(id);
    if (existing.is_null())
        return false;

    std::string now = get_current_timestamp();
    sqlite3_stmt* stmt;
    const char* sql = R"(
        UPDATE tasks SET title=?, description=?, status=?, priority=?, due_date=?, tags=?, updated_at=?
        WHERE id=?
    )";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, data.value("title", existing["title"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, data.value("description", existing["description"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, data.value("status", existing["status"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, data.value("priority", existing["priority"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, data.value("due_date", existing["due_date"].get<std::string>()).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, data.value("tags", existing["tags"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, id.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool AgentDatabase::complete_task(const std::string& id) {
    return update_task(id, {{"status", "completed"}});
}

bool AgentDatabase::delete_task(const std::string& id) {
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM tasks WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

} // namespace agent
} // namespace delta
