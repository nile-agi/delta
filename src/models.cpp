/**
 * Models Module - Model management for Delta CLI
 */

#include "delta_cli.h"
#include <algorithm>
#include <sys/stat.h>
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cctype>

namespace delta {

// Define the default model (qwen3:0.6b - 400 MB, ultra-compact multilingual)
const std::string ModelManager::DEFAULT_MODEL_NAME = "qwen3:0.6b";

ModelManager::ModelManager() : progress_callback_(nullptr) {
    std::string home = tools::FileOps::get_home_dir();
    models_dir_ = tools::FileOps::join_path(home, ".delta-cli");
    models_dir_ = tools::FileOps::join_path(models_dir_, "models");
    ensure_models_dir();
    init_model_registry();
}

ModelManager::~ModelManager() {
}

void ModelManager::ensure_models_dir() {
    if (!tools::FileOps::dir_exists(models_dir_)) {
        tools::FileOps::create_dir(models_dir_);
    }
}

std::vector<std::string> ModelManager::list_models() {
    std::vector<std::string> models;
    
    if (!tools::FileOps::dir_exists(models_dir_)) {
        return models;
    }
    
    auto files = tools::FileOps::list_dir(models_dir_);
    
    for (const auto& file : files) {
        // Only include .gguf files
        if (file.length() > 5 && file.substr(file.length() - 5) == ".gguf") {
            models.push_back(file.substr(0, file.length() - 5));
        }
    }
    
    std::sort(models.begin(), models.end());
    return models;
}

bool ModelManager::has_model(const std::string& model_name) {
    std::string path = get_model_path(model_name);
    return !path.empty() && tools::FileOps::file_exists(path);
}

std::string ModelManager::get_model_path(const std::string& model_name) {
    // Try direct path first (absolute or relative)
    if (tools::FileOps::file_exists(model_name)) {
        return model_name;
    }
    
    // Resolve short name to full filename
    std::string filename = resolve_model_name(model_name);
    
    // Check in models directory
    std::string full_path = tools::FileOps::join_path(models_dir_, filename);
    if (tools::FileOps::file_exists(full_path)) {
        return full_path;
    }
    
    // If still not found, try the original input with .gguf
    std::string with_ext = model_name;
    if (model_name.length() < 5 || model_name.substr(model_name.length() - 5) != ".gguf") {
        with_ext += ".gguf";
    }
    full_path = tools::FileOps::join_path(models_dir_, with_ext);
    if (tools::FileOps::file_exists(full_path)) {
        return full_path;
    }
    
    return "";
}

bool ModelManager::add_model(const std::string& model_name, const std::string& file_path) {
    ensure_models_dir();
    
    if (!tools::FileOps::file_exists(file_path)) {
        return false;
    }
    
    std::string dest_name = model_name;
    if (dest_name.length() < 5 || dest_name.substr(dest_name.length() - 5) != ".gguf") {
        dest_name += ".gguf";
    }
    
    std::string dest_path = tools::FileOps::join_path(models_dir_, dest_name);
    
    // Use streaming copy for large files instead of loading into memory
    std::ifstream src(file_path, std::ios::binary);
    std::ofstream dst(dest_path, std::ios::binary);
    
    if (!src || !dst) {
        return false;
    }
    
    // Copy in chunks to avoid memory issues with large models
    constexpr size_t buffer_size = 1024 * 1024; // 1 MB chunks
    char buffer[buffer_size];
    
    while (src.read(buffer, buffer_size) || src.gcount() > 0) {
        dst.write(buffer, src.gcount());
        if (!dst) {
            src.close();
            dst.close();
            std::remove(dest_path.c_str()); // Clean up partial file
            return false;
        }
    }
    
    src.close();
    dst.close();
    
    return true;
}

bool ModelManager::remove_model(const std::string& model_name) {
    std::string path = get_model_path(model_name);
    if (path.empty()) {
        return false;
    }
    
    return std::remove(path.c_str()) == 0;
}

bool ModelManager::remove_model_with_confirmation(const std::string& model_name) {
    // Resolve model name (handle short names like "qwen3:0.6b")
    std::string resolved_name = resolve_model_name(model_name);
    if (resolved_name.empty()) {
        UI::print_error("Model '" + model_name + "' not found");
        UI::print_info("Use 'delta --list-models' to see installed models");
        return false;
    }
    
    // Check if model exists
    if (!has_model(resolved_name)) {
        UI::print_error("Model '" + model_name + "' is not installed locally");
        UI::print_info("Use 'delta --list-models' to see installed models");
        return false;
    }
    
    // Get model info for confirmation
    std::string path = get_model_path(resolved_name);
    auto info = get_model_info(resolved_name);
    
    // Display model info
    UI::print_border("CONFIRM MODEL DELETION");
    UI::print_info("Model: " + resolved_name);
    if (!info.empty() && info.find("size") != info.end()) {
        UI::print_info("Size: " + info["size"]);
    }
    UI::print_info("Path: " + path);
    std::cout << std::endl;
    
    // Confirmation prompt
    UI::print_warning("This action cannot be undone!");
    std::cout << "Are you sure you want to delete this model? (y/N): " << std::flush;
    
    std::string response;
    std::getline(std::cin, response);
    
    // Trim whitespace and convert to lowercase
    response.erase(0, response.find_first_not_of(" \t\n\r"));
    response.erase(response.find_last_not_of(" \t\n\r") + 1);
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);
    
    if (response != "y" && response != "yes") {
        UI::print_info("Deletion cancelled");
        return false;
    }
    
    // Perform deletion
    bool success = remove_model(resolved_name);
    if (success) {
        UI::print_success("Model '" + resolved_name + "' deleted successfully");
    } else {
        UI::print_error("Failed to delete model '" + resolved_name + "'");
        UI::print_info("Check file permissions and try again");
    }
    
    return success;
}

std::map<std::string, std::string> ModelManager::get_model_info(const std::string& model_name) {
    std::map<std::string, std::string> info;
    
    std::string path = get_model_path(model_name);
    if (path.empty()) {
        return info;
    }
    
    // Get file size
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        double size_mb = st.st_size / (1024.0 * 1024.0);
        double size_gb = size_mb / 1024.0;
        
        char buffer[64];
        if (size_gb >= 1.0) {
            snprintf(buffer, sizeof(buffer), "%.2f GB", size_gb);
        } else {
            snprintf(buffer, sizeof(buffer), "%.2f MB", size_mb);
        }
        info["size"] = buffer;
        info["path"] = path;
    }
    
    // Try to detect quantization from filename
    std::string lower_name = model_name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    if (lower_name.find("q4_0") != std::string::npos) {
        info["quantization"] = "Q4_0";
    } else if (lower_name.find("q4_1") != std::string::npos) {
        info["quantization"] = "Q4_1";
    } else if (lower_name.find("q5_0") != std::string::npos) {
        info["quantization"] = "Q5_0";
    } else if (lower_name.find("q5_1") != std::string::npos) {
        info["quantization"] = "Q5_1";
    } else if (lower_name.find("q8_0") != std::string::npos) {
        info["quantization"] = "Q8_0";
    } else if (lower_name.find("f16") != std::string::npos) {
        info["quantization"] = "F16";
    } else if (lower_name.find("f32") != std::string::npos) {
        info["quantization"] = "F32";
    }
    
    return info;
}

// ============================================================================
// NEW: Download functionality implementation
// ============================================================================

void ModelManager::init_model_registry() {
    // Enhanced registry with short names and friendly display
    // Format: {name, short_name, repo_id, filename, quant, size, description, display_name}
    // Updated with verified HuggingFace repositories as of v1.0.0
    
    // ===== QWEN 3 SERIES (Latest generation) =====
    model_registry_["qwen3:0.6b"] = {
        "qwen3:0.6b",
        "qwen3-0.6b",
        "unsloth/Qwen3-0.6B-GGUF",
        "Qwen3-0.6B-Q4_K_M.gguf",
        "Q4_K_M",
        400LL * 1024 * 1024,      // ~400 MB
        "Ultra-compact multilingual model",
        "Qwen 3 0.6B"
    };
    
    model_registry_["qwen3:1.7b"] = {
        "qwen3:1.7b",
        "qwen3-1.7b",
        "unsloth/Qwen3-1.7B-GGUF",
        "Qwen3-1.7B-Q4_K_M.gguf",
        "Q4_K_M",
        1126LL * 1024 * 1024,     // ~1.1 GB
        "Efficient small multilingual model",
        "Qwen 3 1.7B"
    };
    
    model_registry_["qwen3:4b"] = {
        "qwen3:4b",
        "qwen3-4b",
        "unsloth/Qwen3-4B-GGUF",
        "Qwen3-4B-Q4_K_M.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Balanced multilingual reasoning model",
        "Qwen 3 4B"
    };
    
    model_registry_["qwen3:8b"] = {
        "qwen3:8b",
        "qwen3-8b",
        "Qwen/Qwen3-8B-GGUF",
        "Qwen3-8B-Q4_K_M.gguf",
        "Q4_K_M",
        4915LL * 1024 * 1024,     // ~4.8 GB
        "Powerful multilingual instruct model",
        "Qwen 3 8B"
    };
    
    // ===== QWEN 3 VL (Vision-Language) INSTRUCT from NexaAI =====
    model_registry_["qwen3-vl:4b-instruct"] = {
        "qwen3-vl:4b",
        "qwen3-vl-4b-instruct",
        "NexaAI/Qwen3-VL-4B-Instruct-GGUF",
        "Qwen3-VL-4B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4000LL * 1024 * 1024,     // ~4.0 GB (approx)
        "Qwen3-VL 4B Instruct vision-language model",
        "Qwen3-VL 4B Instruct"
    };
    
    model_registry_["qwen3-vl:8b-instruct"] = {
        "qwen3-vl:8b",
        "qwen3-vl-8b-instruct",
        "NexaAI/Qwen3-VL-8B-Instruct-GGUF",
        "Qwen3-VL-8B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        8000LL * 1024 * 1024,     // ~8.0 GB (approx)
        "Qwen3-VL 8B Instruct vision-language model",
        "Qwen3-VL 8B Instruct"
    };
    
    // ===== QWEN 2.5 CODER SERIES (Code-specialized) =====
    model_registry_["qwen2.5-coder:0.5b"] = {
        "qwen2.5-coder:0.5b",
        "qwen2.5-coder-0.5b",
        "unsloth/Qwen2.5-Coder-0.5B-Instruct-128K-GGUF",
        "Qwen2.5-Coder-0.5B-Instruct-128K-Q4_K_M.gguf",
        "Q4_K_M",
        352LL * 1024 * 1024,      // ~352 MB
        "Tiny code generation model",
        "Qwen 2.5 Coder 0.5B"
    };
    
    model_registry_["qwen2.5-coder:1.5b"] = {
        "qwen2.5-coder:1.5b",
        "qwen2.5-coder-1.5b",
        "QuantFactory/Qwen2.5-Coder-1.5B-GGUF",
        "Qwen2.5-Coder-1.5B-Q4_K_M.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Small code-focused model",
        "Qwen 2.5 Coder 1.5B"
    };
    
    model_registry_["qwen2.5-coder:3b"] = {
        "qwen2.5-coder:3b",
        "qwen2.5-coder-3b",
        "Qwen/Qwen2.5-Coder-3B-Instruct-GGUF",
        "Qwen2.5-Coder-3B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        1946LL * 1024 * 1024,     // ~1.9 GB
        "Balanced coding assistant",
        "Qwen 2.5 Coder 3B"
    };
    
    model_registry_["qwen2.5-coder:7b"] = {
        "qwen2.5-coder:7b",
        "qwen2.5-coder-7b",
        "Qwen/Qwen2.5-Coder-7B-Instruct-GGUF",
        "Qwen2.5-Coder-7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced code generation model",
        "Qwen 2.5 Coder 7B"
    };
    
    // ===== QWEN 2.5 SERIES (Latest instruct models) =====
    model_registry_["qwen2.5:0.5b"] = {
        "qwen2.5:0.5b",
        "qwen2.5-0.5b",
        "Qwen/Qwen2.5-0.5B-Instruct-GGUF",
        "qwen2.5-0.5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        350LL * 1024 * 1024,       // ~0.35 GB
        "Ultra-compact instruct model",
        "Qwen 2.5 0.5B"
    };
    
    model_registry_["qwen2.5:1.5b"] = {
        "qwen2.5:1.5b",
        "qwen2.5-1.5b",
        "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
        "qwen2.5-1.5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,      // ~1 GB
        "Small instruct model for edge devices",
        "Qwen 2.5 1.5B"
    };
    
    model_registry_["qwen2.5:3b"] = {
        "qwen2.5:3b",
        "qwen2.5-3b",
        "Qwen/Qwen2.5-3B-Instruct-GGUF",
        "qwen2.5-3b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        2048LL * 1024 * 1024,      // ~2 GB
        "Balanced instruct model",
        "Qwen 2.5 3B"
    };
    
    model_registry_["qwen2.5:7b"] = {
        "qwen2.5:7b",
        "qwen2.5-7b",
        "Qwen/Qwen2.5-7B-Instruct-GGUF",
        "qwen2.5-7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,      // ~4.5 GB
        "Powerful instruct model for complex tasks",
        "Qwen 2.5 7B"
    };
    
    // ===== ORIGINAL QWEN SERIES =====
    model_registry_["qwen:0.5b"] = {
        "qwen:0.5b",
        "qwen-0.5b",
        "Qwen/Qwen2-0.5B-Instruct-GGUF",
        "Qwen2-0.5B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        352LL * 1024 * 1024,      // ~352 MB
        "Original compact Qwen model",
        "Qwen 0.5B"
    };
    
    model_registry_["qwen:1.8b"] = {
        "qwen:1.8b",
        "qwen-1.8b",
        "RichardErkhov/Qwen_-_Qwen-1_8B-gguf",
        "Qwen-1_8B-Q4_K_M.gguf",
        "Q4_K_M",
        1126LL * 1024 * 1024,     // ~1.1 GB
        "Early Qwen series model",
        "Qwen 1.8B"
    };
    
    model_registry_["qwen:4b"] = {
        "qwen:4b",
        "qwen-4b",
        "Qwen/Qwen3-4B-GGUF",
        "Qwen3-4B-Q4_K_M.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Mid-size original Qwen",
        "Qwen 4B"
    };
    
    model_registry_["qwen:7b"] = {
        "qwen:7b",
        "qwen-7b",
        "Qwen/Qwen2-7B-Instruct-GGUF",
        "Qwen2-7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4368LL * 1024 * 1024,     // ~4.3 GB
        "Full-size original Qwen model",
        "Qwen 7B"
    };
    
    // ===== QWEN 2 SERIES =====
    model_registry_["qwen2:0.5b"] = {
        "qwen2:0.5b",
        "qwen2-0.5b",
        "Qwen/Qwen2-0.5B-Instruct-GGUF",
        "Qwen2-0.5B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        352LL * 1024 * 1024,      // ~352 MB
        "Improved compact model",
        "Qwen 2 0.5B"
    };
    
    model_registry_["qwen2:1.5b"] = {
        "qwen2:1.5b",
        "qwen2-1.5b",
        "Qwen/Qwen2-1.5B-Instruct-GGUF",
        "Qwen2-1.5B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Enhanced small model",
        "Qwen 2 1.5B"
    };
    
    model_registry_["qwen2:7b"] = {
        "qwen2:7b",
        "qwen2-7b",
        "Qwen/Qwen2-7B-Instruct-GGUF",
        "Qwen2-7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced Qwen 2 series",
        "Qwen 2 7B"
    };
    
    // ===== QWEN 2.5 VL (Vision-Language) =====
    model_registry_["qwen2.5vl:1.5b"] = {
        "qwen2.5vl:1.5b",
        "qwen2.5vl-1.5b",
        "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
        "Qwen2.5-1.5B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Vision-language model",
        "Qwen 2.5 VL 1.5B"
    };
    
    model_registry_["qwen2.5vl:7b"] = {
        "qwen2.5vl:7b",
        "qwen2.5vl-7b",
        "unsloth/Qwen2.5-VL-7B-Instruct-GGUF",
        "Qwen2.5-VL-7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced vision-language model",
        "Qwen 2.5 VL 7B"
    };
    
    // ===== QWEN 2 MATH (Math-specialized) =====
    model_registry_["qwen2-math:1.5b"] = {
        "qwen2-math:1.5b",
        "qwen2-math-1.5b",
        "QuantFactory/Qwen2-Math-1.5B-GGUF",
        "Qwen2-Math-1.5B-Q4_K_M.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Math-specialized model",
        "Qwen 2 Math 1.5B"
    };
    
    model_registry_["qwen2-math:7b"] = {
        "qwen2-math:7b",
        "qwen2-math-7b",
        "QuantFactory/Qwen2-Math-7B-GGUF",
        "Qwen2-Math-7B-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced math reasoning model",
        "Qwen 2 Math 7B"
    };
    
    // ===== QWEN 3 EMBEDDING MODELS =====
    model_registry_["qwen3-embedding:0.6b"] = {
        "qwen3-embedding:0.6b",
        "qwen3-embedding-0.6b",
        "Qwen/Qwen3-Embedding-0.6B-GGUF",
        "Qwen3-Embedding-0.6B-Q4_K_M.gguf",
        "Q4_K_M",
        400LL * 1024 * 1024,      // ~400 MB
        "Compact embedding model",
        "Qwen 3 Embedding 0.6B"
    };
    
    model_registry_["qwen3-embedding:4b"] = {
        "qwen3-embedding:4b",
        "qwen3-embedding-4b",
        "Qwen/Qwen3-Embedding-4B-GGUF",
        "Qwen3-Embedding-4B-Q4_K_M.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Balanced embedding model",
        "Qwen 3 Embedding 4B"
    };
    
    model_registry_["qwen3-embedding:8b"] = {
        "qwen3-embedding:8b",
        "qwen3-embedding-8b",
        "Mungert/Qwen3-Embedding-8B-GGUF",
        "Qwen3-Embedding-8B-Q4_K_M.gguf",
        "Q4_K_M",
        4915LL * 1024 * 1024,     // ~4.8 GB
        "Powerful embedding model",
        "Qwen 3 Embedding 8B"
    };
    
    // ===== GEMMA SERIES =====
    model_registry_["gemma:2b"] = {
        "gemma:2b",
        "gemma-2b",
        "google/gemma-2b-GGUF",
        "gemma-2b-Q4_K_M.gguf",
        "Q4_K_M",
        1536LL * 1024 * 1024,     // ~1.5 GB
        "Google's lightweight model",
        "Gemma 2B"
    };
    
    model_registry_["gemma:7b"] = {
        "gemma:7b",
        "gemma-7b",
        "google/gemma-7b-GGUF",
        "gemma-7b-Q4_K_M.gguf",
        "Q4_K_M",
        4368LL * 1024 * 1024,     // ~4.3 GB
        "Google's efficient model",
        "Gemma 7B"
    };
    
    // ===== GEMMA 3 SERIES =====
    model_registry_["gemma3:270m"] = {
        "gemma3:270m",
        "gemma3-270m",
        "unsloth/gemma-3-270m-it-GGUF",
        "gemma-3-270m-it-Q4_K_M.gguf",
        "Q4_K_M",
        164LL * 1024 * 1024,      // ~164 MB
        "Ultra-small Gemma 3",
        "Gemma 3 270M"
    };
    
    model_registry_["gemma3:1b"] = {
        "gemma3:1b",
        "gemma3-1b",
        "unsloth/gemma-3-1b-it-GGUF",
        "gemma-3-1b-it-Q4_K_M.gguf",
        "Q4_K_M",
        729LL * 1024 * 1024,      // ~729 MB
        "Compact Gemma 3",
        "Gemma 3 1B"
    };
    
    model_registry_["gemma3:4b"] = {
        "gemma3:4b",
        "gemma3-4b",
        "google/gemma-3-4b-it-qat-q4_0-gguf",
        "gemma-3-4b-it-qat-q4_0.gguf",
        "Q4_0",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Balanced Gemma 3",
        "Gemma 3 4B"
    };
    
    model_registry_["gemma3:12b"] = {
        "gemma3:12b",
        "gemma3-12b",
        "google/gemma-3-12b-it-qat-q4_0-gguf",
        "gemma-3-12b-it-qat-q4_0.gguf",
        "Q4_0",
        7372LL * 1024 * 1024,     // ~7.2 GB
        "Powerful Gemma 3",
        "Gemma 3 12B"
    };
    
    model_registry_["gemma3n:e2b"] = {
        "gemma3n:e2b",
        "gemma3n-e2b",
        "unsloth/gemma-3n-E2B-it-GGUF",
        "gemma-3n-E2B-it-Q4_K_M.gguf",
        "Q4_K_M",
        1536LL * 1024 * 1024,     // ~1.5 GB
        "Enhanced 2B variant",
        "Gemma 3N E2B"
    };
    
    model_registry_["gemma3n:e4b"] = {
        "gemma3n:e4b",
        "gemma3n-e4b",
        "unsloth/gemma-3n-E4B-it-GGUF",
        "gemma-3n-E4B-it-Q4_K_M.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Enhanced 4B variant",
        "Gemma 3N E4B"
    };
    
    // ===== DEEPSEEK R1 SERIES =====
    model_registry_["deepseek-r1:1.5b"] = {
        "deepseek-r1:1.5b",
        "deepseek-r1-1.5b",
        "unsloth/DeepSeek-R1-Distill-Qwen-1.5B-GGUF",
        "DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Research-focused model",
        "DeepSeek R1 1.5B"
    };
    
    model_registry_["deepseek-r1:7b"] = {
        "deepseek-r1:7b",
        "deepseek-r1-7b",
        "unsloth/DeepSeek-R1-Distill-Qwen-7B-GGUF",
        "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced research model",
        "DeepSeek R1 7B"
    };
    
    model_registry_["deepseek-r1:8b"] = {
        "deepseek-r1:8b",
        "deepseek-r1-8b",
        "unsloth/DeepSeek-R1-Distill-Llama-8B-GGUF",
        "DeepSeek-R1-Distill-Llama-8B-Q4_K_M.gguf",
        "Q4_K_M",
        4915LL * 1024 * 1024,     // ~4.8 GB
        "High-performance research model",
        "DeepSeek R1 8B"
    };
    
    // ===== LLAMA 3 SERIES =====
    model_registry_["llama3:8b"] = {
        "llama3:8b",
        "llama3-8b",
        "QuantFactory/Meta-Llama-3-8B-Instruct-GGUF",
        "Meta-Llama-3-8B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4661LL * 1024 * 1024,     // ~4.7 GB
        "Meta's open-source model",
        "Llama 3 8B"
    };
    
    // ===== LLAMA 3.1 SERIES (Latest Meta models) =====
    model_registry_["llama3.1:8b"] = {
        "llama3.1:8b",
        "llama3.1-8b",
        "bartowski/Meta-Llama-3.1-8B-Instruct-GGUF",
        "Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4700LL * 1024 * 1024,     // ~4.7 GB
        "Meta's versatile multilingual instruct model",
        "Llama 3.1 8B"
    };
    
    // ===== LLAMA 3.2 SERIES (Vision-Language models) =====
    model_registry_["llama3.2:1b"] = {
        "llama3.2:1b",
        "llama3.2-1b",
        "bartowski/Llama-3.2-1B-Instruct-GGUF",
        "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        730LL * 1024 * 1024,      // ~0.73 GB
        "Meta's compact vision-language model",
        "Llama 3.2 1B"
    };
    
    model_registry_["llama3.2:3b"] = {
        "llama3.2:3b",
        "llama3.2-3b",
        "bartowski/Llama-3.2-3B-Instruct-GGUF",
        "Llama-3.2-3B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        2000LL * 1024 * 1024,     // ~2.0 GB
        "Meta's balanced vision-language model for edge devices",
        "Llama 3.2 3B"
    };
    
    // ===== LLAVA (Vision-Language) =====
    model_registry_["llava"] = {
        "llava",
        "llava",
        "second-state/Llava-v1.5-7B-GGUF",
        "Llava-v1.5-7B-Q4_K_M.gguf",
        "Q4_K_M",
        4368LL * 1024 * 1024,     // ~4.3 GB
        "Multimodal vision-language model",
        "LLaVA 1.5 7B"
    };
    
    // ===== LLAMA 2 SERIES =====
    model_registry_["llama2:7b"] = {
        "llama2:7b",
        "llama2-7b",
        "TheBloke/Llama-2-7B-GGUF",
        "Llama-2-7B-Q4_K_M.gguf",
        "Q4_K_M",
        4080LL * 1024 * 1024,     // ~4 GB
        "Original Llama series",
        "Llama 2 7B"
    };
    
    model_registry_["llama2:13b"] = {
        "llama2:13b",
        "llama2-13b",
        "TheBloke/Llama-2-13B-GGUF",
        "Llama-2-13B-Q4_K_M.gguf",
        "Q4_K_M",
        7370LL * 1024 * 1024,     // ~7.2 GB
        "Larger original Llama",
        "Llama 2 13B"
    };
    
    // ===== TINYLLAMA =====
    model_registry_["tinyllama"] = {
        "tinyllama",
        "tinyllama",
        "TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF",
        "TinyLlama-1.1B-Chat-v1.0-Q4_K_M.gguf",
        "Q4_K_M",
        669LL * 1024 * 1024,      // ~669 MB
        "Ultra-small efficient model",
        "TinyLlama 1.1B"
    };
    
    // ===== BGE-M3 (Embedding) =====
    model_registry_["bge-m3"] = {
        "bge-m3",
        "bge-m3",
        "bbvch-ai/bge-m3-GGUF",
        "bge-m3-Q4_K_M.gguf",
        "Q4_K_M",
        512LL * 1024 * 1024,      // ~512 MB
        "Embedding model for retrieval",
        "BGE-M3"
    };
    
    // ===== SMOLLM 2 SERIES =====
    model_registry_["smollm2:135m"] = {
        "smollm2:135m",
        "smollm2-135m",
        "QuantFactory/SmolLM2-135M-GGUF",
        "SmolLM2-135M-Q4_K_M.gguf",
        "Q4_K_M",
        82LL * 1024 * 1024,       // ~82 MB
        "Tiny SmolLM variant",
        "SmolLM 2 135M"
    };
    
    model_registry_["smollm2:360m"] = {
        "smollm2:360m",
        "smollm2-360m",
        "HuggingFaceTB/SmolLM2-360M-Instruct-GGUF",
        "SmolLM2-360M-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        220LL * 1024 * 1024,      // ~220 MB
        "Small SmolLM variant",
        "SmolLM 2 360M"
    };
    
    model_registry_["smollm2:1.7b"] = {
        "smollm2:1.7b",
        "smollm2-1.7b",
        "HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF",
        "SmolLM2-1.7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        1126LL * 1024 * 1024,     // ~1.1 GB
        "Balanced SmolLM",
        "SmolLM 2 1.7B"
    };
    
    // ===== SMOLLM SERIES (Original) =====
    model_registry_["smollm:135m"] = {
        "smollm:135m",
        "smollm-135m",
        "QuantFactory/SmolLM-135M-GGUF",
        "SmolLM-135M-Q4_K_M.gguf",
        "Q4_K_M",
        82LL * 1024 * 1024,       // ~82 MB
        "Original tiny SmolLM",
        "SmolLM 135M"
    };
    
    model_registry_["smollm:360m"] = {
        "smollm:360m",
        "smollm-360m",
        "QuantFactory/SmolLM-360M-GGUF",
        "SmolLM-360M-Q4_K_M.gguf",
        "Q4_K_M",
        220LL * 1024 * 1024,      // ~220 MB
        "Original small SmolLM",
        "SmolLM 360M"
    };
    
    model_registry_["smollm:1.7b"] = {
        "smollm:1.7b",
        "smollm-1.7b",
        "QuantFactory/SmolLM-1.7B-GGUF",
        "SmolLM-1.7B-Q4_K_M.gguf",
        "Q4_K_M",
        1126LL * 1024 * 1024,     // ~1.1 GB
        "Original balanced SmolLM",
        "SmolLM 1.7B"
    };
    
    // ===== FALCON 3 SERIES =====
    model_registry_["falcon3:1b"] = {
        "falcon3:1b",
        "falcon3-1b",
        "tiiuae/Falcon3-1B-Instruct-GGUF",
        "Falcon3-1B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        729LL * 1024 * 1024,      // ~729 MB
        "Efficient small Falcon",
        "Falcon 3 1B"
    };
    
    model_registry_["falcon3:3b"] = {
        "falcon3:3b",
        "falcon3-3b",
        "tiiuae/Falcon3-3B-Instruct-GGUF",
        "Falcon3-3B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        2048LL * 1024 * 1024,     // ~2 GB
        "Balanced Falcon model",
        "Falcon 3 3B"
    };
    
    model_registry_["falcon3:7b"] = {
        "falcon3:7b",
        "falcon3-7b",
        "tiiuae/Falcon3-7B-Instruct-GGUF",
        "Falcon3-7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Powerful Falcon model",
        "Falcon 3 7B"
    };
    
    // ===== PHI SERIES =====
    model_registry_["phi"] = {
        "phi",
        "phi",
        "microsoft/Phi-3-mini-4k-instruct-gguf",
        "Phi-3-mini-4k-instruct-Q4_K_M.gguf",
        "Q4_K_M",
        2355LL * 1024 * 1024,     // ~2.3 GB
        "Microsoft's reasoning model",
        "Phi-3 Mini"
    };
    
    model_registry_["phi2"] = {
        "phi2",
        "phi2",
        "TheBloke/phi-2-GGUF",
        "phi-2-Q4_K_M.gguf",
        "Q4_K_M",
        1638LL * 1024 * 1024,     // ~1.6 GB
        "Improved reasoning model",
        "Phi-2"
    };
    
    model_registry_["phi4-mini"] = {
        "phi4-mini",
        "phi4-mini",
        "tensorblock/Phi-4-mini-instruct-GGUF",
        "Phi-4-mini-instruct-Q4_K_M.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Compact Phi variant",
        "Phi-4 Mini"
    };
    
    // ===== GRANITE4 SERIES (IBM Granite models) =====
    model_registry_["granite4:350m"] = {
        "granite4:350m",
        "granite4-350m",
        "ibm/Granite-4-350M-Instruct-GGUF",
        "granite-4-350m-instruct-Q4_K_M.gguf",
        "Q4_K_M",
        220LL * 1024 * 1024,      // ~220 MB
        "Ultra-compact Granite 4 model",
        "Granite 4 350M"
    };
    
    model_registry_["granite4:350m-h"] = {
        "granite4:350m-h",
        "granite4-350m-h",
        "ibm/Granite-4-350M-Instruct-GGUF",
        "granite-4-350m-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        220LL * 1024 * 1024,      // ~220 MB
        "Ultra-compact Granite 4 model (HF format)",
        "Granite 4 350M-H"
    };
    
    model_registry_["granite4:1b"] = {
        "granite4:1b",
        "granite4-1b",
        "ibm/Granite-4-1B-Instruct-GGUF",
        "granite-4-1b-instruct-Q4_K_M.gguf",
        "Q4_K_M",
        729LL * 1024 * 1024,      // ~729 MB
        "Compact Granite 4 model",
        "Granite 4 1B"
    };
    
    model_registry_["granite4:1b-h"] = {
        "granite4:1b-h",
        "granite4-1b-h",
        "ibm/Granite-4-1B-Instruct-GGUF",
        "granite-4-1b-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        729LL * 1024 * 1024,      // ~729 MB
        "Compact Granite 4 model (HF format)",
        "Granite 4 1B-H"
    };
    
    model_registry_["granite4:3b"] = {
        "granite4:3b",
        "granite4-3b",
        "ibm/Granite-4-3B-Instruct-GGUF",
        "granite-4-3b-instruct-Q4_K_M.gguf",
        "Q4_K_M",
        1946LL * 1024 * 1024,     // ~1.9 GB
        "Balanced Granite 4 model",
        "Granite 4 3B"
    };
    
    model_registry_["granite4:micro"] = {
        "granite4:micro",
        "granite4-micro",
        "ibm/Granite-4-Micro-Instruct-GGUF",
        "granite-4-micro-instruct-Q4_K_M.gguf",
        "Q4_K_M",
        100LL * 1024 * 1024,      // ~100 MB (estimated)
        "Tiny Granite 4 model",
        "Granite 4 Micro"
    };
    
    model_registry_["granite4:3b-h"] = {
        "granite4:3b-h",
        "granite4-3b-h",
        "ibm/Granite-4-3B-Instruct-GGUF",
        "granite-4-3b-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        1946LL * 1024 * 1024,     // ~1.9 GB
        "Balanced Granite 4 model (HF format)",
        "Granite 4 3B-H"
    };
    
    model_registry_["granite4:micro-h"] = {
        "granite4:micro-h",
        "granite4-micro-h",
        "ibm/Granite-4-Micro-Instruct-GGUF",
        "granite-4-micro-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        100LL * 1024 * 1024,      // ~100 MB (estimated)
        "Tiny Granite 4 model (HF format)",
        "Granite 4 Micro-H"
    };
    
    model_registry_["granite4:7b-a1b-h"] = {
        "granite4:7b-a1b-h",
        "granite4-7b-a1b-h",
        "ibm/Granite-4-7B-A1B-Instruct-GGUF",
        "granite-4-7b-a1b-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Powerful Granite 4 7B A1B model (HF format)",
        "Granite 4 7B-A1B-H"
    };
    
    model_registry_["granite4:tiny-h"] = {
        "granite4:tiny-h",
        "granite4-tiny-h",
        "ibm/Granite-4-Tiny-Instruct-GGUF",
        "granite-4-tiny-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        50LL * 1024 * 1024,       // ~50 MB (estimated)
        "Ultra-tiny Granite 4 model (HF format)",
        "Granite 4 Tiny-H"
    };
    
    model_registry_["granite4:32b-a9b-h"] = {
        "granite4:32b-a9b-h",
        "granite4-32b-a9b-h",
        "ibm/Granite-4-32B-A9B-Instruct-GGUF",
        "granite-4-32b-a9b-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        18432LL * 1024 * 1024,    // ~18 GB
        "Large Granite 4 32B A9B model (HF format)",
        "Granite 4 32B-A9B-H"
    };
    
    model_registry_["granite4:small-h"] = {
        "granite4:small-h",
        "granite4-small-h",
        "ibm/Granite-4-Small-Instruct-GGUF",
        "granite-4-small-instruct-hf-Q4_K_M.gguf",
        "Q4_K_M",
        512LL * 1024 * 1024,      // ~512 MB (estimated)
        "Small Granite 4 model (HF format)",
        "Granite 4 Small-H"
    };
}

std::vector<ModelRegistry> ModelManager::get_registry_models() {
    std::vector<ModelRegistry> models;
    for (const auto& pair : model_registry_) {
        models.push_back(pair.second);
    }
    return models;
}

ModelRegistry ModelManager::get_registry_entry(const std::string& model_name) {
    auto it = model_registry_.find(model_name);
    if (it != model_registry_.end()) {
        return it->second;
    }
    return ModelRegistry{}; // Return empty registry entry
}

bool ModelManager::is_in_registry(const std::string& model_name) {
    return model_registry_.find(model_name) != model_registry_.end();
}

void ModelManager::set_progress_callback(ProgressCallback callback) {
    progress_callback_ = callback;
}

// ============================================================================
// NEW: Short name resolution and friendly listing
// ============================================================================

std::string ModelManager::resolve_model_name(const std::string& input_name) {
    // Resolve model name to GGUF filename
    // Accepts: "qwen3:0.6b" (registry .name), "qwen3-0.6b" (short_name), or "Model.gguf" (filename)
    // Returns: Full GGUF filename (e.g., "Qwen3-0.6B-Q4_K_M.gguf")
    
    // If input already ends with .gguf, return as-is
    if (input_name.length() >= 5 && 
        input_name.substr(input_name.length() - 5) == ".gguf") {
        return input_name;
    }
    
    // First, check if it matches a registry name (with colon notation: "qwen3:0.6b")
    auto it = model_registry_.find(input_name);
    if (it != model_registry_.end()) {
        return it->second.filename;
    }
    
    // Check if it matches a short_name in registry ("qwen3-0.6b")
    for (const auto& pair : model_registry_) {
        if (pair.second.short_name == input_name) {
            return pair.second.filename;
        }
    }
    
    // Try converting dash notation to colon notation
    // "qwen2.5-0.5b" -> "qwen2.5:0.5b"
    size_t last_dash = input_name.find_last_of('-');
    if (last_dash != std::string::npos) {
        std::string colon_name = input_name.substr(0, last_dash) + ":" + 
                                 input_name.substr(last_dash + 1);
        auto it2 = model_registry_.find(colon_name);
        if (it2 != model_registry_.end()) {
            return it2->second.filename;
        }
    }
    
    // If not found, assume it's already a filename (with or without .gguf)
    if (input_name.length() < 5 || input_name.substr(input_name.length() - 5) != ".gguf") {
        return input_name + ".gguf";
    }
    
    return input_name;
}

bool ModelManager::is_model_installed(const std::string& model_name) {
    // Check if a model (by short name or filename) is installed locally
    std::string filename = resolve_model_name(model_name);
    std::string full_path = tools::FileOps::join_path(models_dir_, filename);
    return tools::FileOps::file_exists(full_path);
}

std::vector<ModelManager::ModelInfo> ModelManager::get_friendly_model_list(bool include_available) {
    std::vector<ModelInfo> result;
    
    // Use locale-aware size formatting
    auto format_size = [](long long bytes) -> std::string {
        return UI::format_size(bytes);
    };
    
    if (include_available) {
        // Show all models from registry using .name (e.g., "qwen3:0.6b")
        for (const auto& pair : model_registry_) {
            const auto& reg = pair.second;
            ModelInfo info;
            info.name = reg.name;  // Use registry .name (with colon)
            info.display_name = reg.display_name;
            info.description = reg.description;
            info.size_str = format_size(reg.size_bytes);
            info.quantization = reg.quantization;
            info.size_bytes = reg.size_bytes;
            info.installed = is_model_installed(reg.name);  // Check by .name
            result.push_back(info);
        }
    } else {
        // Only show installed models using .name (e.g., "qwen3:0.6b")
        for (const auto& pair : model_registry_) {
            const auto& reg = pair.second;
            if (is_model_installed(reg.name)) {
                ModelInfo info;
                info.name = reg.name;  // Use registry .name (with colon)
                info.display_name = reg.display_name;
                info.description = reg.description;
                info.size_str = format_size(reg.size_bytes);
                info.quantization = reg.quantization;
                info.size_bytes = reg.size_bytes;
                info.installed = true;
                result.push_back(info);
            }
        }
        
        // Also check for any locally installed models not in registry
        auto local_files = list_models();
        for (const auto& filename : local_files) {
            // Check if this filename is already in our result
            bool found = false;
            for (const auto& reg_pair : model_registry_) {
                std::string reg_filename = reg_pair.second.filename;
                if (reg_filename.length() >= 5) {
                    reg_filename = reg_filename.substr(0, reg_filename.length() - 5);
                }
                if (filename == reg_filename) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // Unknown model - get actual size from disk
                std::string full_path = tools::FileOps::join_path(models_dir_, filename + ".gguf");
                struct stat st;
                long long size_bytes = 0;
                if (stat(full_path.c_str(), &st) == 0) {
                    size_bytes = st.st_size;
                }
                
                ModelInfo info;
                info.name = filename;
                info.display_name = filename;
                info.description = "Custom model (not in registry)";
                info.size_str = format_size(size_bytes);
                info.quantization = "Unknown";
                info.size_bytes = size_bytes;
                info.installed = true;
                result.push_back(info);
            }
        }
    }
    
    // Sort by size (smallest first)
    std::sort(result.begin(), result.end(), 
              [](const ModelInfo& a, const ModelInfo& b) {
                  return a.size_bytes < b.size_bytes;
              });
    
    return result;
}

std::string ModelManager::get_hf_url(const std::string& repo_id, const std::string& filename) {
    return "https://huggingface.co/" + repo_id + "/resolve/main/" + filename;
}

// libcurl write callback
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::ofstream* file = static_cast<std::ofstream*>(userp);
    file->write(static_cast<const char*>(contents), total_size);
    return total_size;
}

// libcurl progress callback
static int progress_callback_wrapper(void* clientp, 
                                     curl_off_t dltotal, curl_off_t dlnow,
                                     curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;
    
    if (dltotal > 0) {
        ModelManager::ProgressCallback* callback = 
            static_cast<ModelManager::ProgressCallback*>(clientp);
        if (*callback) {
            double progress = (double)dlnow / (double)dltotal * 100.0;
            (*callback)(progress, dlnow, dltotal);
        }
    }
    return 0; // Return 0 to continue download
}

bool ModelManager::download_file(const std::string& url, 
                                 const std::string& dest_path,
                                 ProgressCallback progress) {
    CURL* curl;
    CURLcode res;
    bool success = false;
    
    // Create temporary file path
    std::string temp_path = dest_path + ".tmp";
    
    // Open file for writing
    std::ofstream file(temp_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // Follow redirects
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        
        // Set write callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        
        // Set user agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Delta-CLI/1.0");
        
        // Enable progress meter
        if (progress) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback_wrapper);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
        }
        
        // Set timeout (30 seconds connect, 0 = infinite transfer)
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
        
        // Perform download
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 200) {
                success = true;
            } else {
                // Handle HTTP error codes
                if (response_code == 404) {
                    UI::print_error("Model file not found (404) - repository may have changed");
                } else if (response_code >= 500) {
                    UI::print_error("Server error (" + std::to_string(response_code) + ") - try again later");
                } else {
                    UI::print_error("HTTP error " + std::to_string(response_code));
                }
            }
        } else {
            // Handle curl errors
            if (res == CURLE_COULDNT_CONNECT || res == CURLE_COULDNT_RESOLVE_HOST) {
                UI::print_error("Network error - check your internet connection");
            } else if (res == CURLE_OPERATION_TIMEDOUT) {
                UI::print_error("Download timeout - try again with better connection");
            } else {
                UI::print_error("Download failed: " + std::string(curl_easy_strerror(res)));
            }
        }
        
        // Cleanup
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    file.close();
    
    // Move temp file to destination if successful
    if (success) {
        // Remove any existing file at destination
        std::remove(dest_path.c_str());
        
        // Attempt to rename temp file to final destination
        if (std::rename(temp_path.c_str(), dest_path.c_str()) != 0) {
            // Rename failed (possibly across filesystems) - fall back to copy + delete
            std::ifstream src(temp_path, std::ios::binary);
            std::ofstream dst(dest_path, std::ios::binary);
            
            if (src && dst) {
                dst << src.rdbuf();
                src.close();
                dst.close();
                std::remove(temp_path.c_str());
            } else {
                // Copy failed too - cleanup and report failure
                src.close();
                dst.close();
                std::remove(temp_path.c_str());
                success = false;
            }
        }
    } else {
        // Remove temp file on failure
        std::remove(temp_path.c_str());
    }
    
    return success;
}

bool ModelManager::pull_model(const std::string& model_name, const std::string& quantization) {
    // Note: quantization parameter reserved for future use (multiple quantization support)
    // Currently using default Q4_K_M/Q4_0 from registry
    if (!quantization.empty()) {
        // Future: allow override of quantization format
    }
    
    // Check if model exists in registry
    if (!is_in_registry(model_name)) {
        UI::print_error("Model '" + model_name + "' not found in registry");
        UI::print_info("Use 'delta list-models --available' to see available models");
        return false;
    }
    
    // Get registry entry
    ModelRegistry entry = get_registry_entry(model_name);
    
    // Check if already downloaded
    if (has_model(model_name)) {
        UI::print_info("Model '" + model_name + "' already exists locally");
        std::string path = get_model_path(model_name);
        UI::print_info("Path: " + path);
        return true;
    }
    
    // Construct download URL
    std::string url = get_hf_url(entry.repo_id, entry.filename);
    
    // Determine destination filename
    std::string dest_filename = entry.filename;
    std::string dest_path = tools::FileOps::join_path(models_dir_, dest_filename);
    
    // Print download info
    UI::print_border("DOWNLOADING MODEL");
    UI::print_info("Model: " + entry.name);
    UI::print_info("Description: " + entry.description);
    UI::print_info("Quantization: " + entry.quantization);
    
    // Format size
    double size_gb = entry.size_bytes / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream size_str;
    size_str << std::fixed << std::setprecision(2) << size_gb << " GB";
    UI::print_info("Approximate size: " + size_str.str());
    
    UI::print_info("Source: " + entry.repo_id);
    UI::print_info("Destination: " + dest_path);
    std::cout << std::endl;
    
    // Download with progress
    UI::print_info("Downloading... (this may take a while)");
    
    bool success = download_file(url, dest_path, progress_callback_);
    
    if (success) {
        std::cout << std::endl;
        UI::print_info("✓ Download complete!");
        UI::print_info("Model saved to: " + dest_path);
        UI::print_info("You can now use: delta --model " + model_name);
        return true;
    } else {
        std::cout << std::endl;
        UI::print_error("✗ Download failed");
        UI::print_error("Please check your internet connection and try again");
        UI::print_info("Or manually download from: " + url);
        return false;
    }
}

// ============================================================================
// DEFAULT MODEL SUPPORT
// ============================================================================

std::string ModelManager::get_default_model() {
    return DEFAULT_MODEL_NAME;  // "qwen3:0.6b"
}

std::string ModelManager::get_default_model_short_name() const {
    // Convert "qwen3:0.6b" to "qwen3-0.6b" for CLI usage
    auto it = model_registry_.find(DEFAULT_MODEL_NAME);
    if (it != model_registry_.end()) {
        return it->second.short_name;
    }
    return "qwen3-0.6b";  // Fallback
}

bool ModelManager::ensure_default_model_installed(ProgressCallback progress) {
    // Check if default model is already installed
    std::string short_name = get_default_model_short_name();
    if (is_model_installed(short_name)) {
        return true;  // Already installed
    }
    
    // Get registry entry
    auto it = model_registry_.find(DEFAULT_MODEL_NAME);
    if (it == model_registry_.end()) {
        UI::print_error("Default model not found in registry: " + DEFAULT_MODEL_NAME);
        return false;
    }
    
    // Show friendly message with better formatting
    UI::print_border("SETTING UP DEFAULT MODEL");
    UI::print_info("Model: " + it->second.display_name);
    UI::print_info("Description: " + it->second.description);
    
    // Format size nicely
    double size_mb = it->second.size_bytes / (1024.0 * 1024.0);
    double size_gb = size_mb / 1024.0;
    std::ostringstream size_str;
    if (size_gb >= 1.0) {
        size_str << std::fixed << std::setprecision(2) << size_gb << " GB";
    } else {
        size_str << std::fixed << std::setprecision(0) << size_mb << " MB";
    }
    UI::print_info("Size: " + size_str.str());
    UI::print_info("Quantization: " + it->second.quantization);
    UI::print_info("This is a one-time download (internet required)");
    std::cout << std::endl;
    
    // Download the model
    set_progress_callback(progress);
    bool success = pull_model(DEFAULT_MODEL_NAME);
    set_progress_callback(nullptr);
    
    if (success) {
        UI::print_info("✓ Default model installed successfully!");
        UI::print_info("You can now start chatting with your AI assistant!");
        std::cout << std::endl;
    } else {
        UI::print_error("Failed to download default model");
        UI::print_info("Possible reasons:");
        UI::print_info("  • No internet connection");
        UI::print_info("  • Insufficient disk space");
        UI::print_info("  • Network timeout");
        UI::print_info("You can manually download it later with: delta pull " + DEFAULT_MODEL_NAME);
        UI::print_info("Or try a different model: delta --list-models --available");
    }
    
    return success;
}

std::string ModelManager::get_auto_selected_model() {
    // First, check if default model is installed
    std::string default_short = get_default_model_short_name();
    if (is_model_installed(default_short)) {
        return default_short;
    }
    
    // Fall back to any installed model (smallest first)
    auto models = list_models();
    if (!models.empty()) {
        return models[0];  // Return first installed model
    }
    
    // No models installed - return default for download
    return default_short;
}

} // namespace delta

