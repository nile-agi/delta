/**
 * Inference Engine Tests
 */

#include <catch2/catch_test_macros.hpp>
#include "../src/delta_cli.h"

using namespace delta;

TEST_CASE("InferenceEngine initialization", "[inference]") {
    SECTION("Constructor succeeds") {
        REQUIRE_NOTHROW({
            InferenceEngine engine;
        });
    }
    
    SECTION("New engine is not loaded") {
        InferenceEngine engine;
        REQUIRE(engine.is_loaded() == false);
    }
}

TEST_CASE("InferenceConfig defaults", "[inference]") {
    SECTION("Default configuration values") {
        InferenceConfig config;
        REQUIRE(config.n_ctx == 4096);
        REQUIRE(config.n_batch == 512);
        REQUIRE(config.n_threads == 4);
        REQUIRE(config.n_gpu_layers == 0);
        REQUIRE(config.temperature > 0.0f);
        REQUIRE(config.use_mmap == true);
        REQUIRE(config.multimodal == false);
    }
}

TEST_CASE("InferenceEngine load operations", "[inference]") {
    InferenceEngine engine;
    InferenceConfig config;
    config.model_path = "/non/existent/model.gguf";
    
    SECTION("load_model() fails with invalid path") {
        bool result = engine.load_model(config);
        REQUIRE(result == false);
        REQUIRE(engine.is_loaded() == false);
    }
    
    SECTION("unload_model() doesn't crash on unloaded engine") {
        REQUIRE_NOTHROW(engine.unload_model());
    }
}

TEST_CASE("InferenceEngine tokenization", "[inference]") {
    InferenceEngine engine;
    
    SECTION("tokenize() requires loaded model") {
        REQUIRE_THROWS(engine.tokenize("test text"));
    }
    
    SECTION("detokenize() requires loaded model") {
        std::vector<int> tokens = {1, 2, 3};
        REQUIRE_THROWS(engine.detokenize(tokens));
    }
}

TEST_CASE("InferenceEngine generation", "[inference]") {
    InferenceEngine engine;
    
    SECTION("generate() requires loaded model") {
        REQUIRE_THROWS(engine.generate("test prompt"));
    }
    
    SECTION("generate_multimodal() not yet implemented") {
        std::vector<std::string> images;
        REQUIRE_THROWS(engine.generate_multimodal("prompt", images));
    }
}

TEST_CASE("InferenceEngine info methods", "[inference]") {
    InferenceEngine engine;
    
    SECTION("get_model_name() returns empty when not loaded") {
        std::string name = engine.get_model_name();
        REQUIRE(name.empty());
    }
    
    SECTION("get_model_size() returns 0 when not loaded") {
        size_t size = engine.get_model_size();
        REQUIRE(size == 0);
    }
    
    SECTION("get_context_size() returns 0 when not loaded") {
        int ctx_size = engine.get_context_size();
        REQUIRE(ctx_size == 0);
    }
}

// Note: Full integration tests would require an actual model file
// These tests focus on API behavior without requiring model files
TEST_CASE("InferenceEngine integration scenarios", "[inference][integration]") {
    SECTION("Load, unload, reload cycle") {
        InferenceEngine engine;
        InferenceConfig config;
        config.model_path = "/non/existent.gguf";
        
        // Try to load (will fail)
        engine.load_model(config);
        REQUIRE(engine.is_loaded() == false);
        
        // Unload
        engine.unload_model();
        REQUIRE(engine.is_loaded() == false);
        
        // Try to load again
        engine.load_model(config);
        REQUIRE(engine.is_loaded() == false);
    }
}

