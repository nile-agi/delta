/**
 * Test Default Session Enforcement for Delta CLI
 * Tests that Delta ALWAYS uses the default session on startup
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
            return "/tmp/delta_test_enforcement";
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

void test_default_session_enforcement() {
    std::cout << "Testing default session enforcement..." << std::endl;
    
    // Clean up any existing test data
    std::filesystem::remove_all("/tmp/delta_test_enforcement");
    
    // Create history manager (should auto-enforce default session)
    HistoryManager& history_mgr = get_history_manager();
    
    // Verify default session is ALWAYS active
    assert(history_mgr.get_current_session() == "default");
    assert(history_mgr.is_default_session_active());
    
    std::cout << "✓ Default session enforced on startup" << std::endl;
}

void test_session_switching_reverts() {
    std::cout << "Testing session switching behavior..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Create a test session
    bool created = history_mgr.create_session("test_session", "test_model");
    assert(created);
    
    // Switch to the test session
    bool switched = history_mgr.switch_session("test_session");
    assert(switched);
    assert(history_mgr.get_current_session() == "test_session");
    
    // Now enforce default session (simulates next delta startup)
    bool enforced = history_mgr.enforce_default_session();
    assert(enforced);
    assert(history_mgr.get_current_session() == "default");
    assert(history_mgr.is_default_session_active());
    
    std::cout << "✓ Session switching reverts to default on enforcement" << std::endl;
}

void test_multilingual_enforcement() {
    std::cout << "Testing multilingual support with default session..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Ensure we're using default session
    history_mgr.enforce_default_session();
    assert(history_mgr.is_default_session_active());
    
    // Test with non-ASCII characters (Arabic)
    std::string arabic_input = "مرحبا بالعالم!";
    std::string arabic_response = "أهلاً وسهلاً!";
    
    history_mgr.add_entry(arabic_input, arabic_response, "qwen_model");
    
    auto history = history_mgr.get_history();
    assert(history.size() >= 1);
    
    // Find the Arabic entry
    bool found_arabic = false;
    for (const auto& entry : history) {
        if (entry.user_message == arabic_input && entry.ai_response == arabic_response) {
            found_arabic = true;
            break;
        }
    }
    assert(found_arabic);
    
    std::cout << "✓ Multilingual support works with default session enforcement" << std::endl;
}

void test_persistence_across_restarts() {
    std::cout << "Testing persistence across restarts..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Ensure default session
    history_mgr.enforce_default_session();
    
    // Add some history
    history_mgr.add_entry("Hello", "Hi there!", "test_model");
    history_mgr.add_entry("How are you?", "I'm doing well!", "test_model");
    
    // Simulate restart by creating new history manager
    cleanup_history_manager();
    HistoryManager& new_history_mgr = get_history_manager();
    
    // Enforce default session on "restart"
    new_history_mgr.enforce_default_session();
    
    // Verify we're back to default session
    assert(new_history_mgr.get_current_session() == "default");
    assert(new_history_mgr.is_default_session_active());
    
    // Verify history is still there
    auto history = new_history_mgr.get_history();
    assert(history.size() >= 2);
    
    std::cout << "✓ Default session persists across restarts" << std::endl;
}

void test_web_mode_enforcement() {
    std::cout << "Testing web mode default session enforcement..." << std::endl;
    
    HistoryManager& history_mgr = get_history_manager();
    
    // Simulate web mode by enforcing default session
    bool enforced = history_mgr.enforce_default_session();
    assert(enforced);
    assert(history_mgr.get_current_session() == "default");
    
    // Verify default session is active
    assert(history_mgr.is_default_session_active());
    
    std::cout << "✓ Web mode enforces default session" << std::endl;
}

int main() {
    std::cout << "=== Delta CLI Default Session Enforcement Tests ===" << std::endl;
    
    try {
        test_default_session_enforcement();
        test_session_switching_reverts();
        test_multilingual_enforcement();
        test_persistence_across_restarts();
        test_web_mode_enforcement();
        
        std::cout << "\n=== All enforcement tests passed! ===" << std::endl;
        
        // Cleanup
        std::filesystem::remove_all("/tmp/delta_test_enforcement");
        cleanup_history_manager();
        
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
