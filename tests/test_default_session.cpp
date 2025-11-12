/**
 * Test Default Session Handling for Delta CLI
 * Tests the automatic default session creation and management
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <string>
#include <vector>

// Mock the required classes for testing
namespace delta {
namespace tools {
    class FileOps {
    public:
        static std::string get_home_dir() {
            return "/tmp/delta_test";
        }
        
        static std::string join_path(const std::string& a, const std::string& b) {
            return a + "/" + b;
        }
        
        static bool dir_exists(const std::string& path) {
            return std::filesystem::exists(path) && std::filesystem::is_directory(path);
        }
        
        static bool file_exists(const std::string& path) {
            return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
        }
        
        static void create_dir(const std::string& path) {
            std::filesystem::create_directories(path);
        }
        
        static std::vector<std::string> list_dir(const std::string& path) {
            std::vector<std::string> files;
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path().filename().string());
                }
            }
            return files;
        }
    };
}

namespace UI {
    void print_info(const std::string& msg) {
        std::cout << "INFO: " << msg << std::endl;
    }
    
    void print_success(const std::string& msg) {
        std::cout << "SUCCESS: " << msg << std::endl;
    }
    
    void print_error(const std::string& msg) {
        std::cout << "ERROR: " << msg << std::endl;
    }
}
}

// Include the actual history implementation
#include "../src/history.h"

using namespace delta;

void test_default_session_creation() {
    std::cout << "Testing default session creation..." << std::endl;
    
    // Clean up any existing test data
    std::filesystem::remove_all("/tmp/delta_test");
    
    // Create history manager (should auto-create default session)
    HistoryManager& history_mgr = get_history_manager();
    
    // Verify default session exists
    auto sessions = history_mgr.list_sessions();
    assert(!sessions.empty());
    assert(std::find(sessions.begin(), sessions.end(), "default") != sessions.end());
    
    // Verify we're using default session
    assert(history_mgr.get_current_session() == "default");
    assert(history_mgr.is_default_session_active());
    
    std::cout << "✓ Default session created successfully" << std::endl;
}

void test_session_switching() {
    std::cout << "Testing session switching..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Create a new session
    bool created = history_mgr.create_session("test_session", "test_model");
    assert(created);
    
    // Switch to the new session
    bool switched = history_mgr.switch_session("test_session");
    assert(switched);
    assert(history_mgr.get_current_session() == "test_session");
    assert(!history_mgr.is_default_session_active());
    
    // Switch back to default
    switched = history_mgr.switch_session("default");
    assert(switched);
    assert(history_mgr.get_current_session() == "default");
    assert(history_mgr.is_default_session_active());
    
    std::cout << "✓ Session switching works correctly" << std::endl;
}

void test_history_persistence() {
    std::cout << "Testing history persistence..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Add some history entries
    history_mgr.add_entry("Hello", "Hi there!", "test_model");
    history_mgr.add_entry("How are you?", "I'm doing well!", "test_model");
    
    // Get history
    auto history = history_mgr.get_history();
    assert(history.size() == 2);
    assert(history[0].user_message == "Hello");
    assert(history[0].ai_response == "Hi there!");
    
    std::cout << "✓ History persistence works correctly" << std::endl;
}

void test_multilingual_support() {
    std::cout << "Testing multilingual support..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Test with non-ASCII characters (Chinese)
    std::string chinese_input = "你好，世界！";
    std::string chinese_response = "你好！很高兴见到你！";
    
    history_mgr.add_entry(chinese_input, chinese_response, "qwen_model");
    
    auto history = history_mgr.get_history();
    assert(history.size() >= 1);
    
    // Find the Chinese entry
    bool found_chinese = false;
    for (const auto& entry : history) {
        if (entry.user_message == chinese_input && entry.ai_response == chinese_response) {
            found_chinese = true;
            break;
        }
    }
    assert(found_chinese);
    
    std::cout << "✓ Multilingual support works correctly" << std::endl;
}

void test_session_cleanup() {
    std::cout << "Testing session cleanup..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Create a test session
    history_mgr.create_session("cleanup_test", "test_model");
    history_mgr.switch_session("cleanup_test");
    
    // Add some history
    history_mgr.add_entry("Test message", "Test response", "test_model");
    
    // Switch back to default
    history_mgr.switch_session("default");
    
    // Delete the test session
    bool deleted = history_mgr.delete_session("cleanup_test");
    assert(deleted);
    
    // Verify session is gone
    auto sessions = history_mgr.list_sessions();
    assert(std::find(sessions.begin(), sessions.end(), "cleanup_test") == sessions.end());
    
    std::cout << "✓ Session cleanup works correctly" << std::endl;
}

int main() {
    std::cout << "=== Delta CLI Default Session Tests ===" << std::endl;
    
    try {
        test_default_session_creation();
        test_session_switching();
        test_history_persistence();
        test_multilingual_support();
        test_session_cleanup();
        
        std::cout << "\n=== All tests passed! ===" << std::endl;
        
        // Cleanup
        std::filesystem::remove_all("/tmp/delta_test");
        cleanup_history_manager();
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
