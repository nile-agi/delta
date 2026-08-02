#include "agent_database.h"
#include "time_compat.h"
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
    std::cerr << "[delta-db] schema version: " << current << std::endl;

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
        current = 1;
    }

    if (current < 2) {
        exec_sql("ALTER TABLE calendar_events ADD COLUMN reminder_minutes INTEGER DEFAULT 15;");
        exec_sql("ALTER TABLE calendar_events ADD COLUMN reminded INTEGER DEFAULT 0;");
        exec_sql("ALTER TABLE calendar_events ADD COLUMN status TEXT DEFAULT 'upcoming';");
        exec_sql("ALTER TABLE tasks ADD COLUMN reminder_minutes INTEGER DEFAULT 0;");
        exec_sql("ALTER TABLE tasks ADD COLUMN reminded INTEGER DEFAULT 0;");
        set_schema_version(2);
        current = 2;
    }

    sqlite3_exec(db_, "ALTER TABLE calendar_events ADD COLUMN status TEXT DEFAULT 'upcoming';", nullptr, nullptr,
                 nullptr);

    if (current < 3) {
        std::cerr << "[delta-db] running migration v3: adding type/priority/tags columns" << std::endl;
        exec_sql("ALTER TABLE calendar_events ADD COLUMN type TEXT DEFAULT 'event';");
        exec_sql("ALTER TABLE calendar_events ADD COLUMN priority TEXT DEFAULT 'medium';");
        exec_sql("ALTER TABLE calendar_events ADD COLUMN tags TEXT DEFAULT '';");

        // Migrate existing tasks into calendar_events
        const char* migrate = R"(
            INSERT INTO calendar_events (id, title, description, start_time, end_time, location,
                all_day, created_at, updated_at, reminder_minutes, reminded, status, type, priority, tags)
            SELECT id, title, description,
                COALESCE(NULLIF(due_date, ''), created_at),
                '',
                '',
                CASE WHEN due_date LIKE '%T%' THEN 0 ELSE 1 END,
                created_at, updated_at, reminder_minutes, reminded,
                CASE status WHEN 'pending' THEN 'upcoming' ELSE status END,
                'task', priority, tags
            FROM tasks;
        )";
        exec_sql(migrate);

        exec_sql("DROP TABLE IF EXISTS tasks;");
        exec_sql("DROP INDEX IF EXISTS idx_tasks_status;");
        exec_sql("DROP INDEX IF EXISTS idx_tasks_due;");
        exec_sql("CREATE INDEX IF NOT EXISTS idx_events_type ON calendar_events(type);");

        set_schema_version(3);
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
    struct tm t{};
    utc_time(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    return std::string(buf);
}

// Column order for all SELECT queries:
// 0:id 1:title 2:description 3:start_time 4:end_time 5:location 6:all_day
// 7:created_at 8:updated_at 9:reminder_minutes 10:reminded 11:status
// 12:type 13:priority 14:tags

static const char* ALL_COLS = "id, title, description, start_time, end_time, location, all_day, "
                              "created_at, updated_at, reminder_minutes, reminded, status, type, priority, tags";

nlohmann::json AgentDatabase::row_to_event(sqlite3_stmt* stmt) {
    auto col = [&](int i) -> std::string {
        const unsigned char* txt = sqlite3_column_text(stmt, i);
        return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
    };
    int col_count = sqlite3_column_count(stmt);
    nlohmann::json result = {{"id", col(0)},
                             {"title", col(1)},
                             {"description", col(2)},
                             {"start_time", col(3)},
                             {"end_time", col(4)},
                             {"location", col(5)},
                             {"all_day", sqlite3_column_int(stmt, 6) != 0},
                             {"created_at", col(7)},
                             {"updated_at", col(8)}};
    if (col_count > 9) {
        result["reminder_minutes"] = sqlite3_column_int(stmt, 9);
        result["reminded"] = sqlite3_column_int(stmt, 10) != 0;
        result["status"] = col(11).empty() ? "upcoming" : col(11);
    }
    if (col_count > 12) {
        result["type"] = col(12).empty() ? "event" : col(12);
        result["priority"] = col(13).empty() ? "medium" : col(13);
        result["tags"] = col(14);
    }
    return result;
}

// --- Calendar CRUD (unified events + tasks) ---

std::string AgentDatabase::create_event(const nlohmann::json& data) {
    std::string title = data.value("title", "");
    std::string start_time = data.value("start_time", "");
    std::string type = data.value("type", "event");

    std::string id = generate_uuid();
    std::string now = get_current_timestamp();

    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO calendar_events (id, title, description, start_time, end_time, location, all_day,
            created_at, updated_at, reminder_minutes, status, type, priority, tags)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[delta-db] create_event prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return "";
    }

    int default_reminder = (type == "task") ? 0 : 15;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, data.value("description", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, start_time.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, data.value("end_time", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, data.value("location", "").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, data.value("all_day", false) ? 1 : 0);
    sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 10, data.value("reminder_minutes", default_reminder));
    sqlite3_bind_text(stmt, 11, data.value("status", "upcoming").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, data.value("priority", "medium").c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, data.value("tags", "").c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "[delta-db] create_event step failed: " << sqlite3_errmsg(db_) << std::endl;
    }
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? id : "";
}

nlohmann::json AgentDatabase::get_event(const std::string& id) {
    sqlite3_stmt* stmt;
    std::string sql = std::string("SELECT ") + ALL_COLS + " FROM calendar_events WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return nullptr;

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    nlohmann::json result = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = row_to_event(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::vector<nlohmann::json> AgentDatabase::list_events(const std::string& start, const std::string& end, int limit,
                                                       const std::string& type, const std::string& status,
                                                       const std::string& priority, const std::string& tags) {
    std::string sql = std::string("SELECT ") + ALL_COLS + " FROM calendar_events";
    std::vector<std::string> conditions;
    if (!start.empty())
        conditions.push_back("date(start_time) >= date(?)");
    if (!end.empty())
        conditions.push_back("date(start_time) <= date(?)");
    if (!type.empty())
        conditions.push_back("type = ?");
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
    sql += " ORDER BY start_time ASC LIMIT ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    int idx = 1;
    if (!start.empty())
        sqlite3_bind_text(stmt, idx++, start.c_str(), -1, SQLITE_TRANSIENT);
    if (!end.empty())
        sqlite3_bind_text(stmt, idx++, end.c_str(), -1, SQLITE_TRANSIENT);
    if (!type.empty())
        sqlite3_bind_text(stmt, idx++, type.c_str(), -1, SQLITE_TRANSIENT);
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
        UPDATE calendar_events SET title=?, description=?, start_time=?, end_time=?, location=?,
            all_day=?, updated_at=?, reminder_minutes=?, reminded=?, status=?, type=?, priority=?, tags=?
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
    int rm = data.contains("reminder_minutes") ? data["reminder_minutes"].get<int>()
                                               : existing.value("reminder_minutes", 15);
    sqlite3_bind_int(stmt, 8, rm);
    bool time_changed = data.contains("start_time") || data.contains("reminder_minutes");
    int reminded = time_changed ? 0 : (existing.value("reminded", false) ? 1 : 0);
    sqlite3_bind_int(stmt, 9, reminded);
    sqlite3_bind_text(stmt, 10, data.value("status", existing.value("status", "upcoming")).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, data.value("type", existing.value("type", "event")).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, data.value("priority", existing.value("priority", "medium")).c_str(), -1,
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, data.value("tags", existing.value("tags", "")).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, id.c_str(), -1, SQLITE_TRANSIENT);

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

// --- Reminders ---

std::vector<nlohmann::json> AgentDatabase::get_upcoming_reminders() {
    std::vector<nlohmann::json> results;

    const char* sql = R"(
        SELECT id, title, start_time, reminder_minutes, type
        FROM calendar_events
        WHERE reminded = 0
          AND reminder_minutes >= 0
          AND start_time != ''
          AND (status IS NULL OR status NOT IN ('completed', 'cancelled'))
          AND datetime(start_time, '-' || reminder_minutes || ' minutes') <= datetime('now', 'localtime')
    )";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto col = [&](int i) -> std::string {
                const unsigned char* txt = sqlite3_column_text(stmt, i);
                return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
            };
            results.push_back({{"type", col(4).empty() ? "event" : col(4)},
                               {"id", col(0)},
                               {"title", col(1)},
                               {"time", col(2)},
                               {"reminder_minutes", sqlite3_column_int(stmt, 3)}});
        }
        sqlite3_finalize(stmt);
    }

    return results;
}

bool AgentDatabase::mark_reminded(const std::string& id) {
    const char* sql = "UPDATE calendar_events SET reminded = 1 WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

} // namespace agent
} // namespace delta
