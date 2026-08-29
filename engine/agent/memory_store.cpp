#include "memory_store.h"
#include "time_compat.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iostream>
#include <random>
#include <sstream>

namespace delta {
namespace agent {

namespace {

std::string new_id() {
    static std::mutex mtx;
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    std::lock_guard<std::mutex> lock(mtx);
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (auto& c : uuid) {
        if (c == 'x')
            c = hex[dis(gen)];
        else if (c == 'y')
            c = hex[(dis(gen) & 0x3) | 0x8];
    }
    return uuid;
}

std::string utc_now() {
    time_t now = time(nullptr);
    struct tm t{};
    utc_time(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    return std::string(buf);
}

std::string lower(std::string s) {
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> keywords(const std::string& text) {
    // Words shorter than 3 characters carry no signal for this kind of matching, and a short
    // stop list keeps common verbs from matching every memory equally.
    static const std::vector<std::string> stop = {"the",  "and",  "for",  "you", "your", "are",  "was", "with",
                                                  "that", "this", "have", "has", "but",  "not",  "all", "any",
                                                  "can",  "did",  "does", "how", "what", "when", "who", "why"};
    std::vector<std::string> out;
    std::string current;
    for (char raw : text) {
        unsigned char c = static_cast<unsigned char>(raw);
        if (std::isalnum(c)) {
            current += static_cast<char>(std::tolower(c));
        } else {
            if (current.size() >= 3 && std::find(stop.begin(), stop.end(), current) == stop.end())
                out.push_back(current);
            current.clear();
        }
    }
    if (current.size() >= 3 && std::find(stop.begin(), stop.end(), current) == stop.end())
        out.push_back(current);
    return out;
}

std::string col_text(sqlite3_stmt* stmt, int i) {
    const unsigned char* txt = sqlite3_column_text(stmt, i);
    return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
}

} // namespace

nlohmann::json Memory::to_json() const {
    return {{"id", id},
            {"kind", kind},
            {"content", content},
            {"tags", tags},
            {"importance", importance},
            {"source", source},
            {"created_at", created_at},
            {"updated_at", updated_at},
            {"use_count", use_count}};
}

MemoryStore& MemoryStore::instance() {
    static MemoryStore store;
    return store;
}

bool MemoryStore::init(sqlite3* db) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db)
        return false;
    db_ = db;

    const char* schema = R"(
      CREATE TABLE IF NOT EXISTS agent_memories (
        id TEXT PRIMARY KEY,
        kind TEXT NOT NULL DEFAULT 'fact',
        content TEXT NOT NULL,
        tags TEXT DEFAULT '',
        importance INTEGER DEFAULT 1,
        source TEXT DEFAULT '',
        created_at TEXT NOT NULL,
        updated_at TEXT NOT NULL,
        use_count INTEGER DEFAULT 0
      );
      CREATE INDEX IF NOT EXISTS idx_agent_memories_importance ON agent_memories(importance DESC);
      CREATE INDEX IF NOT EXISTS idx_agent_memories_updated ON agent_memories(updated_at DESC);

      CREATE TABLE IF NOT EXISTS agent_scratchpad (
        run_id TEXT PRIMARY KEY,
        goal TEXT DEFAULT '',
        steps TEXT DEFAULT '[]',
        updated_at TEXT NOT NULL
      );

      CREATE TABLE IF NOT EXISTS agent_tool_policy (
        tool TEXT PRIMARY KEY,
        decision TEXT NOT NULL,
        updated_at TEXT NOT NULL
      );
    )";

    char* err = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[delta-memory] schema init failed: " << (err ? err : "unknown") << std::endl;
        sqlite3_free(err);
        db_ = nullptr;
        return false;
    }
    return true;
}

std::string MemoryStore::remember(const std::string& content, const std::string& kind, const std::string& tags,
                                  int importance, const std::string& source) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || content.empty())
        return "";

    const std::string now = utc_now();

    // Overwrite a near-identical memory instead of accumulating duplicates: the model tends to
    // re-save the same fact whenever it comes up again.
    {
        sqlite3_stmt* dup = nullptr;
        const char* dup_sql = "SELECT id FROM agent_memories WHERE lower(content) = lower(?) LIMIT 1";
        if (sqlite3_prepare_v2(db_, dup_sql, -1, &dup, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(dup, 1, content.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(dup) == SQLITE_ROW) {
                std::string existing = col_text(dup, 0);
                sqlite3_finalize(dup);
                sqlite3_stmt* upd = nullptr;
                const char* upd_sql = "UPDATE agent_memories SET kind=?, tags=?, importance=?, updated_at=? WHERE id=?";
                if (sqlite3_prepare_v2(db_, upd_sql, -1, &upd, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(upd, 1, kind.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(upd, 2, tags.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(upd, 3, importance);
                    sqlite3_bind_text(upd, 4, now.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(upd, 5, existing.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(upd);
                    sqlite3_finalize(upd);
                }
                return existing;
            }
            sqlite3_finalize(dup);
        }
    }

    const std::string id = new_id();
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO agent_memories (id, kind, content, tags, importance, source, created_at, "
                      "updated_at, use_count) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0)";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[delta-memory] remember prepare failed: " << sqlite3_errmsg(db_) << std::endl;
        return "";
    }
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, (kind.empty() ? "fact" : kind).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, tags.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, std::max(1, std::min(3, importance)));
    sqlite3_bind_text(stmt, 6, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, now.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok ? id : "";
}

bool MemoryStore::forget(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM agent_memories WHERE id = ?", -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok && sqlite3_changes(db_) > 0;
}

bool MemoryStore::touch(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "UPDATE agent_memories SET use_count = use_count + 1 WHERE id = ?", -1, &stmt,
                           nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<Memory> MemoryStore::query_all() const {
    std::vector<Memory> out;
    if (!db_)
        return out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, kind, content, tags, importance, source, created_at, updated_at, use_count "
                      "FROM agent_memories";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Memory m;
        m.id = col_text(stmt, 0);
        m.kind = col_text(stmt, 1);
        m.content = col_text(stmt, 2);
        m.tags = col_text(stmt, 3);
        m.importance = sqlite3_column_int(stmt, 4);
        m.source = col_text(stmt, 5);
        m.created_at = col_text(stmt, 6);
        m.updated_at = col_text(stmt, 7);
        m.use_count = sqlite3_column_int(stmt, 8);
        out.push_back(std::move(m));
    }
    sqlite3_finalize(stmt);
    return out;
}

std::vector<Memory> MemoryStore::search(const std::string& query, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto all = query_all();
    if (all.empty() || limit <= 0)
        return {};

    const auto terms = keywords(query);
    if (terms.empty()) {
        // No usable query: fall back to what matters most, most recently.
        std::sort(all.begin(), all.end(), [](const Memory& a, const Memory& b) {
            if (a.importance != b.importance)
                return a.importance > b.importance;
            return a.updated_at > b.updated_at;
        });
        if (static_cast<int>(all.size()) > limit)
            all.resize(limit);
        return all;
    }

    // Score by how many distinct query terms appear in the memory, weighted by importance.
    // Deliberately keyword-based: Delta runs fully offline and cannot assume an embedding model.
    std::vector<std::pair<int, const Memory*>> scored;
    for (const auto& m : all) {
        const std::string haystack = lower(m.content + " " + m.tags + " " + m.kind);
        int hits = 0;
        for (const auto& term : terms) {
            if (haystack.find(term) != std::string::npos)
                hits++;
        }
        if (hits == 0)
            continue;
        scored.emplace_back(hits * 10 + m.importance * 3 + std::min(m.use_count, 5), &m);
    }
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first)
            return a.first > b.first;
        return a.second->updated_at > b.second->updated_at;
    });

    std::vector<Memory> out;
    for (const auto& [score, mem] : scored) {
        (void)score;
        if (static_cast<int>(out.size()) >= limit)
            break;
        out.push_back(*mem);
    }
    return out;
}

std::vector<Memory> MemoryStore::pinned(int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto all = query_all();
    std::vector<Memory> out;
    std::sort(all.begin(), all.end(), [](const Memory& a, const Memory& b) { return a.updated_at > b.updated_at; });
    for (auto& m : all) {
        if (m.importance >= 3 && static_cast<int>(out.size()) < limit)
            out.push_back(std::move(m));
    }
    return out;
}

std::vector<Memory> MemoryStore::recent(int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto all = query_all();
    std::sort(all.begin(), all.end(), [](const Memory& a, const Memory& b) { return a.updated_at > b.updated_at; });
    if (static_cast<int>(all.size()) > limit)
        all.resize(limit);
    return all;
}

int MemoryStore::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return 0;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM agent_memories", -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    int n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

void MemoryStore::set_plan(const std::string& run_id, const std::string& goal, const nlohmann::json& steps) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || run_id.empty())
        return;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO agent_scratchpad (run_id, goal, steps, updated_at) VALUES (?, ?, ?, ?) "
                      "ON CONFLICT(run_id) DO UPDATE SET goal=excluded.goal, steps=excluded.steps, "
                      "updated_at=excluded.updated_at";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return;
    const std::string steps_str = steps.dump();
    const std::string now = utc_now();
    sqlite3_bind_text(stmt, 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, goal.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, steps_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

nlohmann::json MemoryStore::get_plan(const std::string& run_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_ || run_id.empty())
        return nullptr;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT goal, steps, updated_at FROM agent_scratchpad WHERE run_id = ?", -1, &stmt,
                           nullptr) != SQLITE_OK)
        return nullptr;
    sqlite3_bind_text(stmt, 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
    nlohmann::json out = nullptr;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        nlohmann::json steps = nlohmann::json::array();
        try {
            steps = nlohmann::json::parse(col_text(stmt, 1));
        } catch (...) {
        }
        out = {{"goal", col_text(stmt, 0)}, {"steps", steps}, {"updated_at", col_text(stmt, 2)}};
    }
    sqlite3_finalize(stmt);
    return out;
}

void MemoryStore::clear_plan(const std::string& run_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM agent_scratchpad WHERE run_id = ?", -1, &stmt, nullptr) != SQLITE_OK)
        return;
    sqlite3_bind_text(stmt, 1, run_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void MemoryStore::prune_plans(int keep_hours) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return;
    time_t cutoff_t = time(nullptr) - static_cast<time_t>(keep_hours) * 3600;
    struct tm t{};
    utc_time(&cutoff_t, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM agent_scratchpad WHERE updated_at < ?", -1, &stmt, nullptr) != SQLITE_OK)
        return;
    sqlite3_bind_text(stmt, 1, buf, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::string MemoryStore::get_policy(const std::string& tool) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return "";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT decision FROM agent_tool_policy WHERE tool = ?", -1, &stmt, nullptr) !=
        SQLITE_OK)
        return "";
    sqlite3_bind_text(stmt, 1, tool.c_str(), -1, SQLITE_TRANSIENT);
    std::string decision;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        decision = col_text(stmt, 0);
    sqlite3_finalize(stmt);
    return decision;
}

void MemoryStore::set_policy(const std::string& tool, const std::string& decision) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO agent_tool_policy (tool, decision, updated_at) VALUES (?, ?, ?) "
                      "ON CONFLICT(tool) DO UPDATE SET decision=excluded.decision, updated_at=excluded.updated_at";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return;
    const std::string now = utc_now();
    sqlite3_bind_text(stmt, 1, tool.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, decision.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void MemoryStore::clear_policies() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_)
        return;
    sqlite3_exec(db_, "DELETE FROM agent_tool_policy", nullptr, nullptr, nullptr);
}

nlohmann::json MemoryStore::list_policies() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json out = nlohmann::json::object();
    if (!db_)
        return out;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT tool, decision FROM agent_tool_policy", -1, &stmt, nullptr) != SQLITE_OK)
        return out;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        out[col_text(stmt, 0)] = col_text(stmt, 1);
    sqlite3_finalize(stmt);
    return out;
}

} // namespace agent
} // namespace delta
