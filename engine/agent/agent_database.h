#ifndef DELTA_AGENT_DATABASE_H
#define DELTA_AGENT_DATABASE_H

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

    // Calendar CRUD
    std::string create_event(const nlohmann::json& data);
    nlohmann::json get_event(const std::string& id);
    std::vector<nlohmann::json> list_events(const std::string& start = "", const std::string& end = "", int limit = 50);
    bool update_event(const std::string& id, const nlohmann::json& data);
    bool delete_event(const std::string& id);

    // Task CRUD
    std::string create_task(const nlohmann::json& data);
    nlohmann::json get_task(const std::string& id);
    std::vector<nlohmann::json> list_tasks(const std::string& status = "", const std::string& priority = "",
                                           int limit = 50, const std::string& tags = "");
    bool update_task(const std::string& id, const nlohmann::json& data);
    bool complete_task(const std::string& id);
    bool delete_task(const std::string& id);

  private:
    AgentDatabase() = default;
    ~AgentDatabase();
    AgentDatabase(const AgentDatabase&) = delete;
    AgentDatabase& operator=(const AgentDatabase&) = delete;

    sqlite3* db_ = nullptr;
    std::string db_path_;

    bool run_migrations();
    int get_schema_version();
    void set_schema_version(int version);
    std::string generate_uuid();
    std::string get_current_timestamp();

    bool exec_sql(const std::string& sql);
    nlohmann::json row_to_event(sqlite3_stmt* stmt);
    nlohmann::json row_to_task(sqlite3_stmt* stmt);
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_DATABASE_H
