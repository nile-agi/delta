/**
 * History and Session Management Header
 * Provides structures and declarations for history management
 */

#ifndef DELTA_HISTORY_H
#define DELTA_HISTORY_H

#include <string>
#include <vector>

namespace delta {

// ============================================================================
// HISTORY ENTRY STRUCTURE
// ============================================================================

struct HistoryEntry {
    std::string id;
    std::string timestamp;
    std::string user_message;
    std::string ai_response;
    std::string model_used;
    std::string session_id;
    
    // Constructor
    HistoryEntry() = default;
    
    HistoryEntry(const std::string& user_msg, const std::string& ai_resp, const std::string& model, const std::string& session);
    
private:
    std::string generate_id();
    std::string get_current_timestamp();
};

// ============================================================================
// SESSION STRUCTURE
// ============================================================================

struct Session {
    std::string name;
    std::string id;
    std::string created_at;
    std::string last_accessed;
    std::string model_used;
    std::vector<HistoryEntry> entries;
    
    Session() = default;
    
    Session(const std::string& session_name, const std::string& model);
    
private:
    std::string generate_session_id();
    std::string get_current_timestamp();
};

// ============================================================================
// HISTORY MANAGER CLASS
// ============================================================================

class HistoryManager {
private:
    std::string history_dir_;
    std::string sessions_dir_;
    std::string current_session_;
    std::vector<Session> sessions_;
    std::vector<HistoryEntry> current_history_;
    
public:
    HistoryManager();
    
    // History loading state
    bool history_loaded_ = false;
    bool is_history_loaded() const { return history_loaded_; }
    
    // Session management
    bool create_session(const std::string& name, const std::string& model);
    bool switch_session(const std::string& name);
    bool delete_session(const std::string& name);
    std::vector<std::string> list_sessions();
    std::string get_current_session() const;
    
    // History management
    void add_entry(const std::string& user_message, const std::string& ai_response, const std::string& model);
    std::vector<HistoryEntry> get_history() const;
    bool delete_history_entry(const std::string& entry_id);
    bool delete_history_by_date(const std::string& date_type, const std::string& date);
    void clear_history();
    
    // Default session handling
    void initialize_default_session();
    bool ensure_default_session();
    bool enforce_default_session();  // Always enforce default session usage
    bool is_default_session_active() const;
    std::string get_current_session_name() const;
    
    // Session info and status
    std::string get_session_info() const;  // Get formatted session information
    
    // Public methods for persistence
    void load_session_history(const std::string& name);
    void save_current_session();
    
    // Critical persistence methods
    void save_message_to_disk(const HistoryEntry& entry);
    bool load_history_from_disk();
    void ensure_session_directory();
    void save_session_history_to_disk();
    
    // JSON parsing helpers
    std::vector<HistoryEntry> parse_history_json_array(const std::string& json_content);
    HistoryEntry parse_new_format_entry(const std::string& entry_json);
    
private:
    void ensure_directories();
    bool save_session(const Session& session);
    bool load_sessions();
    bool load_session(const std::string& name);
    std::string escape_json(const std::string& str);
    bool matches_date_filter(const std::string& timestamp, const std::string& filter_type, const std::string& date);
    std::string get_current_timestamp();
    bool parse_session_json(const std::string& json_content, Session& session);
    HistoryEntry parse_history_entry(const std::string& entry_json);
};

// Global history manager access
HistoryManager& get_history_manager();
void cleanup_history_manager();

} // namespace delta

#endif // DELTA_HISTORY_H
