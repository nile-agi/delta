/**
 * Inference Module - Modern llama.cpp integration for Delta CLI
 * Based on llama.cpp/tools/run/run.cpp implementation
 */

#include "delta_cli.h"
#include <iostream>
#include <stdexcept>
#include <memory>
#include <vector>
#include <list>

// Modern llama.cpp headers
#include "llama.h"
#include <limits>

namespace delta {

// Custom log callback to suppress verbose llama.cpp logs
static void llama_log_callback(enum ggml_log_level level, const char* text, void* user_data) {
    (void)user_data; // Suppress unused parameter warning
    
    // Suppress all logs except errors
    if (level == GGML_LOG_LEVEL_ERROR) {
        std::cerr << text;
    }
}

// Helper function to convert token to string (from run.cpp)
static int convert_token_to_string(const llama_vocab* vocab, const llama_token token_id, std::string& piece) {
    char buf[256];
    int n = llama_token_to_piece(vocab, token_id, buf, sizeof(buf), 0, true);
    if (n < 0) {
        return 1;
    }
    piece = std::string(buf, n);
    return 0;
}

// Helper function to print and concatenate response (from run.cpp)
static void print_word_and_concatenate_to_response(const std::string& piece, std::string& response) {
    printf("%s", piece.c_str());
    fflush(stdout);
    response += piece;
}


InferenceEngine::InferenceEngine() 
    : model_(nullptr), ctx_(nullptr), sampler_(nullptr) {
    // Set custom log callback to suppress verbose output
    llama_log_set(llama_log_callback, nullptr);
    // Initialize llama.cpp backend
    llama_backend_init();
}

InferenceEngine::~InferenceEngine() {
    unload_model();
    llama_backend_free();
}

bool InferenceEngine::load_model(const InferenceConfig& config) {
    unload_model();
    
    config_ = config;
    
    // Set up model parameters
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = config.n_gpu_layers;
    model_params.use_mmap = config.use_mmap;
    model_params.use_mlock = config.use_mlock;
    
    // Load model
    model_ = llama_model_load_from_file(config.model_path.c_str(), model_params);
    if (!model_) {
        UI::print_error("Failed to load model: " + config.model_path);
        return false;
    }
    
    // Set up context parameters
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = config.n_ctx;
    ctx_params.n_batch = config.n_batch;
    ctx_params.n_threads = config.n_threads;
    ctx_params.n_threads_batch = config.n_threads;
    
    // Create context
    ctx_ = llama_init_from_model(model_, ctx_params);
    if (!ctx_) {
        UI::print_error("Failed to create context");
        llama_model_free(model_);
        model_ = nullptr;
        return false;
    }
    
    // Set up sampler
    setup_sampler();
    
    
    return true;
}

void InferenceEngine::unload_model() {
    if (sampler_) {
        llama_sampler_free(sampler_);
        sampler_ = nullptr;
    }
    
    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    
    if (model_) {
        llama_model_free(model_);
        model_ = nullptr;
    }
    
}

void InferenceEngine::setup_sampler() {
    if (sampler_) {
        llama_sampler_free(sampler_);
    }
    
    // Create sampler chain
    sampler_ = llama_sampler_chain_init(llama_sampler_chain_default_params());
    
    // Add sampling strategies
    llama_sampler_chain_add(sampler_, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(sampler_, llama_sampler_init_temp(config_.temperature));
    llama_sampler_chain_add(sampler_, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
}


std::vector<int> InferenceEngine::tokenize(const std::string& text, bool add_bos) {
    if (!model_ || !ctx_) {
        throw std::runtime_error("Model not loaded");
    }

    const llama_vocab* vocab = llama_model_get_vocab(model_);

    // Mirror tools/run/run.cpp logic: add BOS only for the first prompt in the session
    const bool is_first = llama_memory_seq_pos_max(llama_get_memory(ctx_), 0) == -1;
    const bool add_bos_effective = add_bos && is_first;

    // Allocate an initial buffer; llama_tokenize returns negative if buffer too small
    int estimated = static_cast<int>(text.size()) + (add_bos_effective ? 2 : 1);
    std::vector<llama_token> buf(std::max(estimated, 8));

    int n = llama_tokenize(
        vocab,
        text.c_str(),
        static_cast<int>(text.size()),
        buf.data(),
        static_cast<int>(buf.size()),
        add_bos_effective,
        /*parse_special=*/true);

    if (n == std::numeric_limits<int32_t>::min()) {
        throw std::runtime_error("Tokenization failed: input too large");
    }

    if (n < 0) {
        buf.resize(-n);
        int check = llama_tokenize(
            vocab,
            text.c_str(),
            static_cast<int>(text.size()),
            buf.data(),
            static_cast<int>(buf.size()),
            add_bos_effective,
            /*parse_special=*/true);
        if (check != -n) {
            throw std::runtime_error("Tokenization failed: size mismatch");
        }
        n = check;
    } else {
        buf.resize(n);
    }

    return std::vector<int>(buf.begin(), buf.end());
}

std::string InferenceEngine::detokenize(const std::vector<int>& tokens) {
    if (!model_) {
        throw std::runtime_error("Model not loaded");
    }
    
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    std::string result;
    
    for (int token : tokens) {
        char buf[256];
        int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
        if (n > 0) {
            result += std::string(buf, n);
        }
    }
    
    return result;
}

std::string InferenceEngine::generate(const std::string& prompt, int max_tokens, bool stream) {
    if (!is_loaded()) {
        throw std::runtime_error("Model not loaded");
    }
    
    // Use a simple, direct prompt without modifications
    std::string clean_prompt = prompt;
    
    // Tokenize prompt
    auto tokens = tokenize(clean_prompt, true);
    return generate_internal(tokens, max_tokens, stream);
}

std::string InferenceEngine::generate_internal(const std::vector<int>& tokens, 
                                               int max_tokens, 
                                               bool stream) {
    if (!is_loaded()) {
        throw std::runtime_error("Model not loaded");
    }
    
    // Clear memory to ensure clean context
    llama_memory_clear(llama_get_memory(ctx_), true);
    
    // Convert to llama_token
    std::vector<llama_token> prompt_tokens(tokens.begin(), tokens.end());
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    
    // Reset sampler
    llama_sampler_reset(sampler_);
    
    // Create batch for prompt
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());
    // Basic ctx capacity check similar to tools/run/run.cpp
    {
        const int n_ctx = llama_n_ctx(ctx_);
        const int n_ctx_used = llama_memory_seq_pos_max(llama_get_memory(ctx_), 0);
        if (n_ctx_used + batch.n_tokens > n_ctx) {
            throw std::runtime_error("Context size exceeded while submitting prompt");
        }
    }
    
    // Evaluate prompt
    if (llama_decode(ctx_, batch)) {
        throw std::runtime_error("Failed to evaluate prompt");
    }
    
    std::string response;
    
    // Generate tokens with aggressive stopping for concise responses
    for (int i = 0; i < max_tokens; i++) {
        // Sample next token
        llama_token token = llama_sampler_sample(sampler_, ctx_, -1);
        
        // Check for EOS token
        if (llama_vocab_is_eog(vocab, token)) {
            break;
        }
        
        // Convert token to string
        std::string piece;
        if (convert_token_to_string(vocab, token, piece)) {
            break;
        }
        
        // Print and concatenate response
        if (stream) {
            print_word_and_concatenate_to_response(piece, response);
        } else {
            response += piece;
        }
        
        // Aggressive early stopping for concise responses
        if (response.length() > 100) {
            // Check for natural stopping points
            char last_char = response.back();
            if (last_char == '.' || last_char == '!' || last_char == '?') {
                // If we have a complete sentence and enough content, stop
                if (response.length() > 50) {
                    break;
                }
            }
        }
        
        // Check for repetitive patterns to prevent infinite loops
        if (response.length() > 150) {
            std::string last_50 = response.substr(response.length() - 50);
            std::string prev_50 = response.substr(response.length() - 100, 50);
            if (last_50 == prev_50) {
                break;
            }
        }
        
        // Accept the token
        llama_sampler_accept(sampler_, token);
        
        // Prepare next batch
        llama_batch next_batch = llama_batch_get_one(&token, 1);
        {
            const int n_ctx = llama_n_ctx(ctx_);
            const int n_ctx_used = llama_memory_seq_pos_max(llama_get_memory(ctx_), 0);
            if (n_ctx_used + next_batch.n_tokens > n_ctx) {
                break;
            }
        }
        if (llama_decode(ctx_, next_batch)) {
            break;
        }
    }
    
    return response;
}

std::string InferenceEngine::generate_multimodal(const std::string& prompt,
                                                 const std::vector<std::string>& image_paths,
                                                 int max_tokens,
                                                 bool stream) {
    (void)image_paths; // Suppress unused parameter warning
    // For now, just generate text without image processing
    // This can be extended later for true multimodal support
    return generate(prompt, max_tokens, stream);
}



} // namespace delta