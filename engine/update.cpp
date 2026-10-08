/**
 * Update Module - Automatic update checking and installation for Delta CLI
 */

#include "update.h"
#include "update_install.h"
#include "delta_cli.h"
#include "vendor/json.hpp"
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
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

std::string UpdateManager::get_binary_url_for_platform() {
    return download_url_;
}

// libcurl write callback for a file.
static size_t write_file_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* file = static_cast<std::ofstream*>(userp);
    file->write(static_cast<const char*>(contents), static_cast<std::streamsize>(size * nmemb));
    return file->good() ? size * nmemb : 0;
}

bool UpdateManager::download_binary(const std::string& url, const std::string& dest) {
    std::ofstream file(dest, std::ios::binary);
    if (!file.is_open())
        return false;

    bool success = false;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Delta-CLI/1.0");
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        // Archives are tens of megabytes; give up only if the transfer stalls.
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
        success = curl_easy_perform(curl) == CURLE_OK;
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    file.close();
    return success && file.good();
}

bool UpdateManager::perform_update() {
    namespace fs = std::filesystem;

    if (!update_available_ && !check_for_updates(false)) {
        UI::print_info("You have the latest version (" + get_current_version().to_string() + ").");
        return true;
    }

    // Replace the files this delta actually runs from, following any symlink to them.
    std::error_code ec;
    const fs::path exe_dir = fs::weakly_canonical(fs::path(tools::FileOps::get_executable_dir()), ec);
    const std::string bin_dir = exe_dir.string();
    const std::string manager = update::package_manager_for((exe_dir / "delta").string());
    if (!manager.empty()) {
        UI::print_info("Delta was installed with " + manager + ", so update it the same way:");
        std::cout << "    " << update::upgrade_command(manager) << std::endl << std::endl;
        return false;
    }

    const std::string archive_url = get_binary_url_for_platform();
    if (archive_url.empty() || checksums_url_.empty()) {
        UI::print_error("The latest release has no verified CLI download for this platform.");
        UI::print_info("Download Delta from: " + RELEASES_PAGE + "/latest");
        return false;
    }

    // Check we can write there before downloading anything.
    {
        const fs::path probe = exe_dir / ".delta-update-probe";
        std::ofstream test(probe);
        const bool writable = test.good();
        test.close();
        fs::remove(probe, ec);
        if (!writable) {
#ifdef _WIN32
            UI::print_error("No permission to change " + bin_dir + ". Run the update from an administrator terminal.");
#else
            UI::print_error("No permission to change " + bin_dir + ". Run: sudo delta --update");
#endif
            return false;
        }
    }

    UI::print_border("UPDATING DELTA");
    UI::print_info(get_current_version().to_string() + " -> " + latest_version_.to_string());

    const fs::path work = fs::temp_directory_path(ec) / ("delta-update-" + latest_version_.to_string());
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);
    const std::string archive_name = archive_url.substr(archive_url.find_last_of('/') + 1);
    const fs::path archive = work / archive_name;

    UI::print_info("Downloading " + archive_name + "...");
    if (!download_binary(archive_url, archive.string())) {
        UI::print_error("Download failed. Check your connection and try again.");
        fs::remove_all(work, ec);
        return false;
    }

    // Never install anything that does not match the published checksum.
    const std::string expected = update::checksum_for(fetch_url(checksums_url_), archive_name);
    const std::string actual = update::sha256_file(archive.string());
    if (expected.empty() || actual != expected) {
        UI::print_error(expected.empty() ? "The release lists no checksum for " + archive_name + "; not installing it."
                                         : "The download does not match its published checksum; not installing it.");
        fs::remove_all(work, ec);
        return false;
    }
    UI::print_info("Checksum verified.");

    const std::string webui_dir = update::webui_dir_for(bin_dir);
    auto result = update::install_archive(archive.string(), work.string(), bin_dir, webui_dir);
    fs::remove_all(work, ec);
    if (!result.ok) {
        UI::print_error("Update failed, nothing was changed: " + result.error);
        return false;
    }

    UI::print_info("Updated to " + latest_version_.to_string() + " in " + bin_dir +
                   (webui_dir.empty() ? "." : ", web UI included."));
    UI::print_info("Restart Delta (and any running delta server) to use the new version.");
    UI::print_info("Release notes: " + RELEASES_PAGE + "/tag/" + latest_version_.tag);
    return true;
}

} // namespace delta
