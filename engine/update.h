#ifndef DELTA_UPDATE_H
#define DELTA_UPDATE_H

#include <string>

namespace delta {

// Version information
struct Version {
    int major;
    int minor;
    int patch;
    std::string tag;  // e.g., "v1.1.0"
    
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
    
    // Get latest version info from GitHub
    Version get_latest_version();
    
    // Download and install update
    bool perform_update();
    
    // Check if we have write permissions to update
    bool can_update() const;
    
    // Backup current installation
    bool backup_current();
    
    // Rollback to backup
    bool rollback();
    
private:
    std::string github_api_url_;
    std::string download_url_;
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
    
    // Replace current executable
    bool replace_executable(const std::string& new_binary);
};

} // namespace delta

#endif // DELTA_UPDATE_H

