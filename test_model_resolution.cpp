/**
 * Quick test to verify model name resolution works correctly
 * Compile: g++ -std=c++17 -I. test_model_resolution.cpp src/models.cpp src/tools/file_ops.cpp -o test_resolution
 */

#include "src/delta_cli.h"
#include <iostream>
#include <iomanip>

using namespace delta;

void test_resolution(ModelManager& mgr, const std::string& input, const std::string& expected) {
    std::string result = mgr.resolve_model_name(input);
    bool passed = (result == expected);
    
    std::cout << "  Input: " << std::setw(35) << std::left << input 
              << " -> " << result 
              << (passed ? " ✅" : " ❌ FAILED")
              << std::endl;
    
    if (!passed) {
        std::cout << "    Expected: " << expected << std::endl;
    }
}

void test_registry_lookup(ModelManager& mgr, const std::string& name, bool should_exist) {
    bool exists = mgr.is_in_registry(name);
    bool passed = (exists == should_exist);
    
    std::cout << "  Registry check: " << std::setw(30) << std::left << name 
              << " -> " << (exists ? "Found" : "Not found")
              << (passed ? " ✅" : " ❌ FAILED")
              << std::endl;
}

void test_model_info(ModelManager& mgr, const std::string& name) {
    auto entry = mgr.get_registry_entry(name);
    bool valid = !entry.name.empty();
    
    std::cout << "  Model info: " << name << std::endl;
    if (valid) {
        std::cout << "    Name: " << entry.name << std::endl;
        std::cout << "    Short name: " << entry.short_name << std::endl;
        std::cout << "    Filename: " << entry.filename << std::endl;
        std::cout << "    Repo: " << entry.repo_id << std::endl;
        std::cout << "    Size: " << (entry.size_bytes / (1024.0 * 1024.0)) << " MB" << std::endl;
        std::cout << "    ✅ Valid entry" << std::endl;
    } else {
        std::cout << "    ❌ Entry not found" << std::endl;
    }
}

int main() {
    std::cout << "=== Delta CLI Model Resolution Tests ===" << std::endl << std::endl;
    
    ModelManager mgr;
    
    // Test 1: Registry name resolution (colon notation)
    std::cout << "Test 1: Colon notation (registry .name)" << std::endl;
    test_resolution(mgr, "qwen3:0.6b", "Qwen3-0.6B-Q4_K_M.gguf");
    test_resolution(mgr, "qwen3:8b", "Qwen3-8B-Q4_K_M.gguf");
    test_resolution(mgr, "llama3:8b", "Meta-Llama-3-8B-Instruct-Q4_K_M.gguf");
    test_resolution(mgr, "gemma3:4b", "gemma-3-4b-it-qat-q4_0.gguf");
    test_resolution(mgr, "qwen2.5-coder:0.5b", "Qwen2.5-Coder-0.5B-Instruct-128K-Q4_K_M.gguf");
    std::cout << std::endl;
    
    // Test 2: Short name resolution (dash notation)
    std::cout << "Test 2: Dash notation (short_name)" << std::endl;
    test_resolution(mgr, "qwen3-0.6b", "Qwen3-0.6B-Q4_K_M.gguf");
    test_resolution(mgr, "qwen3-8b", "Qwen3-8B-Q4_K_M.gguf");
    test_resolution(mgr, "llama3-8b", "Meta-Llama-3-8B-Instruct-Q4_K_M.gguf");
    std::cout << std::endl;
    
    // Test 3: Auto-conversion (dash to colon)
    std::cout << "Test 3: Auto-conversion (dash -> colon)" << std::endl;
    test_resolution(mgr, "qwen2.5-coder-0.5b", "Qwen2.5-Coder-0.5B-Instruct-128K-Q4_K_M.gguf");
    test_resolution(mgr, "gemma3-4b", "gemma-3-4b-it-qat-q4_0.gguf");
    test_resolution(mgr, "deepseek-r1-8b", "DeepSeek-R1-Distill-Llama-8B-Q4_K_M.gguf");
    std::cout << std::endl;
    
    // Test 4: Direct filename
    std::cout << "Test 4: Direct filename" << std::endl;
    test_resolution(mgr, "Qwen3-0.6B-Q4_K_M.gguf", "Qwen3-0.6B-Q4_K_M.gguf");
    test_resolution(mgr, "custom-model.gguf", "custom-model.gguf");
    std::cout << std::endl;
    
    // Test 5: Registry lookup
    std::cout << "Test 5: Registry lookup" << std::endl;
    test_registry_lookup(mgr, "qwen3:0.6b", true);
    test_registry_lookup(mgr, "llama3:8b", true);
    test_registry_lookup(mgr, "nonexistent:999b", false);
    std::cout << std::endl;
    
    // Test 6: Get model info
    std::cout << "Test 6: Model info retrieval" << std::endl;
    test_model_info(mgr, "qwen3:0.6b");
    std::cout << std::endl;
    test_model_info(mgr, "qwen2.5-coder:7b");
    std::cout << std::endl;
    
    // Test 7: Registry size
    std::cout << "Test 7: Registry statistics" << std::endl;
    auto all_models = mgr.get_registry_models();
    std::cout << "  Total models in registry: " << all_models.size() << std::endl;
    std::cout << "  Expected: 52 models" << std::endl;
    std::cout << "  " << (all_models.size() == 52 ? "✅ Correct" : "❌ FAILED") << std::endl;
    std::cout << std::endl;
    
    // Test 8: Model list format
    std::cout << "Test 8: Model list format (first 5 models)" << std::endl;
    auto model_list = mgr.get_friendly_model_list(true);
    int count = 0;
    for (const auto& model : model_list) {
        if (count++ >= 5) break;
        std::cout << "  " << model.name << " (" << model.size_str << ")" << std::endl;
        
        // Check if name uses colon notation
        bool has_colon = model.name.find(':') != std::string::npos || 
                        model.name.find('-') == std::string::npos;
        if (!has_colon && model.name != "tinyllama" && model.name != "llava" && 
            model.name != "phi" && model.name != "phi2" && model.name != "phi4-mini" && 
            model.name != "bge-m3") {
            std::cout << "    ⚠️  Warning: Expected colon notation" << std::endl;
        }
    }
    std::cout << std::endl;
    
    // Test 9: Default model
    std::cout << "Test 9: Default model" << std::endl;
    std::string default_model = ModelManager::get_default_model();
    std::cout << "  Default model: " << default_model << std::endl;
    std::cout << "  Expected: qwen3:0.6b" << std::endl;
    std::cout << "  " << (default_model == "qwen3:0.6b" ? "✅ Correct" : "❌ FAILED") << std::endl;
    std::cout << std::endl;
    
    // Test 10: Special cases
    std::cout << "Test 10: Special model names (no colon)" << std::endl;
    test_resolution(mgr, "tinyllama", "TinyLlama-1.1B-Chat-v1.0-Q4_K_M.gguf");
    test_resolution(mgr, "llava", "Llava-v1.5-7B-Q4_K_M.gguf");
    test_resolution(mgr, "phi", "Phi-3-mini-4k-instruct-Q4_K_M.gguf");
    test_resolution(mgr, "phi2", "phi-2-Q4_K_M.gguf");
    test_resolution(mgr, "bge-m3", "bge-m3-Q4_K_M.gguf");
    std::cout << std::endl;
    
    std::cout << "=== All Tests Complete ===" << std::endl;
    
    return 0;
}

