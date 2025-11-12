/**
 * Test Default Flow - Tests for no-args case with banner and default model handling
 */

#include "delta_cli.h"
#include <iostream>
#include <sstream>
#include <cassert>

// Note: Catch2 headers not available in sandboxed environment
// This file is for syntax validation and logical correctness testing

using namespace delta;

// Simple test framework for syntax validation
void test_default_model() {
    ModelManager mgr;
    
    // Test get_default_model()
    std::string default_model = mgr.get_default_model();
    assert(default_model == "qwen3:0.6b");
    
    // Test get_default_model_short_name()
    std::string short_name = mgr.get_default_model_short_name();
    assert(short_name == "qwen3-0.6b");
    
    // Test is_in_registry()
    assert(mgr.is_in_registry("qwen3:0.6b"));
    assert(mgr.is_in_registry("qwen3-0.6b"));
    
    // Test get_auto_selected_model()
    std::string auto_model = mgr.get_auto_selected_model();
    assert(auto_model == "qwen3-0.6b");
    
    std::cout << "✓ Default model tests passed" << std::endl;
}

void test_registry() {
    ModelManager mgr;
    
    // Test registry entry
    auto entry = mgr.get_registry_entry("qwen3:0.6b");
    assert(!entry.name.empty());
    assert(entry.name == "qwen3:0.6b");
    assert(entry.short_name == "qwen3-0.6b");
    assert(entry.display_name == "Qwen 3 0.6B");
    assert(entry.size_bytes > 0);
    assert(entry.quantization == "Q4_K_M");
    
    // Test registry size
    auto models = mgr.get_registry_models();
    assert(models.size() >= 50);
    
    std::cout << "✓ Registry tests passed" << std::endl;
}

void test_resolution() {
    ModelManager mgr;
    
    // Test resolve_model_name()
    std::string resolved = mgr.resolve_model_name("qwen3:0.6b");
    assert(resolved == "Qwen3-0.6B-Q4_K_M.gguf");
    
    resolved = mgr.resolve_model_name("qwen3-0.6b");
    assert(resolved == "Qwen3-0.6B-Q4_K_M.gguf");
    
    // Test fallback behavior
    resolved = mgr.resolve_model_name("nonexistent-model");
    assert(resolved == "nonexistent-model.gguf");
    
    std::cout << "✓ Resolution tests passed" << std::endl;
}

void test_ui() {
    // Test UI initialization
    UI::init();
    
    // Test banner display (capture output)
    std::ostringstream oss;
    std::streambuf* old_cout = std::cout.rdbuf();
    std::cout.rdbuf(oss.rdbuf());
    
    UI::print_banner();
    
    std::cout.rdbuf(old_cout);
    std::string output = oss.str();
    
    // Check banner contains expected elements
    assert(output.find("DELTA") != std::string::npos);
    assert(output.find("Version 1.0.0") != std::string::npos);
    
    std::cout << "✓ UI tests passed" << std::endl;
}

int main() {
    std::cout << "Running default flow tests..." << std::endl;
    
    try {
        test_default_model();
        test_registry();
        test_resolution();
        test_ui();
        
        std::cout << "✓ All default flow tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed: " << e.what() << std::endl;
        return 1;
    }
}