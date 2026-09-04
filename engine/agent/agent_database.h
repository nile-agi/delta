#ifndef DELTA_AGENT_DATABASE_H
#define DELTA_AGENT_DATABASE_H

#include <mutex>
#include <string>
#include <vector>
#include "json.hpp"
#include "sqlite3.h"

namespace delta {
namespace agent {

class AgentDatabase {
  public:
    static AgentDatabase& instance();

    bool init(const std::string& db_path = "");
    void close();

    // The shared connection, so subsystems with their own tables (see MemoryStore) can use one
    // writer instead of opening a second handle on the same file. Null until init() succeeds.
    sqlite3* handle() { return db_; }

    // Calendar CRUD
    std::string create_event(const nlohmann::json& data);
    nlohmann::json get_event(const std::string& id);
    std::vector<nlohmann::json> list_events(const std::string& start = "", const std::string& end = "", int limit = 50,
                                            const std::string& type = "", const std::string& status = "",
                                            const std::string& priority = "", const std::string& tags = "");
    bool update_event(const std::string& id, const nlohmann::json& data);
    bool delete_event(const std::string& id);

    // Reminders
    std::vector<nlohmann::json> get_upcoming_reminders();
    bool mark_reminded(const std::string& id);

    // Notes CRUD
    std::string create_note(const nlohmann::json& data);
    nlohmann::json get_note(const std::string& id);
    std::vector<nlohmann::json> list_notes(const std::string& folder = "", const std::string& search = "",
                                           const std::string& tags = "", int limit = 50, bool pinned_only = false);
    bool update_note(const std::string& id, const nlohmann::json& data);
    bool delete_note(const std::string& id);

    // Define the RpcNode struct
    struct RpcNode {
        std::string id;
        std::string name;
        std::string endpoint; // Format: "host:port" (e.g., "127.0.0.1:50052")
        bool enabled;
        std::string created_at;
    };

    // RPC Node Management
    std::string add_rpc_node(const std::string& name, const std::string& endpoint);
    std::vector<RpcNode> get_enabled_rpc_nodes();
    std::vector<RpcNode> list_rpc_nodes();
    bool update_rpc_node_status(const std::string& id, bool enabled);
    bool delete_rpc_node(const std::string& id);

  private:
    AgentDatabase() = default;
    ~AgentDatabase();
    AgentDatabase(const AgentDatabase&) = delete;
    AgentDatabase& operator=(const AgentDatabase&) = delete;

    sqlite3* db_ = nullptr;
    std::string db_path_;
    // Serialises every public method. Statements are already safe on their own (SQLite runs in
    // serialized mode), but update_event and friends read a row and then write it back, and two
    // requests doing that at once lost each other's fields. Recursive because update_* call get_*.
    std::recursive_mutex mutex_;

    bool run_migrations();
    int get_schema_version();
    void set_schema_version(int version);
    std::string generate_uuid();
    std::string get_current_timestamp();

    bool exec_sql(const std::string& sql);
    nlohmann::json row_to_event(sqlite3_stmt* stmt);
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_DATABASE_H