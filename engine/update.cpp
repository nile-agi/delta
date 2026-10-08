/**
 * Update Module - Automatic update checking and installation for Delta CLI
 */

#include "update.h"
#include "delta_cli.h"
#include "vendor/json.hpp"
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

#ifndef DELTA_VERSION
#define DELTA_VERSION "0.0.0"
#endif

// Releases are published to this public repository; the source repository is private.
static const char* RELEASE_REPO = "nile-agi/delta-release";
static const std::string RELEASES_PAGE = std::string("https://github.com/") + RELEASE_REPO + "/releases";

// The CLI archive the release workflow builds for this platform.
static const char* cli_asset_name() {
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    return "delta-cli-macos-arm64.tar.gz";
#elif defined(__APPLE__)
    return "delta-cli-macos-x86_64.tar.gz";
#elif defined(_WIN32)
    return "delta-cli-windows-x64.zip";
#elif defined(__linux__) && defined(__x86_64__)
    return "delta-cli-linux-x86_64.tar.gz";
#else
    return "";
#endif
}

// ============================================================================
// Version Implementation
// ============================================================================

bool Version::is_newer_than(const Version& other) const {
    if (major != other.major)
        return major > other.major;
    if (minor != other.minor)
        return minor > other.minor;
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

UpdateManager::UpdateManager() : update_available_(false) {
    github_api_url_ = std::string("https://api.github.com/repos/") + RELEASE_REPO + "/releases/latest";
}

UpdateManager::~UpdateManager() {}

Version UpdateManager::get_current_version() {
    // Set from version.txt at configure time, so it can no longer drift from the release.
    return Version::from_string(std::string("v") + DELTA_VERSION);
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
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 second timeout

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            response.clear(); // Clear on error
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return response;
}

bool UpdateManager::parse_release_info(const std::string& json) {
    nlohmann::json release = nlohmann::json::parse(json, nullptr, false);
    if (!release.is_object() || !release.contains("tag_name") || !release["tag_name"].is_string())
        return false;
    latest_version_ = Version::from_string(release["tag_name"].get<std::string>());

    // The archive for this platform, not whichever asset happens to come first.
    download_url_.clear();
    checksums_url_.clear();
    const std::string wanted = cli_asset_name();
    if (release.contains("assets") && release["assets"].is_array()) {
        for (const auto& asset : release["assets"]) {
            const std::string name = asset.value("name", "");
            const std::string url = asset.value("browser_download_url", "");
            if (!wanted.empty() && name == wanted)
                download_url_ = url;
            else if (name.rfind("checksums-", 0) == 0 && name.size() > 7 && name.substr(name.size() - 7) == ".sha256")
                checksums_url_ = url;
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
            // GitHub answers {"message": "Not Found"} until a release has been published.
            if (response.find("\"message\"") != std::string::npos)
                UI::print_info("No Delta release has been published yet. See " + RELEASES_PAGE);
            else
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
            UI::print_info("Or visit: " + RELEASES_PAGE);
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
    return true; // Simplified - would check actual permissions
#else
    // Check if we can write to /usr/local/bin
    return access("/usr/local/bin", W_OK) == 0;
#endif
}

std::string UpdateManager::get_binary_url_for_platform() {
    return download_url_;
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
    std::string current_path = "/usr/local/bin/delta"; // Simplified
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
            return true; // Not an error
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

    // Replacing the running binaries in place is not automated yet; give the exact steps instead.
    const std::string archive = cli_asset_name();
    const std::string url = get_binary_url_for_platform();
    if (url.empty()) {
        UI::print_info("No CLI build for this platform in the latest release. Download Delta from:");
        std::cout << "    " << RELEASES_PAGE << "/latest" << std::endl << std::endl;
        return false;
    }

    UI::print_info("Download and check the new version:");
    std::cout << std::endl;
#if defined(_WIN32)
    std::cout << "    curl -LO " << url << std::endl;
    if (!checksums_url_.empty())
        std::cout << "    curl -LO " << checksums_url_ << std::endl;
    std::cout << "    certutil -hashfile " << archive << " SHA256" << std::endl;
    std::cout << std::endl;
    UI::print_info("Compare the hash with the line for " + archive + " in the checksums file, then unzip it");
    UI::print_info("over your current Delta folder.");
#else
    std::cout << "    curl -LO " << url << std::endl;
    if (!checksums_url_.empty()) {
        std::cout << "    curl -LO " << checksums_url_ << std::endl;
        const std::string sums = checksums_url_.substr(checksums_url_.find_last_of('/') + 1);
        std::cout << "    grep " << archive << " " << sums << " | shasum -a 256 -c -" << std::endl;
    }
    std::cout << "    tar -xzf " << archive << std::endl;
    std::cout << std::endl;
    UI::print_info("Then copy delta, delta-server and llama-server over your current install.");
#endif
    std::cout << std::endl;
    UI::print_info("Release notes: " + RELEASES_PAGE + "/latest");

    return false; // Not yet fully implemented
}

} // namespace delta
