/**
 * Update Module - Automatic update checking and installation for Delta CLI
 */

#include "update.h"
#include "delta_cli.h"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace delta {

// Current version - update this with each release
static const int VERSION_MAJOR = 1;
static const int VERSION_MINOR = 0;
static const int VERSION_PATCH = 0;

// GitHub repository info
static const char* GITHUB_REPO_OWNER = "oderoi";
static const char* GITHUB_REPO_NAME = "delta-cli";

// ============================================================================
// Version Implementation
// ============================================================================

bool Version::is_newer_than(const Version& other) const {
    if (major != other.major) return major > other.major;
    if (minor != other.minor) return minor > other.minor;
    return patch > other.patch;
}

std::string Version::to_string() const {
    std::ostringstream oss;
    oss << major << "." << minor << "." << patch;
    return oss.str();
}

Version Version::from_string(const std::string& ver_str) {
    Version v = {0, 0, 0, ""};
    
    // Remove 'v' prefix if present
    std::string s = ver_str;
    if (!s.empty() && s[0] == 'v') {
        s = s.substr(1);
    }
    
    // Parse major.minor.patch
    std::istringstream iss(s);
    char dot;
    iss >> v.major >> dot >> v.minor >> dot >> v.patch;
    v.tag = ver_str;
    
    return v;
}

// ============================================================================
// UpdateManager Implementation
// ============================================================================

UpdateManager::UpdateManager() 
    : update_available_(false) {
    std::ostringstream url;
    url << "https://api.github.com/repos/" 
        << GITHUB_REPO_OWNER << "/" << GITHUB_REPO_NAME << "/releases/latest";
    github_api_url_ = url.str();
}

UpdateManager::~UpdateManager() {
}

Version UpdateManager::get_current_version() {
    Version v;
    v.major = VERSION_MAJOR;
    v.minor = VERSION_MINOR;
    v.patch = VERSION_PATCH;
    
    std::ostringstream tag;
    tag << "v" << v.major << "." << v.minor << "." << v.patch;
    v.tag = tag.str();
    
    return v;
}

// libcurl write callback for string
static size_t write_string_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<const char*>(contents), total_size);
    return total_size;
}

std::string UpdateManager::fetch_url(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string response;
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Delta-CLI/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 10 second timeout
        
        res = curl_easy_perform(curl);
        
        if (res != CURLE_OK) {
            response.clear();  // Clear on error
        }
        
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    return response;
}

bool UpdateManager::parse_release_info(const std::string& json) {
    // Simple JSON parsing for tag_name
    // Look for "tag_name": "v1.1.0"
    size_t tag_pos = json.find("\"tag_name\"");
    if (tag_pos == std::string::npos) {
        return false;
    }
    
    // Find the value after tag_name
    size_t colon_pos = json.find(":", tag_pos);
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    // Find the opening quote
    size_t quote1 = json.find("\"", colon_pos);
    if (quote1 == std::string::npos) {
        return false;
    }
    
    // Find the closing quote
    size_t quote2 = json.find("\"", quote1 + 1);
    if (quote2 == std::string::npos) {
        return false;
    }
    
    // Extract tag
    std::string tag = json.substr(quote1 + 1, quote2 - quote1 - 1);
    latest_version_ = Version::from_string(tag);
    
    // Look for download URLs (assets)
    // This is a simplified version - real implementation would parse all assets
    size_t assets_pos = json.find("\"browser_download_url\"");
    if (assets_pos != std::string::npos) {
        size_t url_start = json.find("\"", json.find(":", assets_pos));
        size_t url_end = json.find("\"", url_start + 1);
        if (url_start != std::string::npos && url_end != std::string::npos) {
            download_url_ = json.substr(url_start + 1, url_end - url_start - 1);
        }
    }
    
    return true;
}

Version UpdateManager::get_latest_version() {
    std::string response = fetch_url(github_api_url_);
    
    if (response.empty()) {
        // Offline or error - return current version
        return get_current_version();
    }
    
    if (parse_release_info(response)) {
        return latest_version_;
    }
    
    // Parse error - return current version
    return get_current_version();
}

bool UpdateManager::check_for_updates(bool verbose) {
    Version current = get_current_version();
    
    if (verbose) {
        UI::print_info("Checking for updates...");
        UI::print_info("Current version: " + current.to_string());
    }
    
    // Fetch latest version info
    std::string response = fetch_url(github_api_url_);
    
    if (response.empty()) {
        if (verbose) {
            UI::print_info("Unable to check for updates (offline or API unavailable)");
            UI::print_info("You have: " + current.to_string());
        }
        return false;
    }
    
    if (!parse_release_info(response)) {
        if (verbose) {
            UI::print_error("Failed to parse update information");
        }
        return false;
    }
    
    // Compare versions
    if (latest_version_.is_newer_than(current)) {
        update_available_ = true;
        
        if (verbose) {
            std::cout << std::endl;
            UI::print_border("UPDATE AVAILABLE");
            std::cout << "  ⚠ New version available: " << latest_version_.to_string() << std::endl;
            std::cout << "  Current version: " << current.to_string() << std::endl;
            std::cout << std::endl;
            UI::print_info("To update, run: delta --update");
            UI::print_info("Or visit: https://github.com/" + std::string(GITHUB_REPO_OWNER) + "/" + std::string(GITHUB_REPO_NAME) + "/releases");
            std::cout << std::endl;
        }
        
        return true;
    } else {
        update_available_ = false;
        
        if (verbose) {
            std::cout << std::endl;
            UI::print_info("✓ You have the latest version (" + current.to_string() + ")");
            std::cout << std::endl;
        }
        
        return false;
    }
}

bool UpdateManager::can_update() const {
#ifdef _WIN32
    // Check if we can write to Program Files
    return true;  // Simplified - would check actual permissions
#else
    // Check if we can write to /usr/local/bin
    return access("/usr/local/bin", W_OK) == 0;
#endif
}

std::string UpdateManager::get_binary_url_for_platform() {
    // Platform-specific binary names
    // In a real implementation, you'd parse the assets list from GitHub
    
#if defined(__APPLE__)
    return download_url_;  // Assumes macOS binary in release
#elif defined(_WIN32)
    return download_url_;  // Assumes Windows binary in release
#elif defined(__linux__)
    return download_url_;  // Assumes Linux binary in release
#else
    return "";
#endif
}

bool UpdateManager::download_binary(const std::string& url, const std::string& dest) {
    CURL* curl;
    CURLcode res;
    bool success = false;
    
    // Open file for writing
    std::ofstream file(dest, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Delta-CLI/1.0");
        
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 200) {
                success = true;
            }
        }
        
        curl_easy_cleanup(curl);
    }
    
    curl_global_cleanup();
    file.close();
    
    return success;
}

bool UpdateManager::backup_current() {
    // Get current executable path
    std::string current_path = "/usr/local/bin/delta";  // Simplified
    std::string backup_path = current_path + ".backup";
    
    // Read current executable
    std::ifstream src(current_path, std::ios::binary);
    if (!src.is_open()) {
        return false;
    }
    
    // Write to backup
    std::ofstream dst(backup_path, std::ios::binary);
    if (!dst.is_open()) {
        return false;
    }
    
    dst << src.rdbuf();
    
    src.close();
    dst.close();
    
    return true;
}

bool UpdateManager::rollback() {
    std::string current_path = "/usr/local/bin/delta";
    std::string backup_path = current_path + ".backup";
    
    // Check if backup exists
    struct stat st;
    if (stat(backup_path.c_str(), &st) != 0) {
        UI::print_error("No backup found to rollback to");
        return false;
    }
    
    // Replace current with backup
    if (std::rename(backup_path.c_str(), current_path.c_str()) != 0) {
        UI::print_error("Failed to rollback");
        return false;
    }
    
    UI::print_info("✓ Rolled back to previous version");
    return true;
}

bool UpdateManager::perform_update() {
    if (!update_available_) {
        // Check first
        if (!check_for_updates(false)) {
            UI::print_info("No updates available");
            return true;  // Not an error
        }
    }
    
    UI::print_border("AUTOMATIC UPDATE");
    UI::print_info("Updating to version: " + latest_version_.to_string());
    std::cout << std::endl;
    
    // Check permissions
    if (!can_update()) {
        UI::print_error("Insufficient permissions to update");
        UI::print_info("Try running with sudo: sudo delta --update");
        return false;
    }
    
    // Inform user about manual update
    UI::print_info("Automatic binary updates are not yet fully implemented.");
    UI::print_info("Please update manually using one of these methods:");
    std::cout << std::endl;
    
    std::cout << "  Method 1 - If installed from source:" << std::endl;
    std::cout << "    cd ~/delta-cli && git pull && git submodule update --recursive --remote && \\" << std::endl;
    std::cout << "    ./installers/build_macos.sh && cd build_macos && sudo cmake --install ." << std::endl;
    std::cout << std::endl;
    
    std::cout << "  Method 2 - Re-run automatic installer:" << std::endl;
#if defined(__APPLE__)
    std::cout << "    bash <(curl -fsSL https://raw.githubusercontent.com/" 
              << GITHUB_REPO_OWNER << "/" << GITHUB_REPO_NAME << "/main/install-macos.sh)" << std::endl;
#elif defined(__linux__)
    std::cout << "    curl -fsSL https://raw.githubusercontent.com/" 
              << GITHUB_REPO_OWNER << "/" << GITHUB_REPO_NAME << "/main/install-linux.sh | bash" << std::endl;
#elif defined(_WIN32)
    std::cout << "    irm https://raw.githubusercontent.com/" 
              << GITHUB_REPO_OWNER << "/" << GITHUB_REPO_NAME << "/main/install-windows.ps1 | iex" << std::endl;
#endif
    std::cout << std::endl;
    
    std::cout << "  Method 3 - Visit releases page:" << std::endl;
    std::cout << "    https://github.com/" << GITHUB_REPO_OWNER << "/" << GITHUB_REPO_NAME << "/releases" << std::endl;
    std::cout << std::endl;
    
    UI::print_info("See UPDATE_GUIDE.md for detailed instructions");
    
    return false;  // Not yet fully implemented
}

} // namespace delta

