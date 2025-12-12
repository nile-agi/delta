/**
 * Authentication Module - Optional one-time telemetry for Delta CLI
 */

#include "delta_cli.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>

#ifdef _WIN32
    #include <windows.h>
    #include <rpc.h>
#else
    #include <uuid/uuid.h>
#endif

// Optional: libcurl for HTTP requests
#ifdef USE_CURL
    #include <curl/curl.h>
#endif

namespace delta {

Auth::Auth() {
    // Get config directory
    std::string home = tools::FileOps::get_home_dir();
    std::string config_dir = tools::FileOps::join_path(home, ".delta-cli");
    
    if (!tools::FileOps::dir_exists(config_dir)) {
        tools::FileOps::create_dir(config_dir);
    }
    
    config_path_ = tools::FileOps::join_path(config_dir, "config.txt");
    load_config();
}

Auth::~Auth() {
}

bool Auth::is_first_run() {
    return !tools::FileOps::file_exists(config_path_);
}

void Auth::handle_first_run() {
    UI::print_border("First Time Setup");
    
    std::cout << "\n";
    std::cout << "Welcome to Delta CLI! This is your first time running the application.\n";
    std::cout << "\n";
    std::cout << "Optional Telemetry:\n";
    std::cout << "To help improve Delta CLI, we can send anonymous install statistics\n";
    std::cout << "to our tracking server. This includes only:\n";
    std::cout << "  • A random device UUID (no personal information)\n";
    std::cout << "  • Your platform (e.g., Linux, macOS, Windows)\n";
    std::cout << "\n";
    std::cout << "This data helps us understand how Delta CLI is used across platforms.\n";
    std::cout << "You can decline and Delta CLI will work perfectly offline.\n";
    std::cout << "\n";
    
    std::cout << "Would you like to send anonymous install statistics? (y/n): ";
    std::string response;
    std::getline(std::cin, response);
    
    // Generate or get UUID
    uuid_ = get_device_uuid();
    
    // Save config (marks first run as complete)
    save_config();
    
    if (response == "y" || response == "Y" || response == "yes" || response == "Yes") {
        std::string platform = get_platform();
        UI::print_info("Thank you! Sending install data...");
        
        bool success = send_install_data(uuid_, platform);
        if (success) {
            UI::print_info("Install data sent successfully");
        } else {
            UI::print_info("Could not reach server (offline mode enabled)");
        }
    } else {
        UI::print_info("Telemetry disabled. Continuing in offline mode.");
    }
    
    std::cout << "\n";
}

bool Auth::send_install_data(const std::string& uuid, const std::string& platform) {
#ifdef USE_CURL
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    // Build JSON payload
    std::string payload = "{\"uuid\":\"" + uuid + "\",\"platform\":\"" + platform + "\"}";
    
    // Set options
    curl_easy_setopt(curl, CURLOPT_URL, "https://delta-dashboard.vercel.app/track");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L); // 5 second timeout
    
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Perform request (fail silently)
    CURLcode res = curl_easy_perform(curl);
    
    bool success = (res == CURLE_OK);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return success;
#else
    // If curl is not available, fail silently
    return false;
#endif
}

std::string Auth::get_device_uuid() {
    if (!uuid_.empty()) {
        return uuid_;
    }
    
#ifdef _WIN32
    // Windows UUID generation
    UUID uuid;
    UuidCreate(&uuid);
    char* str;
    UuidToStringA(&uuid, (RPC_CSTR*)&str);
    std::string result(str);
    RpcStringFreeA((RPC_CSTR*)&str);
    return result;
#else
    // Unix UUID generation
    uuid_t uuid;
    uuid_generate(uuid);
    char str[37];
    uuid_unparse(uuid, str);
    return std::string(str);
#endif
}

std::string Auth::get_platform() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    #if TARGET_OS_IOS
        return "iOS";
    #else
        return "macOS";
    #endif
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__linux__)
    return "Linux";
#elif defined(__unix__)
    return "Unix";
#else
    return "Unknown";
#endif
}

bool Auth::load_config() {
    if (!tools::FileOps::file_exists(config_path_)) {
        return false;
    }
    
    std::string content = tools::FileOps::read_file(config_path_);
    std::istringstream iss(content);
    std::string line;
    
    while (std::getline(iss, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            if (key == "uuid") {
                uuid_ = value;
            }
        }
    }
    
    return true;
}

void Auth::save_config() {
    if (uuid_.empty()) {
        uuid_ = get_device_uuid();
    }
    
    std::string content = "uuid=" + uuid_ + "\n";
    content += "first_run_complete=true\n";
    
    tools::FileOps::write_file(config_path_, content);
}

} // namespace delta

