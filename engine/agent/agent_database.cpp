#include "agent_database.h"
#include "time_compat.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <vector>
#include <string>

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
  if (db_) return true;

  if (!db_path.empty()) {
    db_path_ = db_path;
  } else {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    if (!home) return false;
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
  int rc = sqlite3_prepare_v2(db_, "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1", -1, &stmt, nullptr);
  if (rc != SQLITE_OK) return 0;
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
        id TEXT PRIMARY KEY, title TEXT NOT NULL, description TEXT DEFAULT '',
        start_time TEXT NOT NULL, end_time TEXT, location TEXT DEFAULT '',
        all_day INTEGER DEFAULT 0, created_at TEXT NOT NULL, updated_at TEXT NOT NULL
      );
      CREATE TABLE IF NOT EXISTS tasks (
        id TEXT PRIMARY KEY, title TEXT NOT NULL, description TEXT DEFAULT '',
        status TEXT DEFAULT 'pending', priority TEXT DEFAULT 'medium', due_date TEXT,
        tags TEXT DEFAULT '', created_at TEXT NOT NULL, updated_at TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS idx_events_start ON calendar_events(start_time);
      CREATE INDEX IF NOT EXISTS idx_tasks_status ON tasks(status);
      CREATE INDEX IF NOT EXISTS idx_tasks_due ON tasks(due_date);
    )";
    if (!exec_sql(schema)) return false;
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

  sqlite3_exec(db_, "ALTER TABLE calendar_events ADD COLUMN status TEXT DEFAULT 'upcoming';", nullptr, nullptr, nullptr);

  if (current < 3) {
    std::cerr << "[delta-db] running migration v3" << std::endl;
    exec_sql("ALTER TABLE calendar_events ADD COLUMN type TEXT DEFAULT 'event';");
    exec_sql("ALTER TABLE calendar_events ADD COLUMN priority TEXT DEFAULT 'medium';");
    exec_sql("ALTER TABLE calendar_events ADD COLUMN tags TEXT DEFAULT '';");
    const char* migrate = R"(
      INSERT INTO calendar_events (id, title, description, start_time, end_time, location,
        all_day, created_at, updated_at, reminder_minutes, reminded, status, type, priority, tags)
      SELECT id, title, description, COALESCE(NULLIF(due_date, ''), created_at), '', '',
        CASE WHEN due_date LIKE '%T%' THEN 0 ELSE 1 END, created_at, updated_at,
        reminder_minutes, reminded, CASE status WHEN 'pending' THEN 'upcoming' ELSE status END,
        'task', priority, tags FROM tasks;
    )";
    exec_sql(migrate);
    exec_sql("DROP TABLE IF EXISTS tasks;");
    exec_sql("DROP INDEX IF EXISTS idx_tasks_status;");
    exec_sql("DROP INDEX IF EXISTS idx_tasks_due;");
    exec_sql("CREATE INDEX IF NOT EXISTS idx_events_type ON calendar_events(type);");
    set_schema_version(3);
  }

  if (current < 4) {
    std::cerr << "[delta-db] running migration v4: adding notes table" << std::endl;
    exec_sql(R"(
      CREATE TABLE IF NOT EXISTS notes (
        id TEXT PRIMARY KEY, title TEXT NOT NULL, content TEXT DEFAULT '',
        folder TEXT DEFAULT 'General', tags TEXT DEFAULT '', pinned INTEGER DEFAULT 0,
        created_at TEXT NOT NULL, updated_at TEXT NOT NULL
      );
      CREATE INDEX IF NOT EXISTS idx_notes_folder ON notes(folder);
      CREATE INDEX IF NOT EXISTS idx_notes_updated ON notes(updated_at DESC);
      CREATE INDEX IF NOT EXISTS idx_notes_pinned ON notes(pinned, updated_at DESC);
    )");
    set_schema_version(4);
    current = 4;
  }

  // NEW: Migration v5 for RPC Worker Nodes
  if (current < 5) {
    std::cerr << "[delta-db] running migration v5: adding worker_nodes table" << std::endl;
    exec_sql(R"(
      CREATE TABLE IF NOT EXISTS worker_nodes (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        ip TEXT NOT NULL,
        port INTEGER NOT NULL DEFAULT 50051,
        enabled INTEGER DEFAULT 1,
        created_at INTEGER DEFAULT (strftime('%s', 'now'))
      );
    )");
    set_schema_version(5);
    current = 5;
  }

  return true;
}

std::string AgentDatabase::generate_uuid() {
  static std::mutex mtx;
  static std::mt19937 gen(std::random_device{}());
  static std::uniform_int_distribution<> dis(0, 15);
  static const char* hex = "0123456789abcdef";
  std::lock_guard lock(mtx);
  std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
  for (auto& c : uuid) {
    if (c == 'x') c = hex[dis(gen)];
    else if (c == 'y') c = hex[(dis(gen) & 0x3) | 0x8];
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

static const char* ALL_COLS = "id, title, description, start_time, end_time, location, all_day, "
                              "created_at, updated_at, reminder_minutes, reminded, status, type, priority, tags";

nlohmann::json AgentDatabase::row_to_event(sqlite3_stmt* stmt) {
  auto col = [&](int i) -> std::string {
    const unsigned char* txt = sqlite3_column_text(stmt, i);
    return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
  };
  int col_count = sqlite3_column_count(stmt);
  nlohmann::json result = {{"id", col(0)}, {"title", col(1)}, {"description", col(2)},
                           {"start_time", col(3)}, {"end_time", col(4)}, {"location", col(5)},
                           {"all_day", sqlite3_column_int(stmt, 6) != 0},
                           {"created_at", col(7)}, {"updated_at", col(8)}};
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

std::string AgentDatabase::create_event(const nlohmann::json& data) {
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
  int default_reminder = (data.value("type", "event") == "task") ? 0 : 15;
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, data.value("title", "").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, data.value("description", "").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, data.value("start_time", "").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, data.value("end_time", "").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, data.value("location", "").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 7, data.value("all_day", false) ? 1 : 0);
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 9, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 10, data.value("reminder_minutes", default_reminder));
  sqlite3_bind_text(stmt, 11, data.value("status", "upcoming").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 12, data.value("type", "event").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 13, data.value("priority", "medium").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 14, data.value("tags", "").c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? id : "";
}

nlohmann::json AgentDatabase::get_event(const std::string& id) {
  sqlite3_stmt* stmt;
  std::string sql = std::string("SELECT ") + ALL_COLS + " FROM calendar_events WHERE id = ?";
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  nlohmann::json result = nullptr;
  if (sqlite3_step(stmt) == SQLITE_ROW) result = row_to_event(stmt);
  sqlite3_finalize(stmt);
  return result;
}

std::vector<nlohmann::json> AgentDatabase::list_events(const std::string& start, const std::string& end, int limit,
                                                         const std::string& type, const std::string& status,
                                                         const std::string& priority, const std::string& tags) {
  std::string sql = std::string("SELECT ") + ALL_COLS + " FROM calendar_events";
  std::vector<std::string> conditions;
  if (!start.empty()) conditions.push_back("date(start_time) >= date(?)");
  if (!end.empty()) conditions.push_back("date(start_time) <= date(?)");
  if (!type.empty()) conditions.push_back("type = ?");
  if (!status.empty()) conditions.push_back("status = ?");
  if (!priority.empty()) conditions.push_back("priority = ?");
  if (!tags.empty()) conditions.push_back("tags LIKE ?");
  if (!conditions.empty()) {
    sql += " WHERE ";
    for (size_t i = 0; i < conditions.size(); i++) {
      if (i > 0) sql += " AND ";
      sql += conditions[i];
    }
  }
  sql += " ORDER BY start_time ASC LIMIT ?";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return {};
  int idx = 1;
  if (!start.empty()) sqlite3_bind_text(stmt, idx++, start.c_str(), -1, SQLITE_TRANSIENT);
  if (!end.empty()) sqlite3_bind_text(stmt, idx++, end.c_str(), -1, SQLITE_TRANSIENT);
  if (!type.empty()) sqlite3_bind_text(stmt, idx++, type.c_str(), -1, SQLITE_TRANSIENT);
  if (!status.empty()) sqlite3_bind_text(stmt, idx++, status.c_str(), -1, SQLITE_TRANSIENT);
  if (!priority.empty()) sqlite3_bind_text(stmt, idx++, priority.c_str(), -1, SQLITE_TRANSIENT);
  if (!tags.empty()) {
    std::string pattern = "%" + tags + "%";
    sqlite3_bind_text(stmt, idx++, pattern.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int(stmt, idx, limit);
  std::vector<nlohmann::json> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) results.push_back(row_to_event(stmt));
  sqlite3_finalize(stmt);
  return results;
}

bool AgentDatabase::update_event(const std::string& id, const nlohmann::json& data) {
  auto existing = get_event(id);
  if (existing.is_null()) return false;
  std::string now = get_current_timestamp();
  sqlite3_stmt* stmt;
  const char* sql = R"(
    UPDATE calendar_events SET title=?, description=?, start_time=?, end_time=?, location=?,
      all_day=?, updated_at=?, reminder_minutes=?, reminded=?, status=?, type=?, priority=?, tags=?
    WHERE id=?
  )";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, data.value("title", existing["title"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, data.value("description", existing["description"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, data.value("start_time", existing["start_time"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, data.value("end_time", existing["end_time"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, data.value("location", existing["location"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  bool all_day = data.contains("all_day") ? data["all_day"].get<bool>() : existing["all_day"].get<bool>();
  sqlite3_bind_int(stmt, 6, all_day ? 1 : 0);
  sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
  int rm = data.contains("reminder_minutes") ? data["reminder_minutes"].get<int>() : existing.value("reminder_minutes", 15);
  sqlite3_bind_int(stmt, 8, rm);
  bool time_changed = data.contains("start_time") || data.contains("reminder_minutes");
  int reminded = time_changed ? 0 : (existing.value("reminded", false) ? 1 : 0);
  sqlite3_bind_int(stmt, 9, reminded);
  sqlite3_bind_text(stmt, 10, data.value("status", existing.value("status", "upcoming")).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 11, data.value("type", existing.value("type", "event")).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 12, data.value("priority", existing.value("priority", "medium")).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 13, data.value("tags", existing.value("tags", "")).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 14, id.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool AgentDatabase::delete_event(const std::string& id) {
  sqlite3_stmt* stmt;
  const char* sql = "DELETE FROM calendar_events WHERE id = ?";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

std::vector<nlohmann::json> AgentDatabase::get_upcoming_reminders() {
  std::vector<nlohmann::json> results;
  const char* sql = R"(
    SELECT id, title, start_time, reminder_minutes, type FROM calendar_events
    WHERE reminded = 0 AND reminder_minutes >= 0 AND start_time != ''
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
      results.push_back({{"type", col(4).empty() ? "event" : col(4)}, {"id", col(0)}, {"title", col(1)},
                         {"time", col(2)}, {"reminder_minutes", sqlite3_column_int(stmt, 3)}});
    }
    sqlite3_finalize(stmt);
  }
  return results;
}

bool AgentDatabase::mark_reminded(const std::string& id) {
  const char* sql = "UPDATE calendar_events SET reminded = 1 WHERE id = ?";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

// --- Notes CRUD ---

std::string AgentDatabase::create_note(const nlohmann::json& data) {
  std::string id = generate_uuid();
  std::string now = get_current_timestamp();
  sqlite3_stmt* stmt;
  const char* sql = R"(
    INSERT INTO notes (id, title, content, folder, tags, pinned, created_at, updated_at)
    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
  )";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "[delta-db] create_note prepare failed: " << sqlite3_errmsg(db_) << std::endl;
    return "";
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, data.value("title", "Untitled").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, data.value("content", "").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, data.value("folder", "General").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, data.value("tags", "").c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 6, data.value("pinned", false) ? 1 : 0);
  sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return (rc == SQLITE_DONE) ? id : "";
}

nlohmann::json AgentDatabase::get_note(const std::string& id) {
  sqlite3_stmt* stmt;
  const char* sql = "SELECT id, title, content, folder, tags, pinned, created_at, updated_at FROM notes WHERE id = ?";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return nullptr;
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  nlohmann::json result = nullptr;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    auto col = [&](int i) -> std::string {
      const unsigned char* txt = sqlite3_column_text(stmt, i);
      return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
    };
    result = {{"id", col(0)}, {"title", col(1)}, {"content", col(2)}, {"folder", col(3)},
              {"tags", col(4)}, {"pinned", sqlite3_column_int(stmt, 5) != 0},
              {"created_at", col(6)}, {"updated_at", col(7)}};
  }
  sqlite3_finalize(stmt);
  return result;
}

std::vector<nlohmann::json> AgentDatabase::list_notes(const std::string& folder,
                                                       const std::string& search,
                                                       const std::string& tags,
                                                       int limit,
                                                       bool pinned_only) {
  std::string sql = "SELECT id, title, content, folder, tags, pinned, created_at, updated_at FROM notes WHERE 1=1";
  std::vector<std::string> params;
  if (pinned_only) sql += " AND pinned = 1";
  if (!folder.empty()) { sql += " AND folder = ?"; params.push_back(folder); }
  if (!search.empty()) {
    sql += " AND (title LIKE ? OR content LIKE ?)";
    params.push_back("%" + search + "%");
    params.push_back("%" + search + "%");
  }
  if (!tags.empty()) { sql += " AND tags LIKE ?"; params.push_back("%" + tags + "%"); }
  sql += " ORDER BY pinned DESC, updated_at DESC LIMIT ?";
  sqlite3_stmt* stmt;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return {};
  int idx = 1;
  for (const auto& p : params) sqlite3_bind_text(stmt, idx++, p.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, idx, limit);
  std::vector<nlohmann::json> results;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    auto col = [&](int i) -> std::string {
      const unsigned char* txt = sqlite3_column_text(stmt, i);
      return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
    };
    results.push_back({{"id", col(0)}, {"title", col(1)}, {"content", col(2)}, {"folder", col(3)},
                       {"tags", col(4)}, {"pinned", sqlite3_column_int(stmt, 5) != 0},
                       {"created_at", col(6)}, {"updated_at", col(7)}});
  }
  sqlite3_finalize(stmt);
  return results;
}

bool AgentDatabase::update_note(const std::string& id, const nlohmann::json& data) {
  auto existing = get_note(id);
  if (existing.is_null()) return false;
  std::string now = get_current_timestamp();
  sqlite3_stmt* stmt;
  const char* sql = R"(
    UPDATE notes SET title=?, content=?, folder=?, tags=?, pinned=?, updated_at=? WHERE id=?
  )";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, data.value("title", existing["title"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, data.value("content", existing["content"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, data.value("folder", existing["folder"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, data.value("tags", existing["tags"].get<std::string>()).c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, data.contains("pinned") ? (data["pinned"].get<bool>() ? 1 : 0)
                                                      : (existing["pinned"].get<bool>() ? 1 : 0));
  sqlite3_bind_text(stmt, 6, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, id.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool AgentDatabase::delete_note(const std::string& id) {
  sqlite3_stmt* stmt;
  const char* sql = "DELETE FROM notes WHERE id = ?";
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

std::string AgentDatabase::add_rpc_node(const std::string& name, const std::string& endpoint) {
    std::string id = generate_uuid();
    std::string now = get_current_timestamp();
    sqlite3_stmt* stmt;
    const char* sql = R"(
        INSERT INTO rpc_nodes (id, name, endpoint, enabled, created_at)
        VALUES (?, ?, ?, 1, ?)
    )";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[delta-db] add_rpc_node prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return "";
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, endpoint.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? id : "";
}

std::vector<AgentDatabase::RpcNode> AgentDatabase::get_enabled_rpc_nodes() {
    std::vector<RpcNode> nodes;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, name, endpoint, enabled, created_at FROM rpc_nodes WHERE enabled = 1 ORDER BY created_at DESC";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return nodes;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RpcNode node;
        const unsigned char* id_txt = sqlite3_column_text(stmt, 0);
        const unsigned char* name_txt = sqlite3_column_text(stmt, 1);
        const unsigned char* endpoint_txt = sqlite3_column_text(stmt, 2);
        const unsigned char* created_txt = sqlite3_column_text(stmt, 4);

        node.id = id_txt ? reinterpret_cast<const char*>(id_txt) : "";
        node.name = name_txt ? reinterpret_cast<const char*>(name_txt) : "";
        node.endpoint = endpoint_txt ? reinterpret_cast<const char*>(endpoint_txt) : "";
        node.enabled = sqlite3_column_int(stmt, 3) != 0;
        node.created_at = created_txt ? reinterpret_cast<const char*>(created_txt) : "";
        
        nodes.push_back(node);
    }
    
    sqlite3_finalize(stmt);
    return nodes;
}

std::vector<AgentDatabase::RpcNode> AgentDatabase::list_rpc_nodes() {
    std::vector<RpcNode> nodes;
    sqlite3_stmt* stmt;
    const char* sql = "SELECT id, name, endpoint, enabled, created_at FROM rpc_nodes ORDER BY created_at DESC";
    
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return nodes;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RpcNode node;
        const unsigned char* id_txt = sqlite3_column_text(stmt, 0);
        const unsigned char* name_txt = sqlite3_column_text(stmt, 1);
        const unsigned char* endpoint_txt = sqlite3_column_text(stmt, 2);
        const unsigned char* created_txt = sqlite3_column_text(stmt, 4);

        node.id = id_txt ? reinterpret_cast<const char*>(id_txt) : "";
        node.name = name_txt ? reinterpret_cast<const char*>(name_txt) : "";
        node.endpoint = endpoint_txt ? reinterpret_cast<const char*>(endpoint_txt) : "";
        node.enabled = sqlite3_column_int(stmt, 3) != 0;
        node.created_at = created_txt ? reinterpret_cast<const char*>(created_txt) : "";
        
        nodes.push_back(node);
    }
    sqlite3_finalize(stmt);
    return nodes;
}

bool AgentDatabase::update_rpc_node_status(const std::string& id, bool enabled) {
    sqlite3_stmt* stmt;
    const char* sql = "UPDATE rpc_nodes SET enabled = ? WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, enabled ? 1 : 0);
    sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool AgentDatabase::delete_rpc_node(const std::string& id) {
    sqlite3_stmt* stmt;
    const char* sql = "DELETE FROM rpc_nodes WHERE id = ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

} // namespace agent
} // namespace delta