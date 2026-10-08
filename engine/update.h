#ifndef DELTA_UPDATE_H
#define DELTA_UPDATE_H

#include <string>

namespace delta {

// Version information
struct Version {
    int major;
    int minor;
    int patch;
    std::string tag; // e.g., "v1.1.0"

    bool is_newer_than(const Version& other) const;
    std::string to_string() const;
    static Version from_string(const std::string& ver_str);
};

// Update manager for automatic updates
class UpdateManager {
  public:
    UpdateManager();
    ~UpdateManager();

    // Get current version
    static Version get_current_version();

    // Check for updates from GitHub API
    // Returns true if update available, false if up-to-date or offline
    bool check_for_updates(bool verbose = true);

    // Download the latest release for this platform, check its SHA-256 against the published
    // checksums, and replace the installed binaries and web UI. Package-managed installs are
    // pointed at their own upgrade command instead.
    bool perform_update();

  private:
    std::string github_api_url_;
    std::string download_url_;
    std::string checksums_url_;
    Version latest_version_;
    bool update_available_;

    // HTTP request helper
    std::string fetch_url(const std::string& url);

    // Parse GitHub API response
    bool parse_release_info(const std::string& json);

    // Download binary
    bool download_binary(const std::string& url, const std::string& dest);

    // Get platform-specific binary URL
    std::string get_binary_url_for_platform();
};

} // namespace delta

#endif // DELTA_UPDATE_H
