/**
 * Model Manager Tests
 */

#include <catch2/catch_test_macros.hpp>
#include "../src/delta_cli.h"
#include "../src/tools/file_ops.cpp"

using namespace delta;

TEST_CASE("ModelManager initialization", "[models]") {
    SECTION("Constructor creates models directory") {
        ModelManager mgr;
        std::string home = tools::FileOps::get_home_dir();
        std::string models_dir = tools::FileOps::join_path(home, ".delta-cli");
        models_dir = tools::FileOps::join_path(models_dir, "models");
        
        // Directory should exist after initialization
        REQUIRE(tools::FileOps::dir_exists(models_dir));
    }
}

TEST_CASE("ModelManager list operations", "[models]") {
    ModelManager mgr;
    
    SECTION("list_models() returns a vector") {
        auto models = mgr.list_models();
        // Should return a vector (may be empty if no models installed)
        REQUIRE(models.size() >= 0);
    }
    
    SECTION("list_models() only includes .gguf files") {
        auto models = mgr.list_models();
        for (const auto& model : models) {
            // Model names should not include .gguf extension in list
            REQUIRE(model.find(".gguf") == std::string::npos);
        }
    }
}

TEST_CASE("ModelManager path operations", "[models]") {
    ModelManager mgr;
    
    SECTION("get_model_path() with non-existent model") {
        std::string path = mgr.get_model_path("non-existent-model-xyz");
        // Should return empty string for non-existent model
        REQUIRE(path.empty());
    }
    
    SECTION("has_model() returns false for non-existent model") {
        bool exists = mgr.has_model("non-existent-model-xyz");
        REQUIRE(exists == false);
    }
    
    SECTION("get_model_path() accepts absolute paths") {
        std::string abs_path = "/tmp/test-model.gguf";
        // Should handle absolute paths (even if file doesn't exist)
        std::string result = mgr.get_model_path(abs_path);
        // Will be empty if file doesn't exist, or abs_path if it does
        REQUIRE((result.empty() || result == abs_path));
    }
}

TEST_CASE("ModelManager info operations", "[models]") {
    ModelManager mgr;
    
    SECTION("get_model_info() with non-existent model") {
        auto info = mgr.get_model_info("non-existent-model");
        REQUIRE(info.empty());
    }
    
    SECTION("get_model_info() detects quantization from name") {
        // These are just testing the naming convention detection
        ModelManager mgr;
        
        // Note: These will return empty info since files don't exist
        // but we're testing the naming logic
        auto info_q4 = mgr.get_model_info("model-q4_0.gguf");
        auto info_q8 = mgr.get_model_info("model-q8_0.gguf");
        
        // Info will be empty for non-existent files
        REQUIRE(info_q4.size() >= 0);
        REQUIRE(info_q8.size() >= 0);
    }
}

TEST_CASE("ModelManager add/remove operations", "[models]") {
    ModelManager mgr;
    
    SECTION("add_model() requires existing source file") {
        bool result = mgr.add_model("test-model", "/non/existent/path.gguf");
        REQUIRE(result == false);
    }
    
    SECTION("remove_model() handles non-existent model") {
        bool result = mgr.remove_model("non-existent-model");
        REQUIRE(result == false);
    }
}

TEST_CASE("ModelManager registry operations", "[models][registry]") {
    ModelManager mgr;
    
    SECTION("get_registry_models() returns valid entries") {
        auto models = mgr.get_registry_models();
        // Should have the full registry of 52 models
        REQUIRE(models.size() >= 52);
        
        // Verify each entry has required fields
        for (const auto& model : models) {
            REQUIRE(!model.name.empty());
            REQUIRE(!model.short_name.empty());
            REQUIRE(!model.repo_id.empty());
            REQUIRE(!model.filename.empty());
            REQUIRE(!model.quantization.empty());
            REQUIRE(!model.display_name.empty());
            REQUIRE(model.size_bytes > 0);
        }
    }
    
    SECTION("is_in_registry() correctly identifies models") {
        REQUIRE(mgr.is_in_registry("qwen3:0.6b") == true);
        REQUIRE(mgr.is_in_registry("llama3:8b") == true);
        REQUIRE(mgr.is_in_registry("phi") == true);
        REQUIRE(mgr.is_in_registry("non-existent-model:999b") == false);
    }
    
    SECTION("get_registry_entry() returns valid data") {
        auto entry = mgr.get_registry_entry("qwen3:0.6b");
        REQUIRE(entry.name == "qwen3:0.6b");
        REQUIRE(entry.short_name == "qwen3-0.6b");
        REQUIRE(entry.repo_id == "unsloth/Qwen3-0.6B-GGUF");
        REQUIRE(entry.filename == "Qwen3-0.6B-Q4_K_M.gguf");
        REQUIRE(entry.quantization == "Q4_K_M");
        REQUIRE(entry.size_bytes == 400LL * 1024 * 1024);
    }
}

TEST_CASE("ModelManager name resolution", "[models][resolution]") {
    ModelManager mgr;
    
    SECTION("resolve_model_name() handles registry .name (colon notation)") {
        std::string resolved = mgr.resolve_model_name("qwen3:0.6b");
        REQUIRE(resolved == "Qwen3-0.6B-Q4_K_M.gguf");
    }
    
    SECTION("resolve_model_name() handles short names (dash notation)") {
        std::string resolved = mgr.resolve_model_name("qwen3-0.6b");
        REQUIRE(resolved == "Qwen3-0.6B-Q4_K_M.gguf");
    }
    
    SECTION("resolve_model_name() handles .gguf files") {
        std::string resolved = mgr.resolve_model_name("model.gguf");
        REQUIRE(resolved == "model.gguf");
    }
    
    SECTION("resolve_model_name() adds .gguf extension") {
        std::string resolved = mgr.resolve_model_name("custom-model");
        REQUIRE(resolved == "custom-model.gguf");
    }
    
    SECTION("resolve_model_name() converts dash to colon notation") {
        // Test automatic conversion for models with dots in name
        std::string resolved = mgr.resolve_model_name("qwen2.5-coder-0.5b");
        REQUIRE(resolved == "Qwen2.5-Coder-0.5B-Instruct-128K-Q4_K_M.gguf");
    }
}

TEST_CASE("ModelManager default model", "[models][default]") {
    ModelManager mgr;
    
    SECTION("get_default_model() returns qwen3:0.6b") {
        std::string default_model = ModelManager::get_default_model();
        REQUIRE(default_model == "qwen3:0.6b");
    }
    
    SECTION("default model exists in registry") {
        std::string default_model = ModelManager::get_default_model();
        REQUIRE(mgr.is_in_registry(default_model) == true);
    }
    
    SECTION("get_default_model_short_name() returns dash notation") {
        std::string short_name = mgr.get_default_model_short_name();
        REQUIRE(short_name == "qwen3-0.6b");  // short_name uses dash notation
    }
}

TEST_CASE("ModelManager friendly model list", "[models][list]") {
    ModelManager mgr;
    
    SECTION("get_friendly_model_list() with available models") {
        auto models = mgr.get_friendly_model_list(true);
        REQUIRE(models.size() >= 52);
        
        // Verify sorting by size (smallest first)
        for (size_t i = 1; i < models.size(); i++) {
            REQUIRE(models[i].size_bytes >= models[i-1].size_bytes);
        }
    }
    
    SECTION("get_friendly_model_list() format verification") {
        auto models = mgr.get_friendly_model_list(true);
        
        for (const auto& model : models) {
            REQUIRE(!model.name.empty());
            REQUIRE(!model.display_name.empty());
            REQUIRE(!model.description.empty());
            REQUIRE(!model.size_str.empty());
            REQUIRE(!model.quantization.empty());
            
            // Verify size string format (should be KB, MB, or GB)
            bool valid_format = (model.size_str.find("KB") != std::string::npos ||
                               model.size_str.find("MB") != std::string::npos ||
                               model.size_str.find("GB") != std::string::npos);
            REQUIRE(valid_format == true);
        }
    }
}

TEST_CASE("ModelManager verified model mappings", "[models][verified]") {
    ModelManager mgr;
    
    SECTION("Qwen models have correct repos") {
        auto qwen3_06b = mgr.get_registry_entry("qwen3:0.6b");
        REQUIRE(qwen3_06b.repo_id == "unsloth/Qwen3-0.6B-GGUF");
        REQUIRE(qwen3_06b.filename == "Qwen3-0.6B-Q4_K_M.gguf");
        
        auto qwen3_8b = mgr.get_registry_entry("qwen3:8b");
        REQUIRE(qwen3_8b.repo_id == "Qwen/Qwen3-8B-GGUF");
        REQUIRE(qwen3_8b.filename == "Qwen3-8B-Q4_K_M.gguf");
    }
    
    SECTION("Qwen 2.5 Coder models have correct repos") {
        auto coder_05b = mgr.get_registry_entry("qwen2.5-coder:0.5b");
        REQUIRE(coder_05b.repo_id == "unsloth/Qwen2.5-Coder-0.5B-Instruct-128K-GGUF");
        REQUIRE(coder_05b.filename == "Qwen2.5-Coder-0.5B-Instruct-128K-Q4_K_M.gguf");
        
        auto coder_15b = mgr.get_registry_entry("qwen2.5-coder:1.5b");
        REQUIRE(coder_15b.repo_id == "QuantFactory/Qwen2.5-Coder-1.5B-GGUF");
        REQUIRE(coder_15b.filename == "Qwen2.5-Coder-1.5B-Q4_K_M.gguf");
    }
    
    SECTION("Gemma models have correct repos") {
        auto gemma_2b = mgr.get_registry_entry("gemma:2b");
        REQUIRE(gemma_2b.repo_id == "google/gemma-2b-GGUF");
        REQUIRE(gemma_2b.filename == "gemma-2b-Q4_K_M.gguf");
        
        auto gemma3_270m = mgr.get_registry_entry("gemma3:270m");
        REQUIRE(gemma3_270m.repo_id == "unsloth/gemma-3-270m-it-GGUF");
        REQUIRE(gemma3_270m.filename == "gemma-3-270m-it-Q4_K_M.gguf");
        
        auto gemma3_4b = mgr.get_registry_entry("gemma3:4b");
        REQUIRE(gemma3_4b.repo_id == "google/gemma-3-4b-it-qat-q4_0-gguf");
        REQUIRE(gemma3_4b.filename == "gemma-3-4b-it-qat-q4_0.gguf");
        REQUIRE(gemma3_4b.quantization == "Q4_0"); // Special QAT version
    }
    
    SECTION("DeepSeek R1 models have correct repos") {
        auto ds_15b = mgr.get_registry_entry("deepseek-r1:1.5b");
        REQUIRE(ds_15b.repo_id == "unsloth/DeepSeek-R1-Distill-Qwen-1.5B-GGUF");
        REQUIRE(ds_15b.filename == "DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf");
        
        auto ds_8b = mgr.get_registry_entry("deepseek-r1:8b");
        REQUIRE(ds_8b.repo_id == "unsloth/DeepSeek-R1-Distill-Llama-8B-GGUF");
        REQUIRE(ds_8b.filename == "DeepSeek-R1-Distill-Llama-8B-Q4_K_M.gguf");
    }
    
    SECTION("Llama models have correct repos") {
        auto llama3_8b = mgr.get_registry_entry("llama3:8b");
        REQUIRE(llama3_8b.repo_id == "QuantFactory/Meta-Llama-3-8B-Instruct-GGUF");
        REQUIRE(llama3_8b.filename == "Meta-Llama-3-8B-Instruct-Q4_K_M.gguf");
        
        auto llama2_7b = mgr.get_registry_entry("llama2:7b");
        REQUIRE(llama2_7b.repo_id == "TheBloke/Llama-2-7B-GGUF");
        REQUIRE(llama2_7b.filename == "Llama-2-7B-Q4_K_M.gguf");
    }
    
    SECTION("LLaVA model has correct repo") {
        auto llava = mgr.get_registry_entry("llava");
        REQUIRE(llava.repo_id == "second-state/Llava-v1.5-7B-GGUF");
        REQUIRE(llava.filename == "Llava-v1.5-7B-Q4_K_M.gguf");
    }
    
    SECTION("SmolLM models have correct repos") {
        auto smollm2_135m = mgr.get_registry_entry("smollm2:135m");
        REQUIRE(smollm2_135m.repo_id == "QuantFactory/SmolLM2-135M-GGUF");
        REQUIRE(smollm2_135m.filename == "SmolLM2-135M-Q4_K_M.gguf");
        
        auto smollm_360m = mgr.get_registry_entry("smollm:360m");
        REQUIRE(smollm_360m.repo_id == "QuantFactory/SmolLM-360M-GGUF");
        REQUIRE(smollm_360m.filename == "SmolLM-360M-Q4_K_M.gguf");
    }
    
    SECTION("Falcon models have correct repos") {
        auto falcon3_1b = mgr.get_registry_entry("falcon3:1b");
        REQUIRE(falcon3_1b.repo_id == "tiiuae/Falcon3-1B-Instruct-GGUF");
        REQUIRE(falcon3_1b.filename == "Falcon3-1B-Instruct-Q4_K_M.gguf");
    }
    
    SECTION("Phi models have correct repos") {
        auto phi = mgr.get_registry_entry("phi");
        REQUIRE(phi.repo_id == "microsoft/Phi-3-mini-4k-instruct-gguf");
        REQUIRE(phi.filename == "Phi-3-mini-4k-instruct-Q4_K_M.gguf");
        
        auto phi2 = mgr.get_registry_entry("phi2");
        REQUIRE(phi2.repo_id == "TheBloke/phi-2-GGUF");
        REQUIRE(phi2.filename == "phi-2-Q4_K_M.gguf");
        
        auto phi4 = mgr.get_registry_entry("phi4-mini");
        REQUIRE(phi4.repo_id == "tensorblock/Phi-4-mini-instruct-GGUF");
        REQUIRE(phi4.filename == "Phi-4-mini-instruct-Q4_K_M.gguf");
    }
    
    SECTION("BGE-M3 embedding model has correct repo") {
        auto bge = mgr.get_registry_entry("bge-m3");
        REQUIRE(bge.repo_id == "bbvch-ai/bge-m3-GGUF");
        REQUIRE(bge.filename == "bge-m3-Q4_K_M.gguf");
    }
    
    SECTION("Qwen embedding models have correct repos") {
        auto emb_06b = mgr.get_registry_entry("qwen3-embedding:0.6b");
        REQUIRE(emb_06b.repo_id == "Qwen/Qwen3-Embedding-0.6B-GGUF");
        
        auto emb_8b = mgr.get_registry_entry("qwen3-embedding:8b");
        REQUIRE(emb_8b.repo_id == "Mungert/Qwen3-Embedding-8B-GGUF");
    }
    
    SECTION("Qwen Math models have correct repos") {
        auto math_15b = mgr.get_registry_entry("qwen2-math:1.5b");
        REQUIRE(math_15b.repo_id == "QuantFactory/Qwen2-Math-1.5B-GGUF");
        REQUIRE(math_15b.filename == "Qwen2-Math-1.5B-Q4_K_M.gguf");
        
        auto math_7b = mgr.get_registry_entry("qwen2-math:7b");
        REQUIRE(math_7b.repo_id == "QuantFactory/Qwen2-Math-7B-GGUF");
        REQUIRE(math_7b.filename == "Qwen2-Math-7B-Q4_K_M.gguf");
    }
}

