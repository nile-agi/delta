/**
 * Test Interactive Commands - Tests for slash command system
 */

#include "commands.h"
#include "delta_cli.h"
#include <iostream>
#include <sstream>
#include <cassert>

// Note: Catch2 headers not available in sandboxed environment
// This file is for syntax validation and logical correctness testing

using namespace delta;

// Mock classes for testing
class MockInferenceEngine : public InferenceEngine {
public:
    bool load_model(const InferenceConfig& config) override {
        return true; // Mock success
    }
    
    std::string generate(const std::string& prompt, int max_tokens, bool stream) override {
        return "Mock response"; // Mock response
    }
};

class MockModelManager : public ModelManager {
public:
    std::vector<ModelInfo> get_friendly_model_list(bool include_available) override {
        std::vector<ModelInfo> models;
        if (include_available) {
            // Mock available models
            ModelInfo info;
            info.name = "qwen3:0.6b";
            info.display_name = "Qwen 3 0.6B";
            info.description = "Ultra-compact multilingual model";
            info.size_str = "400 MB";
            info.quantization = "Q4_K_M";
            info.size_bytes = 400 * 1024 * 1024;
            info.installed = true;
            models.push_back(info);
        } else {
            // Mock local models
            ModelInfo info;
            info.name = "qwen3:0.6b";
            info.display_name = "Qwen 3 0.6B";
            info.description = "Ultra-compact multilingual model";
            info.size_str = "400 MB";
            info.quantization = "Q4_K_M";
            info.size_bytes = 400 * 1024 * 1024;
            info.installed = true;
            models.push_back(info);
        }
        return models;
    }
    
    bool is_model_installed(const std::string& model_name) override {
        return model_name == "qwen3:0.6b";
    }
    
    std::string get_model_path(const std::string& model_name) override {
        if (model_name == "qwen3:0.6b") {
            return "/path/to/qwen3-0.6b.gguf";
        }
        return "";
    }
    
    bool pull_model(const std::string& model_name) override {
        return model_name == "qwen3:0.6b"; // Mock success for valid model
    }
    
    void set_progress_callback(ProgressCallback callback) override {
        // Mock implementation
    }
};

class MockUpdateManager {
public:
    bool check_for_updates(bool verbose) {
        return false; // Mock no updates
    }
    
    bool perform_update() {
        return true; // Mock successful update
    }
};

// Test command parsing
void test_command_parsing() {
    std::cout << "Testing command parsing..." << std::endl;
    
    // Test parse_args
    auto args1 = Commands::parse_args("download qwen3:0.6b");
    assert(args1.size() == 2);
    assert(args1[0] == "download");
    assert(args1[1] == "qwen3:0.6b");
    
    auto args2 = Commands::parse_args("tokens 1024");
    assert(args2.size() == 2);
    assert(args2[0] == "tokens");
    assert(args2[1] == "1024");
    
    auto args3 = Commands::parse_args("help");
    assert(args3.size() == 1);
    assert(args3[0] == "help");
    
    std::cout << "✓ Command parsing tests passed" << std::endl;
}

// Test command registration
void test_command_registration() {
    std::cout << "Testing command registration..." << std::endl;
    
    Commands::init();
    
    // Test that commands are registered
    assert(Commands::is_online_command("download"));
    assert(Commands::is_online_command("pull"));
    assert(Commands::is_online_command("updates"));
    assert(!Commands::is_online_command("list"));
    assert(!Commands::is_online_command("help"));
    
    std::cout << "✓ Command registration tests passed" << std::endl;
}

// Test session state
void test_session_state() {
    std::cout << "Testing session state..." << std::endl;
    
    InteractiveSession session;
    assert(session.engine == nullptr);
    assert(session.config == nullptr);
    assert(session.model_mgr == nullptr);
    assert(session.current_model == "");
    assert(session.max_tokens == 512);
    assert(session.temperature == 0.8);
    assert(session.gpu_layers == 0);
    assert(session.multimodal == false);
    assert(session.no_color == false);
    
    std::cout << "✓ Session state tests passed" << std::endl;
}

// Test command handlers
void test_command_handlers() {
    std::cout << "Testing command handlers..." << std::endl;
    
    // Create mock session
    InteractiveSession session;
    MockInferenceEngine engine;
    InferenceConfig config;
    MockModelManager model_mgr;
    
    session.engine = &engine;
    session.config = &config;
    session.model_mgr = &model_mgr;
    session.current_model = "qwen3:0.6b";
    
    // Test help command
    std::vector<std::string> help_args;
    bool result = Commands::handle_help(help_args, session);
    assert(result == true);
    
    // Test version command
    std::vector<std::string> version_args;
    result = Commands::handle_version(version_args, session);
    assert(result == true);
    
    // Test tokens command
    std::vector<std::string> tokens_args = {"1024"};
    result = Commands::handle_tokens(tokens_args, session);
    assert(result == true);
    assert(session.max_tokens == 1024);
    
    // Test temperature command
    std::vector<std::string> temp_args = {"0.9"};
    result = Commands::handle_temperature(temp_args, session);
    assert(result == true);
    assert(session.temperature == 0.9);
    
    // Test multimodal toggle
    std::vector<std::string> multimodal_args;
    result = Commands::handle_multimodal(multimodal_args, session);
    assert(result == true);
    assert(session.multimodal == true);
    
    std::cout << "✓ Command handlers tests passed" << std::endl;
}

// Test error handling
void test_error_handling() {
    std::cout << "Testing error handling..." << std::endl;
    
    InteractiveSession session;
    MockInferenceEngine engine;
    InferenceConfig config;
    MockModelManager model_mgr;
    
    session.engine = &engine;
    session.config = &config;
    session.model_mgr = &model_mgr;
    
    // Test invalid tokens
    std::vector<std::string> invalid_tokens = {"-1"};
    bool result = Commands::handle_tokens(invalid_tokens, session);
    assert(result == true); // Should handle error gracefully
    
    // Test invalid temperature
    std::vector<std::string> invalid_temp = {"3.0"};
    result = Commands::handle_temperature(invalid_temp, session);
    assert(result == true); // Should handle error gracefully
    
    // Test missing model for use command
    std::vector<std::string> no_model_args;
    result = Commands::handle_use(no_model_args, session);
    assert(result == true); // Should handle error gracefully
    
    std::cout << "✓ Error handling tests passed" << std::endl;
}

// Test command processing
void test_command_processing() {
    std::cout << "Testing command processing..." << std::endl;
    
    InteractiveSession session;
    MockInferenceEngine engine;
    InferenceConfig config;
    MockModelManager model_mgr;
    
    session.engine = &engine;
    session.config = &config;
    session.model_mgr = &model_mgr;
    session.current_model = "qwen3:0.6b";
    
    // Test valid command
    bool result = Commands::process_command("help", session);
    assert(result == true);
    
    // Test valid command with args
    result = Commands::process_command("tokens 1024", session);
    assert(result == true);
    
    // Test invalid command
    result = Commands::process_command("invalid_command", session);
    assert(result == true); // Should handle gracefully
    
    std::cout << "✓ Command processing tests passed" << std::endl;
}

// Test UI integration
void test_ui_integration() {
    std::cout << "Testing UI integration..." << std::endl;
    
    // Test help display
    std::ostringstream oss;
    std::streambuf* old_cout = std::cout.rdbuf();
    std::cout.rdbuf(oss.rdbuf());
    
    Commands::show_help();
    
    std::cout.rdbuf(old_cout);
    std::string output = oss.str();
    
    // Check that help contains expected commands
    assert(output.find("Interactive Commands:") != std::string::npos);
    assert(output.find("/download") != std::string::npos);
    assert(output.find("/list") != std::string::npos);
    assert(output.find("/help") != std::string::npos);
    
    std::cout << "✓ UI integration tests passed" << std::endl;
}

int main() {
    std::cout << "Running interactive commands tests..." << std::endl;
    
    try {
        test_command_parsing();
        test_command_registration();
        test_session_state();
        test_command_handlers();
        test_error_handling();
        test_command_processing();
        test_ui_integration();
        
        std::cout << "✓ All interactive commands tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
