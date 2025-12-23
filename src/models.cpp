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
    // Format: {name, short_name, repo_id, filename, quant, size, description, display_name, max_context}
    // Updated with verified HuggingFace repositories as of v1.0.0
    // max_context: Maximum usable context size for llama-server (-c parameter)
    
    // ===== QWEN 3 SERIES (Latest generation) =====
    model_registry_["qwen3:0.6b"] = {
        "tinygemma3",
        "qwen3-0.6b",
        "ggml-org/tinygemma3-GGUF",
        "tinygemma3-Q8_0.gguf",
        "Q8_0",
        4720LL * 1024 * 1024,      // ~47.2 MB
        "Ultra-compact multilingual model",
        "Qwen 3 0.6B",
        131072                     // 128K native context
    };

    model_registry_["qwen3:0.6b"] = {
        "qwen3:0.6b",
        "qwen3-0.6b",
        "ggml-org/Qwen3-0.6B-GGUF",
        "Qwen3-0.6B-f16.gguf",
        "F16",
        1546LL * 1024 * 1024,      // ~1.51 GB
        "Ultra-compact multilingual model",
        "Qwen 3 0.6B",
        40960                     // 40K native context
    };
    
    model_registry_["qwen3:1.7b"] = {
        "qwen3:1.7b",
        "qwen3-1.7b",
        "ggml-org/Qwen3-1.7B-GGUF",
        "Qwen3-1.7B-f16.gguf",
        "F16",
        1126LL * 1024 * 1024,     // ~1.28 GB
        "Efficient small multilingual model",
        "Qwen 3 1.7B",
        40960                     // 40K native context
    };
    
    model_registry_["qwen3:4b"] = {
        "qwen3:4b",
        "qwen3-4b",
        "ggml-org/Qwen3-4B-GGUF",
        "Qwen3-4B-Q4_K_M.gguf",
        "Q4_K_M",
        2560LL * 1024 * 1024,     // ~2.5 GB
        "Balanced multilingual reasoning model",
        "Qwen 3 4B",
        40960                     // 40K native context
    };
    
    model_registry_["qwen3:8b"] = {
        "qwen3:8b",
        "qwen3-8b",
        "ggml-org/Qwen3-8B-GGUF",
        "Qwen3-8B-Q4_K_M.gguf",
        "Q4_K_M",
        5150LL * 1024 * 1024,     // ~5.03 GB
        "Powerful multilingual instruct model",
        "Qwen 3 8B",
        40960                     // 40K native context
    };

    model_registry_["qwen3:14b"] = {
        "qwen3:14b",
        "qwen3-14b",
        "ggml-org/Qwen3-14B-GGUF",
        "Qwen3-14B-Q4_K_M.gguf",
        "Q4_K_M",
        9216LL * 1024 * 1024,     // ~9 GB
        "Powerful multilingual instruct model",
        "Qwen 3 14B",
        40960                     // 40K native context
    };

    model_registry_["qwen3think:4b"] = {
        "qwen3think:4b",
        "qwen3-think-4b",
        "ggml-org/Qwen3-4B-Thinking-2507-Q8_0-GGUF",
        "qwen3-4b-thinking-2507-q8_0.gguf",
        "Q8_0",
        4288LL * 1024 * 1024,     // ~4.28 GB
        "Powerful reasoning model",
        "Qwen 3 4B Thinking",
        262144                     // 256K native context
    };

    model_registry_["qwen3it:4b"] = {
        "qwen3it:4b",
        "qwen3-it-4b",
        "ggml-org/Qwen3-4B-Instruct-2507-Q8_0-GGUF",
        "qwen3-4b-instruct-2507-q8_0.gguf",
        "Q8_0",
        4288LL * 1024 * 1024,     // ~4.28 GB
        "Powerful reasoning model",
        "Qwen 3 4B Instruct",
        262144                     // 256K native context
    };
    
    // ===== QWEN 3 VL (Vision-Language) INSTRUCT from NexaAI =====
    model_registry_["qwen3-vl:4b-instruct"] = {
        "qwen3-vl:4b",
        "qwen3-vl-4b-instruct",
        "KathAhegao/Qwen3-VL-4B-Instruct-Q4_K_M-GGUF",
        "qwen3-vl-4b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4000LL * 1024 * 1024,     // ~4.0 GB (approx)
        "Qwen3-VL 4B Instruct vision-language model",
        "Qwen3-VL 4B Instruct",
        32768                     // 32K native context
    };
    
    model_registry_["qwen3-vl:8b-instruct"] = {
        "qwen3-vl:8b",
        "qwen3-vl-8b-instruct",
        "mazrba/Huihui-Qwen3-VL-8B-Instruct-abliterated-Q4_K_M-GGUF",
        "huihui-qwen3-vl-8b-instruct-abliterated-q4_k_m-imat.gguf",
        "Q4_K_M",
        8000LL * 1024 * 1024,     // ~8.0 GB (approx)
        "Qwen3-VL 8B Instruct vision-language model",
        "Qwen3-VL 8B Instruct",
        32768                     // 32K native context
    };
    
    // ===== QWEN 2.5 CODER SERIES (Code-specialized)(128K native) =====
    model_registry_["qwen2.5-coder:0.5b"] = {
        "qwen2.5-coder:0.5b",
        "qwen2.5-coder-0.5b",
        "ggml-org/Qwen2.5-Coder-0.5B-Q8_0-GGUF",
        "qwen2.5-coder-0.5b-q8_0.gguf",
        "Q8_0",
        352LL * 1024 * 1024,      // ~0.53 GB
        "Tiny code generation model",
        "Qwen 2.5 Coder 0.5B",
        32768                    // 32K with RoPE scaling (explicit in filename)
    };
    
    model_registry_["qwen2.5-coder:1.5b"] = {
        "qwen2.5-coder:1.5b",
        "qwen2.5-coder-1.5b",
        "ggml-org/Qwen2.5-Coder-1.5B-Q8_0-GGUF",
        "qwen2.5-coder-1.5b-q8_0.gguf",
        "Q8_0",
        1689LL * 1024 * 1024,     // ~1.65 GB
        "Small code-focused model",
        "Qwen 2.5 Coder 1.5B",
        32768                    // 32K with RoPE scaling
    };
    
    model_registry_["qwen2.5-coder:3b"] = {
        "qwen2.5-coder:3b",
        "qwen2.5-coder-3b",
        "ggml-org/Qwen2.5-Coder-3B-Q8_0-GGUF",
        "qwen2.5-coder-3b-q8_0.gguf",
        "Q8_0",
        3296LL * 1024 * 1024,     // ~3.29 GB
        "Balanced coding assistant",
        "Qwen 2.5 Coder 3B",
        32768                    // 32K with RoPE scaling
    };
    
    model_registry_["qwen2.5-coder:7b"] = {
        "qwen2.5-coder:7b",
        "qwen2.5-coder-7b",
        "Qwen/Qwen2.5-Coder-7B-Instruct-GGUF",
        "qwen2.5-coder-7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced code generation model",
        "Qwen 2.5 Coder 7B",
        131072                    // 128K with RoPE scaling
    };
    
    // ===== QWEN 2.5 SERIES (Latest instruct models)(128K native) =====
    model_registry_["qwen2.5:0.5b"] = {
        "qwen2.5:0.5b",
        "qwen2.5-0.5b",
        "Qwen/Qwen2.5-0.5B-Instruct-GGUF",
        "qwen2.5-0.5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        350LL * 1024 * 1024,       // ~0.35 GB
        "Ultra-compact instruct model",
        "Qwen 2.5 0.5B",
        131072                      // 128K native context
    };
    
    model_registry_["qwen2.5:1.5b"] = {
        "qwen2.5:1.5b",
        "qwen2.5-1.5b",
        "Qwen/Qwen2.5-1.5B-Instruct-GGUF",
        "qwen2.5-1.5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,      // ~1 GB
        "Small instruct model for edge devices",
        "Qwen 2.5 1.5B",
        131072                      // 128K native context
    };
    
    model_registry_["qwen2.5:3b"] = {
        "qwen2.5:3b",
        "qwen2.5-3b",
        "Qwen/Qwen2.5-3B-Instruct-GGUF",
        "qwen2.5-3b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        2048LL * 1024 * 1024,      // ~2 GB
        "Balanced instruct model",
        "Qwen 2.5 3B",
        131072                      // 128K native context
    };
    
    model_registry_["qwen2.5:7b"] = {
        "qwen2.5:7b",
        "qwen2.5-7b",
        "paultimothymooney/Qwen2.5-7B-Instruct-Q4_K_M-GGUF",
        "qwen2.5-7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,      // ~4.5 GB
        "Powerful instruct model for complex tasks",
        "Qwen 2.5 7B",
        131072                      // 128K native context
    };
    
    // ===== ORIGINAL QWEN SERIES (32K) =====
    model_registry_["qwen2:0.5b"] = {
        "qwen2:0.5b",
        "qwen-0.5b",
        "Qwen/Qwen2-0.5B-Instruct-GGUF",
        "qwen2-0_5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        352LL * 1024 * 1024,      // ~352 MB
        "Original compact Qwen model",
        "Qwen 2 0.5B",
        32768                      // 32K native context
    };
    
    model_registry_["qwen:1.8b"] = {
        "qwen:1.8b",
        "qwen-1.8b",
        "mradermacher/Qwen-1_8B-GGUF",
        "Qwen-1_8B.Q4_K_M.gguf",
        "Q4_K_M",
        1126LL * 1024 * 1024,     // ~1.1 GB
        "Early Qwen series model",
        "Qwen 1.8B",
        32768                      // 32K native context
    };
    
    model_registry_["qwen3:4b"] = {
        "qwen3:4b",
        "qwen3-4b",
        "Qwen/Qwen3-4B-GGUF",
        "Qwen3-4B-Q4_K_M.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Mid-size original Qwen",
        "Qwen 3 4B",
        32768                      // 32K native context
    };
    
    model_registry_["qwen2:7b"] = {
        "qwen2:7b",
        "qwen2-7b",
        "NikolayKozloff/Qwen2-7B-Instruct-Q4_K_M-GGUF",
        "qwen2-7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4368LL * 1024 * 1024,     // ~4.3 GB
        "Full-size original Qwen model",
        "Qwen 2 7B",
        32768                      // 32K native context
    };
    
    // ===== QWEN 2 SERIES (32K) =====
    model_registry_["qwen2:0.5b"] = {
        "qwen2:0.5b",
        "qwen2-0.5b",
        "Qwen/Qwen2-0.5B-Instruct-GGUF",
        "qwen2-0_5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        352LL * 1024 * 1024,      // ~352 MB
        "Improved compact model",
        "Qwen 2 0.5B",
        32768                      // 32K native context
    };
    
    model_registry_["qwen2:1.5b"] = {
        "qwen2:1.5b",
        "qwen2-1.5b",
        "Qwen/Qwen2-1.5B-Instruct-GGUF",
        "qwen2-1_5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Enhanced small model",
        "Qwen 2 1.5B",
        32768                      // 32K native context
    };
    
    model_registry_["qwen2:7b"] = {
        "qwen2:7b",
        "qwen2-7b",
        "Qwen/Qwen2-7B-Instruct-GGUF",
        "qwen2-7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced Qwen 2 series",
        "Qwen 2 7B",
        32768                      // 32K native context
    };
    
    // ===== QWEN 2.5 VL (Vision-Language) (128K) =====
    model_registry_["qwen2.5vl:1.5b"] = {
        "qwen2.5vl:1.5b",
        "qwen2.5vl-1.5b",
        "Triangle104/Qwen2.5-1.5B-Instruct-Q4_K_M-GGUF",
        "qwen2.5-1.5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Vision-language model",
        "Qwen 2.5 VL 1.5B",
        131072                      // 128K native context
    };

    model_registry_["qwen2.5vl:3b"] = {
        "qwen2.5vl:3b",
        "qwen2.5vl-3b",
        "ggml-org/Qwen2.5-VL-3B-Instruct-GGUF",
        "Qwen2.5-VL-3B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        1976LL * 1024 * 1024,     // ~1.93 GB
        "Vision-language model",
        "Qwen 2.5 VL 3B",
        131072                      // 128K native context
    };

    model_registry_["qwen2.5vl:7b"] = {
        "qwen2.5vl:7b",
        "qwen2.5vl-7b",
        "ggml-org/Qwen2.5-VL-7B-Instruct-GGUF",
        "Qwen2.5-VL-7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4792LL * 1024 * 1024,     // ~4.68 GB
        "Vision-language model",
        "Qwen 2.5 VL 7B",
        131072                      // 128K native context
    };

    model_registry_["qwen2vl:2b"] = {
        "qwen2vl:2b",
        "qwen2vl-2b",
        "ggml-org/Qwen2-VL-2B-Instruct-GGUF",
        "Qwen2-VL-2B-Instruct-Q8_0.gguf",
        "Q8_0",
        1656LL * 1024 * 1024,     // ~1.65 GB
        "Vision-language model",
        "Qwen 2 VL 2B",
        32768                      // 32K native context
    };
    
    model_registry_["qwen2.5vl:7b"] = {
        "qwen2.5vl:7b",
        "qwen2.5vl-7b",
        "rexionmars/Qwen2.5-VL-7B-Instruct-Q4_K_M-GGUF",
        "qwen2.5-vl-7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced vision-language model",
        "Qwen 2.5 VL 7B",
        131072                      // 128K native context
    };
    
    // ===== QWEN 2 MATH (Math-specialized) (32K) =====
    model_registry_["qwen2-math:1.5b"] = {
        "qwen2-math:1.5b",
        "qwen2-math-1.5b",
        "itlwas/Qwen2-Math-1.5B-Instruct-Q4_K_M-GGUF",
        "qwen2-math-1.5b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Math-specialized model",
        "Qwen 2 Math 1.5B",
        32768                      // 32K native context
    };
    
    model_registry_["qwen2-math:7b"] = {
        "qwen2-math:7b",
        "qwen2-math-7b",
        "gdhnes/Qwen2-Math-7B-Instruct-Q4_K_M-GGUF",
        "qwen2-math-7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced math reasoning model",
        "Qwen 2 Math 7B",
        32768                      // 32K native context
    };
    
    // ===== QWEN 3 EMBEDDING MODELS  (32K)=====
    model_registry_["qwen3-embedding:0.6b"] = {
        "qwen3-embedding:0.6b",
        "qwen3-embedding-0.6b",
        "WariHima/Qwen3-Embedding-0.6B-Q4_K_M-GGUF",
        "qwen3-embedding-0.6b-q4_k_m.gguf",
        "Q4_K_M",
        400LL * 1024 * 1024,      // ~400 MB
        "Compact embedding model",
        "Qwen 3 Embedding 0.6B",
        32768                       // Embedding models typically 32K
    };
    
    model_registry_["qwen3-embedding:4b"] = {
        "qwen3-embedding:4b",
        "qwen3-embedding-4b",
        "enacimie/Qwen3-Embedding-4B-Q4_K_M-GGUF",
        "qwen3-embedding-4b-q4_k_m.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Balanced embedding model",
        "Qwen 3 Embedding 4B",
        32768                       // Embedding models typically 32K
    };
    
    model_registry_["qwen3-embedding:8b"] = {
        "qwen3-embedding:8b",
        "qwen3-embedding-8b",
        "endyjasmi/Qwen3-Embedding-8B-Q4_K_M-GGUF",
        "qwen3-embedding-8b-q4_k_m.gguf",
        "Q4_K_M",
        4915LL * 1024 * 1024,     // ~4.8 GB
        "Powerful embedding model",
        "Qwen 3 Embedding 8B",
        32768                       // Embedding models typically 32K
    };
    
    // ===== GEMMA SERIES (8K) =====
    model_registry_["gemma1.1:2b"] = {
        "gemma1.1:2b",
        "gemma-1.1-2b",
        "ggml-org/gemma-1.1-2b-it-Q8_0-GGUF",
        "gemma-1.1-2b-it.Q8_0.gguf",
        "Q8_0",
        2592LL * 1024 * 1024,     // ~2.6 GB
        "Google's lightweight model",
        "Gemma 1.1 2B",
        8192                       // 8K native context
    };

    model_registry_["gemma1.1:7b"] = {
        "gemma1.1:7b",
        "gemma-1.1-7b",
        "ggml-org/gemma-1.1-7b-it-Q4_K_M-GGUF",
        "gemma-1.1-7b-it.Q4_K_M.gguf",
        "Q4_K_M",
        9024LL * 1024 * 1024,     // ~5.38 GB
        "Google's lightweight model",
        "Gemma 1.1 7B",
        8192                       // 8K native context
    };

    model_registry_["gemma:2b"] = {
        "gemma:2b",
        "gemma-2b",
        "llm-exp/gemma-2b-Q4_K_M-GGUF",
        "gemma-2b.Q4_K_M.gguf",
        "Q4_K_M",
        1536LL * 1024 * 1024,     // ~1.5 GB
        "Google's lightweight model",
        "Gemma 2B",
        8192                       // 8K native context
    };
    
    model_registry_["gemma:7b"] = {
        "gemma:7b",
        "gemma-7b",
        "goromlagche/gemma-7b-Q4_K_M-GGUF",
        "gemma-7b-q4_k_m.gguf",
        "Q4_K_M",
        4368LL * 1024 * 1024,     // ~4.3 GB
        "Google's efficient model",
        "Gemma 7B",
        8192                       // 8K native context
    };
    
    // ===== GEMMA 3 SERIES (128K) =====
    model_registry_["gemma3:270m"] = {
        "gemma3:270m",
        "gemma3-270m",
        "ggml-org/gemma-3-270m-it-GGUF",
        "gemma-3-270m-it-Q8_0.gguf",
        "Q8_0",
        292LL * 1024 * 1024,      // ~292 MB
        "Ultra-small Gemma 3",
        "Gemma 3 270M",
        32768                    // 32K native context
    };

    model_registry_["gemma3qat:270m"] = {
        "gemma3qat:270m",
        "gemma3-Qat-270m",
        "ggml-org/gemma-3-270m-it-qat-GGUF",
        "gemma-3-270m-it-qat-Q4_0.gguf",
        "Q8_0",
        241LL * 1024 * 1024,      // ~241 MB
        "Ultra-small Gemma 3",
        "Gemma 3 270M Qat",
        32768                    // 32K native context
    };

    model_registry_["gemma3qat:1b"] = {
        "gemma3qat:1b",
        "gemma3-qat-1b",
        "ggml-org/gemma-3-1b-it-qat-GGUF",
        "gemma-3-1b-it-qat-Q4_0.gguf",
        "Q4_0",
        729LL * 1024 * 1024,      // ~729 MB
        "Compact Gemma 3",
        "Gemma 3 1B",
        32768                    // 32K native context (1B variant exception)
    };
    
    model_registry_["gemma3qat:4b"] = {
        "gemma3qat:4b",
        "gemma3-qat-4b",
        "ggml-org/gemma-3-4b-it-qat-GGUF",
        "gemma-3-4b-it-qat-Q4_0.gguf",
        "Q4_0",
        2532LL * 1024 * 1024,     // ~2.53 GB
        "Balanced Gemma 3",
        "Gemma 3 4B",
        131072                    // 128K native context
    };
    
    model_registry_["gemma3qat:12b"] = {
        "gemma3qat:12b",
        "gemma3-qat-12b",
        "ggml-org/gemma-3-12b-it-qat-GGUF",
        "gemma-3-12b-it-qat-Q4_0.gguf",
        "Q4_0",
        7136LL * 1024 * 1024,     // ~7.13 GB
        "Powerful Gemma 3",
        "Gemma 3 12B",
        131072                    // 128K native context
    };
    
    model_registry_["gemma3:1b"] = {
        "gemma3:1b",
        "gemma3-1b",
        "ggml-org/gemma-3-1b-it-GGUF",
        "gemma-3-1b-it-Q8_0.gguf",
        "Q8_0",
        729LL * 1024 * 1024,      // ~1.07 GB
        "Compact Gemma 3",
        "Gemma 3 1B",
        32768                    // 32K native context (1B variant exception)
    };
    
    model_registry_["gemma3:4b"] = {
        "gemma3:4b",
        "gemma3-4b",
        "ggml-org/gemma-3-4b-it-GGUF",
        "gemma-3-4b-it-Q4_K_M.gguf",
        "Q4_K_M",
        2496LL * 1024 * 1024,     // ~2.49 GB
        "Balanced Gemma 3",
        "Gemma 3 4B",
        131072                    // 128K native context
    };
    
    model_registry_["gemma3:12b"] = {
        "gemma3:12b",
        "gemma3-12b",
        "ggml-org/gemma-3-12b-it-GGUF",
        "gemma-3-12b-it-Q4_K_M.gguf",
        "Q4_K_M",
        7372LL * 1024 * 1024,     // ~7.3 GB
        "Powerful Gemma 3",
        "Gemma 3 12B",
        131072                    // 128K native context
    };
    
    model_registry_["gemma3n:e2b"] = {
        "gemma3n:e2b",
        "gemma3n-e2b",
        "unsloth/gemma-3n-E2B-it-GGUF",
        "gemma-3n-E2B-it-Q4_K_M.gguf",
        "Q4_K_M",
        3030LL * 1024 * 1024,     // ~3.03 GB
        "Enhanced 2B variant",
        "Gemma 3N E2B",
        32768                    // 32K native context
    };
    
    model_registry_["gemma3n:e4b"] = {
        "gemma3n:e4b",
        "gemma3n-e4b",
        "unsloth/gemma-3n-E4B-it-GGUF",
        "gemma-3n-E4B-it-Q4_K_M.gguf",
        "Q4_K_M",
        4540LL * 1024 * 1024,     // ~4.54 GB
        "Enhanced 4B variant",
        "Gemma 3N E4B",
        32768                    // 32K native context
    };
    
    // ===== DEEPSEEK R1 SERIES (128K) =====
    model_registry_["deepseek-r1:1.5b"] = {
        "deepseek-r1:1.5b",
        "deepseek-r1-1.5b",
        "unsloth/DeepSeek-R1-Distill-Qwen-1.5B-GGUF",
        "DeepSeek-R1-Distill-Qwen-1.5B-Q4_K_M.gguf",
        "Q4_K_M",
        1024LL * 1024 * 1024,     // ~1 GB
        "Research-focused model",
        "DeepSeek R1 1.5B",
        131072                     // Based on Qwen base (128K)
    };
    
    model_registry_["deepseek-r1:7b"] = {
        "deepseek-r1:7b",
        "deepseek-r1-7b",
        "unsloth/DeepSeek-R1-Distill-Qwen-7B-GGUF",
        "DeepSeek-R1-Distill-Qwen-7B-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Advanced research model",
        "DeepSeek R1 7B",
        131072                    // Based on Qwen base (128K)
    };
    
    model_registry_["deepseek-r1:8b"] = {
        "deepseek-r1:8b",
        "deepseek-r1-8b",
        "unsloth/DeepSeek-R1-Distill-Llama-8B-GGUF",
        "DeepSeek-R1-Distill-Llama-8B-Q4_K_M.gguf",
        "Q4_K_M",
        4915LL * 1024 * 1024,     // ~4.8 GB
        "High-performance research model",
        "DeepSeek R1 8B",
        131072                    // Based on Llama base (128K for Llama 3.1)
    };
    
    // ===== LLAMA 3 SERIES (8K) =====
    model_registry_["llama3:8b"] = {
        "llama3:8b",
        "llama3-8b",
        "QuantFactory/Meta-Llama-3-8B-Instruct-GGUF",
        "Meta-Llama-3-8B-Instruct.Q4_K_M.gguf",
        "Q4_K_M",
        4661LL * 1024 * 1024,     // ~4.7 GB
        "Meta's open-source model",
        "Llama 3 8B",
        8192                      // 8K native context
    };
    
    // ===== LLAMA 3.1 SERIES (Latest Meta models) (128K)=====
    model_registry_["llama3.1:8b"] = {
        "llama3.1:8b",
        "llama3.1-8b",
        "bartowski/Meta-Llama-3.1-8B-Instruct-GGUF",
        "Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4700LL * 1024 * 1024,     // ~4.7 GB
        "Meta's versatile multilingual instruct model",
        "Llama 3.1 8B",
        131072                    // 128K native context
    };
    
    // ===== LLAMA 3.2 SERIES (Vision-Language models) (128K) =====
    model_registry_["llama3.2:1b"] = {
        "llama3.2:1b",
        "llama3.2-1b",
        "bartowski/Llama-3.2-1B-Instruct-GGUF",
        "Llama-3.2-1B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        730LL * 1024 * 1024,      // ~0.73 GB
        "Meta's compact vision-language model",
        "Llama 3.2 1B",
        131072                    // 128K native context
    };
    
    model_registry_["llama3.2:3b"] = {
        "llama3.2:3b",
        "llama3.2-3b",
        "bartowski/Llama-3.2-3B-Instruct-GGUF",
        "Llama-3.2-3B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        2000LL * 1024 * 1024,     // ~2.0 GB
        "Meta's balanced vision-language model for edge devices",
        "Llama 3.2 3B",
        131072                    // 128K native context
    };
    
    // ===== LLAVA (Vision-Language) (4K)=====
    model_registry_["llava"] = {
        "llava",
        "llava",
        "second-state/Llava-v1.5-7B-GGUF",
        "llava-v1.5-7b-Q4_K_M.gguf",
        "Q4_K_M",
        4368LL * 1024 * 1024,     // ~4.3 GB
        "Multimodal vision-language model",
        "LLaVA 1.5 7B",
        4096                      // 4K native context
    };
    
    // ===== LLAMA 2 SERIES (4K) =====
    model_registry_["llama2:7b"] = {
        "llama2:7b",
        "llama2-7b",
        "TheBloke/Llama-2-7B-GGUF",
        "llama-2-7b.Q4_K_M.gguf",
        "Q4_K_M",
        4080LL * 1024 * 1024,     // ~4 GB
        "Original Llama series",
        "Llama 2 7B",
        4096                      // 4K native context
    };
    
    model_registry_["llama2:13b"] = {
        "llama2:13b",
        "llama2-13b",
        "TheBloke/Llama-2-13B-GGUF",
        "llama-2-13b.Q4_K_M.gguf",
        "Q4_K_M",
        7370LL * 1024 * 1024,     // ~7.2 GB
        "Larger original Llama",
        "Llama 2 13B",
        4096                      // 4K native context
    };
    
    // ===== TINYLLAMA (2K) =====
    model_registry_["tinyllama"] = {
        "tinyllama",
        "tinyllama",
        "TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF",
        "tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf",
        "Q4_K_M",
        669LL * 1024 * 1024,      // ~669 MB
        "Ultra-small efficient model",
        "TinyLlama 1.1B",
        2048                      // 2K native context
    };
    
    // ===== BGE-M3 (Embedding) (8K) =====
    model_registry_["bge-m3"] = {
        "bge-m3",
        "bge-m3",
        "groonga/bge-m3-Q4_K_M-GGUF",
        "bge-m3-q4_k_m.gguf",
        "Q4_K_M",
        512LL * 1024 * 1024,      // ~512 MB
        "Embedding model for retrieval",
        "BGE-M3",
        8192                      // 8K context for embeddings
    };
    
    // ===== SMOLLM 2 SERIES (128K  )=====
    model_registry_["smollm2:135m"] = {
        "smollm2:135m",
        "smollm2-135m",
        "Segilmez06/SmolLM2-135M-Instruct-Q4_K_M-GGUF",
        "smollm2-135m-instruct-q4_k_m.gguf",
        "Q4_K_M",
        82LL * 1024 * 1024,       // ~82 MB
        "Tiny SmolLM variant",
        "SmolLM 2 135M",
        131072                      // 128K native context
    };
    
    model_registry_["smollm2:360m"] = {
        "smollm2:360m",
        "smollm2-360m",
        "AIronMind/SmolLM2-360M-Instruct-FT-Q4_K_M-GGUF",
        "smollm2-360m-instruct-ft-q4_k_m.gguf",
        "Q4_K_M",
        220LL * 1024 * 1024,      // ~220 MB
        "Small SmolLM variant",
        "SmolLM 2 360M",
        131072                      // 128K native context
    };
    
    model_registry_["smollm2:1.7b"] = {
        "smollm2:1.7b",
        "smollm2-1.7b",
        "HuggingFaceTB/SmolLM2-1.7B-Instruct-GGUF",
        "smollm2-1.7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        1126LL * 1024 * 1024,     // ~1.1 GB
        "Balanced SmolLM",
        "SmolLM 2 1.7B",
        131072                      // 128K native context
    };
    
    // ===== SMOLLM SERIES (Original) (32K) =====
    model_registry_["smollm:135m"] = {
        "smollm:135m",
        "smollm-135m",
        "QuantFactory/SmolLM-135M-GGUF",
        "SmolLM-135M.Q4_K_M.gguf",
        "Q4_K_M",
        82LL * 1024 * 1024,       // ~82 MB
        "Original tiny SmolLM",
        "SmolLM 135M",
        32768                      // 2K native context
    };
    
    model_registry_["smollm:360m"] = {
        "smollm:360m",
        "smollm-360m",
        "QuantFactory/SmolLM2-360M-GGUF",
        "SmolLM2-360M.Q4_K_M.gguf",
        "Q4_K_M",
        220LL * 1024 * 1024,      // ~220 MB
        "Original small SmolLM",
        "SmolLM 360M",
        32768                      // 32K native context
    };
    
    model_registry_["smollm:1.7b"] = {
        "smollm:1.7b",
        "smollm-1.7b",
        "itlwas/SmolLM-1.7B-Instruct-Q4_K_M-GGUF",
        "smollm-1.7b-instruct-q4_k_m.gguf",
        "Q4_K_M",
        1126LL * 1024 * 1024,     // ~1.1 GB
        "Original balanced SmolLM",
        "SmolLM 1.7B",
        32768                      // 32K native context
    };
    
    // ===== FALCON 3 SERIES (32K) =====
    model_registry_["falcon3:1b"] = {
        "falcon3:1b",
        "falcon3-1b",
        "tiiuae/Falcon3-1B-Instruct-GGUF",
        "Falcon3-1B-Instruct-q4_k_m.gguf",
        "Q4_K_M",
        729LL * 1024 * 1024,      // ~729 MB
        "Efficient small Falcon",
        "Falcon 3 1B",
        32768                      // 32K native context
    };
    
    model_registry_["falcon3:3b"] = {
        "falcon3:3b",
        "falcon3-3b",
        "tiiuae/Falcon3-3B-Instruct-GGUF",
        "Falcon3-3B-Instruct-q4_k_m.gguf",
        "Q4_K_M",
        2048LL * 1024 * 1024,     // ~2 GB
        "Balanced Falcon model",
        "Falcon 3 3B",
        32768                      // 32K native context
    };
    
    model_registry_["falcon3:7b"] = {
        "falcon3:7b",
        "falcon3-7b",
        "bartowski/Falcon3-7B-Instruct-GGUF",
        "Falcon3-7B-Instruct-Q4_K_M.gguf",
        "Q4_K_M",
        4608LL * 1024 * 1024,     // ~4.5 GB
        "Powerful Falcon model",
        "Falcon 3 7B",
        32768                      // 32K native context
    };
    
    // ===== PHI SERIES (4K / 128K) =====
    model_registry_["phi3-mini"] = {
        "phi3-mini",
        "phi3-mini",
        "microsoft/Phi-3-mini-4k-instruct-gguf",
        "Phi-3-mini-4k-instruct-q4.gguf",
        "Q4_K_M",
        2355LL * 1024 * 1024,     // ~2.3 GB
        "Microsoft's reasoning model",
        "Phi-3 Mini",
        4096                      // 4K native context (explicit in filename)
    };
    
    model_registry_["phi2"] = {
        "phi2",
        "phi2",
        "TheBloke/phi-2-GGUF",
        "phi-2.Q4_K_M.gguf",
        "Q4_K_M",
        1638LL * 1024 * 1024,     // ~1.6 GB
        "Improved reasoning model",
        "Phi-2",
        2048                      // 2K native context
    };
    
    model_registry_["phi4-mini"] = {
        "phi4-mini",
        "phi4-mini",
        "tensorblock/Phi-4-mini-instruct-GGUF",
        "Phi-4-mini-instruct-Q4_K_M.gguf",
        "Q4_K_M",
        2458LL * 1024 * 1024,     // ~2.4 GB
        "Compact Phi variant",
        "Phi-4 Mini",
        131072                     // 128K native context
    };
    
    // ===== GRANITE4 SERIES (IBM Granite models) (128K) =====
    model_registry_["granite4:350m"] = {
        "granite4:350m",
        "granite4-350m",
        "unsloth/granite-4.0-350m-GGUF",
        "granite-4.0-350m-Q4_K_M.gguf",
        "Q4_K_M",
        237LL * 1024 * 1024,      // ~237 MB
        "Ultra-compact Granite 4 model",
        "Granite 4 350M",
        32768                      // 32K native context
    };
    
    model_registry_["granite4:350m-h"] = {
        "granite4:350m-h",
        "granite4-350m-h",
        "unsloth/granite-4.0-h-350m-GGUF",
        "granite-4.0-h-350m-Q4_K_M.gguf",
        "Q4_K_M",
        223LL * 1024 * 1024,      // ~223 MB
        "Ultra-compact Granite 4 model (HF format)",
        "Granite 4 350M-H",
        1048576                      // 1024K native context
    };
    
    model_registry_["granite4:1b"] = {
        "granite4:1b",
        "granite4-1b",
        "unsloth/granite-4.0-1b-GGUF",
        "granite-4.0-1b-Q4_K_M.gguf",
        "Q4_K_M",
        1020LL * 1024 * 1024,      // ~1.02 GB
        "Compact Granite 4 model",
        "Granite 4 1B",
        131072                      // 128K native context
    };
    
    model_registry_["granite4:1b-h"] = {
        "granite4:1b-h",
        "granite4-1b-h",
        "unsloth/granite-4.0-h-1b-GGUF",
        "granite-4.0-h-1b-Q4_K_M.gguf",
        "Q4_K_M",
        901LL * 1024 * 1024,      // ~901 MB
        "Compact Granite 4 model (HF format)",
        "Granite 4 1B-H",
        1048576                      // 1024K native context
    };
    
    // model_registry_["granite4:3b"] = {
    //     "granite4:3b",
    //     "granite4-3b",
    //     "ibm/Granite-4-3B-Instruct-GGUF",
    //     "granite-4-3b-instruct-Q4_K_M.gguf",
    //     "Q4_K_M",
    //     1946LL * 1024 * 1024,     // ~1.9 GB
    //     "Balanced Granite 4 model",
    //     "Granite 4 3B",
    //     131072                      // 128K native context
    // };
    
    model_registry_["granite4:micro"] = {
        "granite4:micro",
        "granite4-micro",
        "ibm-granite/granite-4.0-micro-GGUF",
        "granite-4.0-micro-Q4_K_M.gguf",
        "Q4_K_M",
        2100LL * 1024 * 1024,      // ~2.1 GB (estimated)
        "Tiny Granite 4 model",
        "Granite 4 Micro",
        131072                      // 128K native context
    };
    
    // model_registry_["granite4:3b-h"] = {
    //     "granite4:3b-h",
    //     "granite4-3b-h",
    //     "granite-4-3b-instruct-hf-Q4_K_M.gguf",
    //     "granite-4-3b-instruct-hf-Q4_K_M.gguf",
    //     "Q4_K_M",
    //     1946LL * 1024 * 1024,     // ~1.9 GB
    //     "Balanced Granite 4 model (HF format)",
    //     "Granite 4 3B-H",
    //     131072                      // 128K native context
    // };
    
    model_registry_["granite4:h-micro"] = {
        "granite4:h-micro",
        "granite4-h-micro",
        "ibm-granite/granite-4.0-h-micro-GGUF",
        "granite-4.0-h-micro-Q4_K_M.gguf",
        "Q4_K_M",
        1940LL * 1024 * 1024,      // ~1.94 GB (estimated)
        "Tiny Granite 4 model (HF format)",
        "Granite 4 Micro-H",
        1048576                      // 1024K native context
    };
    
    // model_registry_["granite4:7b-a1b-h"] = {
    //     "granite4:7b-a1b-h",
    //     "granite4-7b-a1b-h",
    //     "ibm/Granite-4-7B-A1B-Instruct-GGUF",
    //     "granite-4-7b-a1b-instruct-hf-Q4_K_M.gguf",
    //     "Q4_K_M",
    //     4608LL * 1024 * 1024,     // ~4.5 GB
    //     "Powerful Granite 4 7B A1B model (HF format)",
    //     "Granite 4 7B-A1B-H",
    //     131072                      // 128K native context
    // };
    
    model_registry_["granite4:h-tiny"] = {
        "granite4:h-tiny",
        "granite4-h-tiny",
        "unsloth/granite-4.0-h-tiny-GGUF",
        "granite-4.0-h-tiny-Q4_K_M.gguf",
        "Q4_K_M",
        4250LL * 1024 * 1024,       // ~4.25 GB (estimated)
        "Ultra-tiny Granite 4 model (HF format)",
        "Granite 4 Tiny-H",
        1048576                      // 1024K native context
    };
    
    // model_registry_["granite4:32b-a9b-h"] = {
    //     "granite4:32b-a9b-h",
    //     "granite4-32b-a9b-h",
    //     "ibm/Granite-4-32B-A9B-Instruct-GGUF",
    //     "granite-4-32b-a9b-instruct-hf-Q4_K_M.gguf",
    //     "Q4_K_M",
    //     18432LL * 1024 * 1024,    // ~18 GB
    //     "Large Granite 4 32B A9B model (HF format)",
    //     "Granite 4 32B-A9B-H",
    //     131072                      // 128K native context
    // };
    
    // model_registry_["granite4:small-h"] = {
    //     "granite4:small-h",
    //     "granite4-small-h",
    //     "ibm-granite/granite-4.0-h-small-GGUF",
    //     "granite-4.0-h-small-Q4_K_M.gguf",
    //     "Q4_K_M",
    //     512LL * 1024 * 1024,      // ~512 MB (estimated)
    //     "Small Granite 4 model (HF format)",
    //     "Granite 4 Small-H",
    //     131072                      // 128K native context
    // };

    model_registry_["mistral-3:3b"] = {
        "mistral-3:3b",
        "mistral-3-3b",
        "mistralai/Ministral-3-3B-Instruct-2512-GGUF",
        "Ministral-3-3B-Instruct-2512-Q4_K_M.gguf",
        "Q4_K_M",
        2150LL * 1024 * 1024,       // 2.15 GB
        "Edge Instruct model",
        "mistral 3 3b",
        262144
    };

    model_registry_["mistral-3:8b"] = {
        "mistral-3:8b",
        "mistral-3-8b",
        "mistralai/Ministral-3-8B-Instruct-2512-GGUF",
        "Ministral-3-8B-Instruct-2512-Q4_K_M.gguf",
        "Q4_K_M",
        5200LL * 1024 * 1024,       // 5.2 GB
        "Edge Instruct model",
        "mistral 3 8b",
        262144
    };

    model_registry_["mistral-3:14b"] = {
        "mistral-3:14b",
        "mistral-3-14b",
        "mistralai/Ministral-3-14B-Instruct-2512-GGUF",
        "Ministral-3-14B-Instruct-2512-Q4_K_M.gguf",
        "Q4_K_M",
        8240LL * 1024 * 1024,       // 8.24 GB
        "Edge Instruct model",
        "mistral 3 14b",
        262144
    };

    model_registry_["mistral-3R:3b"] = {
        "mistral-3R:3b",
        "mistral-3R-3b",
        "mistralai/Ministral-3-3B-Reasoning-2512-GGUF",
        "Ministral-3-3B-Reasoning-2512-Q4_K_M.gguf",
        "Q4_K_M",
        2150LL * 1024 * 1024,       // 2.15 GB
        "Edge Reasoning model",
        "mistral 3 Reasoning 3b",
        262144
    };

    model_registry_["mistral-3R:8b"] = {
        "mistral-3R:8b",
        "mistral-3R-8b",
        "mistralai/Ministral-3-8B-Reasoning-2512-GGUF",
        "Ministral-3-8B-Reasoning-2512-Q4_K_M.gguf",
        "Q4_K_M",
        5200LL * 1024 * 1024,       // 5.2 GB
        "Edge Reasoning model",
        "mistral 3 Reasoning 8b",
        262144
    };

    model_registry_["mistral-3R:14b"] = {
        "mistral-3R:14b",
        "mistral-3R-14b",
        "mistralai/Ministral-3-14B-Reasoning-2512-GGUF",
        "Ministral-3-14B-Reasoning-2512-Q4_K_M.gguf",
        "Q4_K_M",
        8240LL * 1024 * 1024,       // 8.24 GB
        "Edge Reasoning model",
        "mistral 3 Reasoning 14b",
        262144
    };

    model_registry_["mistral:7b"] = {
        "mistral:7b",
        "mistral-7b",
        "TheBloke/Mistral-7B-Instruct-v0.2-GGUF",
        "mistral-7b-instruct-v0.2.Q4_K_M.gguf",
        "Q4_K_M",
        4370LL * 1024 * 1024,       // 4.37 GB
        "Edge Instruct model",
        "mistral Instruct 7b",
        32768
    };

    model_registry_["Nemotron-3-Nano:30B-A3B"] = {
        "Nemotron-3-Nano:30B-A3B",
        "Nemotron-3-Nano-30B-A3B",
        "unsloth/Nemotron-3-Nano-30B-A3B-GGUF",
        "Nemotron-3-Nano-30B-A3B-UD-Q4_K_XL.gguf",
        "Q4_K_M",
        24600LL * 1024 * 1024,        // 24.6
        "Reasoning and Non-Reasoning Task",
        "Nemotron-3-Nano-30B-A3B",
        1048576
    };

    model_registry_["Devstral-Small-2:24B"] = {
        "Devstral-Small-2:24B",
        "Devstral-Small-2-24B",
        "unsloth/Devstral-Small-2-24B-Instruct-2512-GGUF",
        "Devstral-Small-2-24B-Instruct-2512-Q4_K_M.gguf",
        "Q4_K_M",
        14300LL * 1024 * 1024,
        "gentic LLM for software engineering tasks",
        "Devstral-Small-2-24B",
        393216
    };

    model_registry_["GML-4.6V-Flash"] = {
        "GML-4.6V-Flash",
        "GML-4.6V-Flash",
        "ggml-org/GLM-4.6V-Flash-GGUF",
        "GLM-4.6V-Flash-Q4_K_M.gguf",
        "Q4_K_M",
        6170LL * 1024 * 1024,
        "lightweight model optimized for local deployment and low-latency applications",
        "GML-4.6V-Flash",
        131072
    };

    model_registry_["AutoGLM-Phone:9B"] = {
        "AutoGLM-Phone:9B",
        "AutoGLM-Phone-9B",
        "ggml-org/AutoGLM-Phone-9B-GGUF",
        "AutoGLM-Phone-9B-Q4_K_M.gguf",
        "Q4_K_M",
        6170LL * 1024 * 1024,
        "lightweight model optimized for local deployment and low-latency applications",
        "AutoGLM-Phone-9B",
        65536
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

std::string ModelManager::get_short_name_from_filename(const std::string& filename) {
    // Get short_name from filename by looking up in registry
    // Accepts filename with or without .gguf extension
    std::string search_filename = filename;
    if (search_filename.length() >= 5 && 
        search_filename.substr(search_filename.length() - 5) != ".gguf") {
        search_filename += ".gguf";
    }
    
    // Search registry for matching filename
    for (const auto& pair : model_registry_) {
        if (pair.second.filename == search_filename) {
            return pair.second.short_name;
        }
    }
    
    // If not found, return empty string
    return "";
}

std::string ModelManager::get_name_from_filename(const std::string& filename) {
    // Get name (with colon) from filename by looking up in registry
    // Accepts filename with or without .gguf extension
    std::string search_filename = filename;
    if (search_filename.length() >= 5 && 
        search_filename.substr(search_filename.length() - 5) != ".gguf") {
        search_filename += ".gguf";
    }
    
    // Search registry for matching filename
    for (const auto& pair : model_registry_) {
        if (pair.second.filename == search_filename) {
            return pair.second.name;  // Return name (e.g., "qwen3:0.6b") instead of short_name
        }
    }
    
    // If not found, return empty string
    return "";
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

