/**
 * History and Session Management Module for Delta CLI
 * Provides secure, local history storage and session management
 */

#include "history.h"
#include "delta_cli.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <map>
// Using manual JSON parsing for efficiency and compatibility

namespace delta {

// ============================================================================
// HISTORY ENTRY IMPLEMENTATION
// ============================================================================

HistoryEntry::HistoryEntry(const std::string& user_msg, const std::string& ai_resp, 
                           const std::string& model, const std::string& session) {
    id = generate_id();
    timestamp = get_current_timestamp();
    user_message = user_msg;
    ai_response = ai_resp;
    model_used = model;
    session_id = session;
}

std::string HistoryEntry::generate_id() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    oss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string HistoryEntry::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    // Use locale-aware formatting for international users
    std::ostringstream oss;
    oss.imbue(std::locale("")); // Use system locale
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ============================================================================
// SESSION IMPLEMENTATION
// ============================================================================

Session::Session(const std::string& session_name, const std::string& model) {
    name = session_name;
    id = generate_session_id();
    created_at = get_current_timestamp();
    last_accessed = created_at;
    model_used = model;
}

std::string Session::generate_session_id() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << "session_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return oss.str();
}

std::string Session::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ============================================================================
// HISTORY MANAGER IMPLEMENTATION
// ============================================================================

HistoryManager::HistoryManager() {
    std::string home = tools::FileOps::get_home_dir();
    history_dir_ = tools::FileOps::join_path(home, ".delta-cli");
    history_dir_ = tools::FileOps::join_path(history_dir_, "history");
    sessions_dir_ = tools::FileOps::join_path(history_dir_, "sessions");
    
    ensure_directories();
    load_sessions();
    
    // Initialize default session handling
    initialize_default_session();
}

void HistoryManager::ensure_directories() {
    if (!tools::FileOps::dir_exists(history_dir_)) {
        tools::FileOps::create_dir(history_dir_);
    }
    if (!tools::FileOps::dir_exists(sessions_dir_)) {
        tools::FileOps::create_dir(sessions_dir_);
    }
}

bool HistoryManager::create_session(const std::string& name, const std::string& model) {
    // Check if session already exists
    for (const auto& session : sessions_) {
        if (session.name == name) {
            return false; // Session already exists
        }
    }
    
    Session new_session(name, model);
    sessions_.push_back(new_session);
    current_session_ = name;
    
    return save_session(new_session);
}

bool HistoryManager::switch_session(const std::string& name) {
    // Save current session before switching
    if (!current_session_.empty()) {
        save_current_session();
    }
    
    // Check if session exists
    bool session_exists = false;
    for (const auto& session : sessions_) {
        if (session.name == name) {
            session_exists = true;
            break;
        }
    }
    
    if (!session_exists) {
        return false; // Session not found
    }
    
    // Switch to the new session
    current_session_ = name;
    
    // Load history from disk for the new session
    load_history_from_disk();
    
    return true;
}

bool HistoryManager::delete_session(const std::string& name) {
    // Don't allow deleting current session
    if (current_session_ == name) {
        return false;
    }
    
    // Remove from memory
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [&name](const Session& s) { return s.name == name; });
    
    if (it != sessions_.end()) {
        sessions_.erase(it);
    }
    
    // Remove from disk
    std::string session_file = tools::FileOps::join_path(sessions_dir_, name + ".json");
    if (tools::FileOps::file_exists(session_file)) {
        return std::remove(session_file.c_str()) == 0;
    }
    
    return true;
}

std::vector<std::string> HistoryManager::list_sessions() {
    std::vector<std::string> session_names;
    for (const auto& session : sessions_) {
        session_names.push_back(session.name);
    }
    return session_names;
}

std::string HistoryManager::get_current_session() const {
    return current_session_;
}

void HistoryManager::add_entry(const std::string& user_message, const std::string& ai_response, 
                              const std::string& model) {
    if (current_session_.empty()) {
        // Create default session if none exists
        create_session("default", model);
    }
    
    HistoryEntry entry(user_message, ai_response, model, current_session_);
    current_history_.push_back(entry);
    
    // CRITICAL: Save immediately after each message for persistence
    save_message_to_disk(entry);
    
    // Enhanced auto-save feedback with progress indication
    static int message_count = 0;
    message_count++;
    
    if (message_count % 5 == 0) {
        UI::print_info("✓ Auto-saved conversation (" + std::to_string(message_count) + " messages)");
    } else if (message_count == 1) {
        UI::print_info("✓ Conversation auto-saved");
    }
}

std::vector<HistoryEntry> HistoryManager::get_history() const {
    return current_history_;
}

bool HistoryManager::delete_history_entry(const std::string& entry_id) {
    auto it = std::find_if(current_history_.begin(), current_history_.end(),
        [&entry_id](const HistoryEntry& e) { return e.id == entry_id; });
    
    if (it != current_history_.end()) {
        current_history_.erase(it);
        save_current_session();
        return true;
    }
    return false;
}

bool HistoryManager::delete_history_by_date(const std::string& date_type, const std::string& date) {
    if (date_type == "all") {
        current_history_.clear();
        save_current_session();
        return true;
    }
    
    // Parse date and filter entries
    std::vector<HistoryEntry> filtered_entries;
    for (const auto& entry : current_history_) {
        if (!matches_date_filter(entry.timestamp, date_type, date)) {
            filtered_entries.push_back(entry);
        }
    }
    
    current_history_ = filtered_entries;
    save_current_session();
    return true;
}

void HistoryManager::clear_history() {
    current_history_.clear();
    save_current_session();
}

bool HistoryManager::save_session(const Session& session) {
    std::string session_file = tools::FileOps::join_path(sessions_dir_, session.name + ".json");
    
    std::ofstream file(session_file);
    if (!file.is_open()) {
        return false;
    }
    
    file << "{\n";
    file << "  \"name\": \"" << session.name << "\",\n";
    file << "  \"id\": \"" << session.id << "\",\n";
    file << "  \"created_at\": \"" << session.created_at << "\",\n";
    file << "  \"last_accessed\": \"" << session.last_accessed << "\",\n";
    file << "  \"model_used\": \"" << session.model_used << "\",\n";
    file << "  \"entries\": [\n";
    
    for (size_t i = 0; i < session.entries.size(); ++i) {
        const auto& entry = session.entries[i];
        file << "    {\n";
        file << "      \"id\": \"" << entry.id << "\",\n";
        file << "      \"timestamp\": \"" << entry.timestamp << "\",\n";
        file << "      \"user_message\": \"" << escape_json(entry.user_message) << "\",\n";
        file << "      \"ai_response\": \"" << escape_json(entry.ai_response) << "\",\n";
        file << "      \"model_used\": \"" << entry.model_used << "\",\n";
        file << "      \"session_id\": \"" << entry.session_id << "\"\n";
        file << "    }";
        if (i < session.entries.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    return true;
}

bool HistoryManager::load_sessions() {
    if (!tools::FileOps::dir_exists(sessions_dir_)) {
        return false;
    }
    
    auto files = tools::FileOps::list_dir(sessions_dir_);
    sessions_.clear();
    
    for (const auto& file : files) {
        if (file.length() > 5 && file.substr(file.length() - 5) == ".json") {
            std::string session_name = file.substr(0, file.length() - 5);
            load_session(session_name);
        }
    }
    
    return true;
}

bool HistoryManager::load_session(const std::string& name) {
    std::string session_file = tools::FileOps::join_path(sessions_dir_, name + ".json");
    
    if (!tools::FileOps::file_exists(session_file)) {
        return false;
    }
    
    std::ifstream file(session_file);
    if (!file.is_open()) {
        return false;
    }
    
    // Enhanced JSON parsing for proper history loading
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    Session session;
    session.name = name;
    session.id = "loaded_" + name;
    session.created_at = "unknown";
    session.last_accessed = get_current_timestamp();
    session.model_used = "unknown";
    
    // Parse JSON content to load history entries
    if (parse_session_json(content, session)) {
        sessions_.push_back(session);
        // Only show message for non-empty sessions to reduce spam
        if (!session.entries.empty()) {
            UI::print_info("Loaded session '" + name + "' with " + std::to_string(session.entries.size()) + " entries");
        }
        return true;
    }
    
    // Fallback: create empty session
    sessions_.push_back(session);
    return true;
}

void HistoryManager::load_session_history(const std::string& name) {
    // Load history for the specified session
    current_history_.clear();
    
    for (const auto& session : sessions_) {
        if (session.name == name) {
            current_history_ = session.entries;
            UI::print_info("Loaded " + std::to_string(current_history_.size()) + " history entries for session '" + name + "'");
            break;
        }
    }
}

bool HistoryManager::parse_session_json(const std::string& json_content, Session& session) {
    // Simple JSON parsing for session data
    // Look for entries array and parse each entry
    size_t entries_start = json_content.find("\"entries\":[");
    if (entries_start == std::string::npos) {
        return true; // No entries found, but session is valid
    }
    
    entries_start = json_content.find("[", entries_start);
    if (entries_start == std::string::npos) {
        return true; // No entries array, but session is valid
    }
    
    size_t entries_end = json_content.find("]", entries_start);
    if (entries_end == std::string::npos) {
        return true; // No closing bracket, but session is valid
    }
    
    std::string entries_json = json_content.substr(entries_start + 1, entries_end - entries_start - 1);
    
    // Parse individual entries
    size_t pos = 0;
    while (pos < entries_json.length()) {
        size_t entry_start = entries_json.find("{", pos);
        if (entry_start == std::string::npos) break;
        
        size_t entry_end = entries_json.find("}", entry_start);
        if (entry_end == std::string::npos) break;
        
        std::string entry_json = entries_json.substr(entry_start, entry_end - entry_start + 1);
        HistoryEntry entry = parse_history_entry(entry_json);
        if (!entry.id.empty()) {
            session.entries.push_back(entry);
        }
        
        pos = entry_end + 1;
    }
    
    return true;
}

HistoryEntry HistoryManager::parse_history_entry(const std::string& entry_json) {
    HistoryEntry entry;
    
    // Extract fields from JSON entry
    auto extract_field = [&](const std::string& field_name) -> std::string {
        std::string pattern = "\"" + field_name + "\":\"";
        size_t start = entry_json.find(pattern);
        if (start == std::string::npos) return "";
        
        start += pattern.length();
        size_t end = entry_json.find("\"", start);
        if (end == std::string::npos) return "";
        
        return entry_json.substr(start, end - start);
    };
    
    entry.id = extract_field("id");
    entry.timestamp = extract_field("timestamp");
    entry.user_message = extract_field("user_message");
    entry.ai_response = extract_field("ai_response");
    entry.model_used = extract_field("model_used");
    entry.session_id = extract_field("session_id");
    
    return entry;
}

// Legacy JSON parsing functions removed - now using nlohmann::json for efficiency

// Legacy JSON parsing functions removed - now using nlohmann::json for efficiency

void HistoryManager::save_current_session() {
    if (current_session_.empty()) {
        return;
    }
    
    // Find current session and update it
    for (auto& session : sessions_) {
        if (session.name == current_session_) {
            session.entries = current_history_;
            session.last_accessed = get_current_timestamp();
            save_session(session);
            break;
        }
    }
    
    // Save the entire history to disk for persistence
    save_session_history_to_disk();
}

// Save the entire session history to disk
void HistoryManager::save_session_history_to_disk() {
    if (current_session_.empty()) {
        return;
    }
    
    ensure_session_directory();
    
    std::string history_file = tools::FileOps::join_path(history_dir_, current_session_);
    history_file = tools::FileOps::join_path(history_file, "history.json");
    std::string temp_file = history_file + ".tmp";
    
    try {
        // Write the entire history to temp file
        std::ofstream temp(temp_file, std::ios::binary);
        if (!temp.is_open()) {
            UI::print_error("ℹ History save failed for session '" + current_session_ + "' - using in-memory only");
            return;
        }
        
        // Write optimized JSON array
        temp << "[\n";
        bool first_entry = true;
        for (size_t i = 0; i < current_history_.size(); ++i) {
            const auto& e = current_history_[i];
            
            // Save user message if it exists
            if (!e.user_message.empty()) {
                if (!first_entry) temp << ",";
                temp << "\n  {\n";
                temp << "    \"timestamp\": \"" << escape_json(e.timestamp) << "\",\n";
                temp << "    \"role\": \"user\",\n";
                temp << "    \"content\": \"" << escape_json(e.user_message) << "\",\n";
                temp << "    \"model\": \"" << escape_json(e.model_used) << "\"\n";
                temp << "  }";
                first_entry = false;
            }
            
            // Save AI response if it exists
            if (!e.ai_response.empty()) {
                if (!first_entry) temp << ",";
                temp << "\n  {\n";
                temp << "    \"timestamp\": \"" << escape_json(e.timestamp) << "\",\n";
                temp << "    \"role\": \"assistant\",\n";
                temp << "    \"content\": \"" << escape_json(e.ai_response) << "\",\n";
                temp << "    \"model\": \"" << escape_json(e.model_used) << "\"\n";
                temp << "  }";
                first_entry = false;
            }
        }
        temp << "\n]\n";
        temp.close();
        
        // Atomic rename
        if (std::rename(temp_file.c_str(), history_file.c_str()) != 0) {
            UI::print_error("ℹ History save failed for session '" + current_session_ + "' - using in-memory only");
            std::remove(temp_file.c_str());
        } else {
            // Success feedback
            if (!current_history_.empty()) {
                UI::print_info("ℹ History saved for session '" + current_session_ + "' (" + std::to_string(current_history_.size()) + " entries)");
            }
        }
        
    } catch (const std::exception& e) {
        UI::print_error("ℹ History save failed for session '" + current_session_ + "' - using in-memory only");
        std::remove(temp_file.c_str());
    }
}

// CRITICAL: Optimized JSON-based persistence with efficient I/O for any session
void HistoryManager::save_message_to_disk(const HistoryEntry& entry) {
    if (current_session_.empty()) {
        return;
    }
    
    ensure_session_directory();
    
    std::string history_file = tools::FileOps::join_path(history_dir_, current_session_);
    history_file = tools::FileOps::join_path(history_file, "history.json");
    std::string temp_file = history_file + ".tmp";
    
    try {
        // Load existing history efficiently
        std::vector<HistoryEntry> existing_entries;
        if (tools::FileOps::file_exists(history_file)) {
            std::ifstream file(history_file);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                file.close();
                
                // Parse existing JSON array
                existing_entries = parse_history_json_array(content);
            }
        }
        
        // Add new entry
        existing_entries.push_back(entry);
        
        // Atomic save to temp file
        std::ofstream temp(temp_file, std::ios::binary);
        if (!temp.is_open()) {
            UI::print_error("ℹ History save failed - using in-memory only");
            return;
        }
        
        // Write optimized JSON array
        temp << "[\n";
        bool first_entry = true;
        for (size_t i = 0; i < existing_entries.size(); ++i) {
            const auto& e = existing_entries[i];
            
            // Save user message if it exists
            if (!e.user_message.empty()) {
                if (!first_entry) temp << ",";
                temp << "\n  {\n";
                temp << "    \"timestamp\": \"" << escape_json(e.timestamp) << "\",\n";
                temp << "    \"role\": \"user\",\n";
                temp << "    \"content\": \"" << escape_json(e.user_message) << "\",\n";
                temp << "    \"model\": \"" << escape_json(e.model_used) << "\"\n";
                temp << "  }";
                first_entry = false;
            }
            
            // Save AI response if it exists
            if (!e.ai_response.empty()) {
                if (!first_entry) temp << ",";
                temp << "\n  {\n";
                temp << "    \"timestamp\": \"" << escape_json(e.timestamp) << "\",\n";
                temp << "    \"role\": \"assistant\",\n";
                temp << "    \"content\": \"" << escape_json(e.ai_response) << "\",\n";
                temp << "    \"model\": \"" << escape_json(e.model_used) << "\"\n";
                temp << "  }";
                first_entry = false;
            }
        }
        temp << "\n]\n";
        temp.close();
        
        // Atomic rename
        if (std::rename(temp_file.c_str(), history_file.c_str()) != 0) {
            UI::print_error("ℹ History save failed - using in-memory only");
            std::remove(temp_file.c_str());
        } else {
            // Success feedback (every 5 entries to avoid spam)
            if (existing_entries.size() % 5 == 0) {
                UI::print_info("ℹ History saved (" + std::to_string(existing_entries.size()) + " entries)");
            }
        }
        
    } catch (const std::exception& e) {
        UI::print_error("ℹ History save failed - using in-memory only");
        std::remove(temp_file.c_str());
    }
}

// CRITICAL: Efficient JSON-based history loading with error recovery for any session
bool HistoryManager::load_history_from_disk() {
    if (current_session_.empty()) {
        return false;
    }
    
    std::string history_file = tools::FileOps::join_path(history_dir_, current_session_);
    history_file = tools::FileOps::join_path(history_file, "history.json");
    
    if (!tools::FileOps::file_exists(history_file)) {
        // No history file exists yet - this is normal for new sessions
        current_history_.clear();
        return true;
    }
    
    try {
        std::ifstream file(history_file);
        if (!file.is_open()) {
            UI::print_error("ℹ History load failed for session '" + current_session_ + "' - starting fresh");
            current_history_.clear();
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Parse JSON array using existing parser
        current_history_ = parse_history_json_array(content);
        
        // Keep startup quiet; remove chatty load message
        history_loaded_ = true;
        return true;
        
    } catch (const std::exception& e) {
        UI::print_error("ℹ History load failed for session '" + current_session_ + "' - starting fresh");
        current_history_.clear();
        return false;
    }
}

// Parse JSON array from history file content
std::vector<HistoryEntry> HistoryManager::parse_history_json_array(const std::string& json_content) {
    std::vector<HistoryEntry> entries;
    
    if (json_content.empty() || json_content == "[]") {
        return entries;
    }
    
    try {
        // Enhanced JSON array parsing for the new format
        size_t pos = 0;
        std::map<std::string, HistoryEntry> paired_entries; // Group by timestamp
        
        while (pos < json_content.length()) {
            // Find next entry object
            size_t entry_start = json_content.find("{", pos);
            if (entry_start == std::string::npos) break;
            
            // Find matching closing brace (handle nested objects)
            int brace_count = 0;
            size_t entry_end = entry_start;
            for (size_t i = entry_start; i < json_content.length(); ++i) {
                if (json_content[i] == '{') brace_count++;
                else if (json_content[i] == '}') brace_count--;
                if (brace_count == 0) {
                    entry_end = i;
                    break;
                }
            }
            
            if (entry_end == entry_start) break; // No closing brace found
            
            std::string entry_json = json_content.substr(entry_start, entry_end - entry_start + 1);
            
            // Parse the new JSON format with role-based entries
            HistoryEntry entry = parse_new_format_entry(entry_json);
            if (!entry.id.empty()) {
                // Group entries by timestamp to pair user/assistant messages
                std::string key = entry.timestamp;
                if (paired_entries.find(key) == paired_entries.end()) {
                    paired_entries[key] = entry;
                } else {
                    // Merge with existing entry
                    HistoryEntry& existing = paired_entries[key];
                    if (entry.user_message.empty() && !entry.ai_response.empty()) {
                        existing.ai_response = entry.ai_response;
                    } else if (!entry.user_message.empty() && entry.ai_response.empty()) {
                        existing.user_message = entry.user_message;
                    }
                }
            }
            
            pos = entry_end + 1;
        }
        
        // Convert map to vector
        for (const auto& pair : paired_entries) {
            entries.push_back(pair.second);
        }
        
    } catch (const std::exception& e) {
        UI::print_error("Error parsing history JSON: " + std::string(e.what()));
    }
    
    return entries;
}

// Parse individual JSON entry from the new format
HistoryEntry HistoryManager::parse_new_format_entry(const std::string& entry_json) {
    HistoryEntry entry;
    
    // Enhanced JSON field extraction with proper handling of escaped quotes
    auto extract_field = [&](const std::string& field_name) -> std::string {
        // Look for field with optional whitespace around the colon
        std::string pattern = "\"" + field_name + "\"";
        size_t start = entry_json.find(pattern);
        if (start == std::string::npos) {
            return "";
        }
        
        // Find the colon after the field name
        start = entry_json.find(":", start);
        if (start == std::string::npos) {
            return "";
        }
        
        // Skip whitespace after colon
        start++;
        while (start < entry_json.length() && (entry_json[start] == ' ' || entry_json[start] == '\t' || entry_json[start] == '\n' || entry_json[start] == '\r')) {
            start++;
        }
        
        // Check for opening quote
        if (start >= entry_json.length() || entry_json[start] != '"') {
            return "";
        }
        
        start++; // Skip opening quote
        
        // Find the end of the quoted value, handling escaped quotes
        size_t end = start;
        bool escaped = false;
        while (end < entry_json.length()) {
            if (entry_json[end] == '\\' && !escaped) {
                escaped = true;
            } else if (entry_json[end] == '"' && !escaped) {
                break;
            } else {
                escaped = false;
            }
            end++;
        }
        
        if (end >= entry_json.length()) {
            return "";
        }
        
        std::string value = entry_json.substr(start, end - start);
        
        // Unescape JSON strings
        std::string result;
        for (size_t i = 0; i < value.length(); ++i) {
            if (value[i] == '\\' && i + 1 < value.length()) {
                switch (value[i + 1]) {
                    case '"': result += '"'; i++; break;
                    case '\\': result += '\\'; i++; break;
                    case 'n': result += '\n'; i++; break;
                    case 'r': result += '\r'; i++; break;
                    case 't': result += '\t'; i++; break;
                    default: result += value[i]; break;
                }
            } else {
                result += value[i];
            }
        }
        
        return result;
    };
    
    entry.timestamp = extract_field("timestamp");
    std::string role = extract_field("role");
    std::string content = extract_field("content");
    entry.model_used = extract_field("model");
    entry.session_id = current_session_;
    // Generate simple ID for loaded entry
    entry.id = "loaded_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    if (role == "user") {
        entry.user_message = content;
        entry.ai_response = ""; // Will be filled by next entry if it's assistant
    } else if (role == "assistant") {
        entry.user_message = ""; // This is an AI response, user message from previous entry
        entry.ai_response = content;
    }
    
    return entry;
}

// Ensure session directory exists for any session
void HistoryManager::ensure_session_directory() {
    if (current_session_.empty()) {
        return;
    }
    
    std::string session_dir = tools::FileOps::join_path(history_dir_, current_session_);
    if (!tools::FileOps::file_exists(session_dir)) {
        tools::FileOps::create_dir(session_dir);
    }
}

std::string HistoryManager::escape_json(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

bool HistoryManager::matches_date_filter(const std::string& timestamp, const std::string& filter_type, 
                                        const std::string& date) {
    if (filter_type == "day") {
        return timestamp.substr(0, 10) == date; // YYYY-MM-DD
    } else if (filter_type == "week") {
        // Implement week filtering logic
        return false; // Simplified for now
    } else if (filter_type == "year") {
        return timestamp.substr(0, 4) == date; // YYYY
    }
    return false;
}

std::string HistoryManager::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    // Use locale-aware formatting for international users
    std::ostringstream oss;
    oss.imbue(std::locale("")); // Use system locale
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ============================================================================
// GLOBAL HISTORY MANAGER INSTANCE
// ============================================================================

// ============================================================================
// DEFAULT SESSION HANDLING
// ============================================================================

void HistoryManager::initialize_default_session() {
    // Always enforce default session usage
    bool default_exists = false;
    for (const auto& session : sessions_) {
        if (session.name == "default") {
            default_exists = true;
            break;
        }
    }
    
    // If no default session exists, create one
    if (!default_exists) {
        create_session("default", "unknown");
        UI::print_info("Created default session for new user");
    }
    
    // ALWAYS switch to default session - this enforces default session usage
    switch_session("default");
    // Quiet default session enforcement message
}

bool HistoryManager::ensure_default_session() {
    // ALWAYS enforce default session usage - this is the core behavior
    // Check if default session exists
    bool default_exists = false;
    for (const auto& session : sessions_) {
        if (session.name == "default") {
            default_exists = true;
            break;
        }
    }
    
    // Create default session if it doesn't exist
    if (!default_exists) {
        if (!create_session("default", "unknown")) {
            return false;
        }
    }
    
    // ALWAYS switch to default session - this enforces the behavior
    if (switch_session("default")) {
        // Quiet default session enforcement message
        return true;
    }
    
    return false;
}

bool HistoryManager::enforce_default_session() {
    // This method ALWAYS enforces default session usage
    // It should be called on every delta startup
    
    // Check if default session exists
    bool default_exists = false;
    for (const auto& session : sessions_) {
        if (session.name == "default") {
            default_exists = true;
            break;
        }
    }
    
    // Create default session if it doesn't exist
    if (!default_exists) {
        if (!create_session("default", "unknown")) {
            UI::print_error("Failed to create default session");
            return false;
        }
        UI::print_info("Created default session for new user");
    }
    
    // ALWAYS switch to default session - this is the core enforcement
    if (switch_session("default")) {
        UI::print_info("ℹ Using default session");
        return true;
    }
    
    UI::print_error("Failed to switch to default session");
    return false;
}

bool HistoryManager::is_default_session_active() const {
    return current_session_ == "default";
}

std::string HistoryManager::get_current_session_name() const {
    return current_session_.empty() ? "default" : current_session_;
}

std::string HistoryManager::get_session_info() const {
    std::ostringstream info;
    
    // Session name
    std::string session_name = get_current_session_name();
    info << "Session: " << session_name << "\n";
    
    // Chat count
    size_t chat_count = current_history_.size();
    info << "Chats: " << chat_count << "\n";
    
    // Calculate conversation statistics
    size_t total_chars = 0;
    size_t total_words = 0;
    for (const auto& entry : current_history_) {
        total_chars += entry.user_message.length() + entry.ai_response.length();
        // Simple word count (split by spaces)
        std::string combined = entry.user_message + " " + entry.ai_response;
        std::istringstream iss(combined);
        std::string word;
        while (iss >> word) {
            total_words++;
        }
    }
    
    info << "Total characters: " << total_chars << "\n";
    info << "Total words: " << total_words << "\n";
    
    // Last modified time
    if (!current_history_.empty()) {
        const auto& last_entry = current_history_.back();
        info << "Last modified: " << last_entry.timestamp << "\n";
    } else {
        info << "Last modified: Never\n";
    }
    
    // Preview of last few messages
    if (!current_history_.empty()) {
        info << "\nRecent messages:\n";
        size_t preview_count = std::min(size_t(3), current_history_.size());
        for (size_t i = current_history_.size() - preview_count; i < current_history_.size(); ++i) {
            const auto& entry = current_history_[i];
            info << "  [" << entry.timestamp << "] User: " 
                 << (entry.user_message.length() > 50 ? 
                     entry.user_message.substr(0, 47) + "..." : 
                     entry.user_message) << "\n";
        }
    }
    
    return info.str();
}

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

static HistoryManager* g_history_manager = nullptr;

HistoryManager& get_history_manager() {
    if (!g_history_manager) {
        g_history_manager = new HistoryManager();
    }
    return *g_history_manager;
}

void cleanup_history_manager() {
    if (g_history_manager) {
        delete g_history_manager;
        g_history_manager = nullptr;
    }
}

} // namespace delta