#ifndef DELTA_MEMORY_STORE_H
#define DELTA_MEMORY_STORE_H

#include <mutex>
#include <string>
#include <vector>
#include "json.hpp"
#include "sqlite3.h"

namespace delta {
namespace agent {

struct Memory {
    std::string id;
    std::string kind; // fact | preference | project | reference
    std::string content;
    std::string tags;
    int importance = 1; // 1 normal, 2 notable, 3 always-load
    std::string source;
    // Empty means general: a way of doing things that applies in any conversation. Otherwise the
    // conversation that learned it, and the only one it is recalled in.
    std::string conversation_id;
    std::string created_at;
    std::string updated_at;
    int use_count = 0;

    nlohmann::json to_json() const;
};

// Everything the harness remembers between turns and between conversations.
//
// Three separate concerns share one store because they share one SQLite file:
//   * memories   -- durable facts and preferences the model chooses to keep
//   * scratchpad -- the plan for the run currently in flight
//   * policy     -- the user's remembered approve/deny answers per tool
//
// Shares the AgentDatabase connection rather than opening a second one, so there is never more
// than one writer on the file.
class MemoryStore {
  public:
    static MemoryStore& instance();

    // Creates the tables if needed. Safe to call repeatedly.
    bool init(sqlite3* db);
    bool ready() const { return db_ != nullptr && mutex_ != nullptr; }

    // --- long-term memory ---
    // `conversation_id` empty saves a general memory, visible everywhere. Anything else scopes the
    // memory to that conversation, so one job's details never surface in an unrelated one.
    std::string remember(const std::string& content, const std::string& kind, const std::string& tags, int importance,
                         const std::string& source = "", const std::string& conversation_id = "");
    bool forget(const std::string& id);
    bool touch(const std::string& id);
    // Keyword-ranked retrieval. Falls back to most-important-and-recent when `query` is empty.
    // `conversation_id` limits the result to general memories plus that conversation's own; pass ""
    // to browse everything, which is what the /memory command and the settings UI want.
    std::vector<Memory> search(const std::string& query, int limit, const std::string& conversation_id = "") const;
    // Memories marked importance >= 3, always loaded into the system prompt. Same scoping rule.
    std::vector<Memory> pinned(int limit, const std::string& conversation_id = "") const;
    std::vector<Memory> recent(int limit) const;
    int count() const;

    // --- per-run scratchpad ---
    void set_plan(const std::string& run_id, const std::string& goal, const nlohmann::json& steps);
    nlohmann::json get_plan(const std::string& run_id) const;
    void clear_plan(const std::string& run_id);
    // Removes scratchpads older than `keep_hours`, so the table cannot grow without bound.
    void prune_plans(int keep_hours = 24);

    // --- per-run working notes ---
    //
    // What the model has worked out while doing the current job: a value it read, a decision it
    // made, something it will need again in ten turns. Distinct from a memory, which outlives the
    // job, and safer than the transcript, which gets trimmed when the conversation grows. These are
    // re-rendered into the system prompt every turn, so they survive compaction.
    void add_note(const std::string& run_id, const std::string& note);
    nlohmann::json get_notes(const std::string& run_id) const;
    void clear_notes(const std::string& run_id);
    void prune_notes(int keep_hours = 24);

    // --- remembered approval decisions ---
    // Returns "allow", "deny", or "" when the user has not answered for this tool yet.
    std::string get_policy(const std::string& tool) const;
    void set_policy(const std::string& tool, const std::string& decision);
    void clear_policies();
    nlohmann::json list_policies() const;

  private:
    MemoryStore() = default;
    MemoryStore(const MemoryStore&) = delete;
    MemoryStore& operator=(const MemoryStore&) = delete;

    std::vector<Memory> query_all() const;

    sqlite3* db_ = nullptr;
    // Points at AgentDatabase's lock, because both classes drive the same connection. Null until
    // init() succeeds, which is also what ready() reports.
    mutable std::recursive_mutex* mutex_ = nullptr;
};

} // namespace agent
} // namespace delta

#endif // DELTA_MEMORY_STORE_H
