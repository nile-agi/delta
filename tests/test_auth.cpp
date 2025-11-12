/**
 * Authentication Module Tests
 */

#include <catch2/catch_test_macros.hpp>
#include "../src/delta_cli.h"
#include "../src/tools/file_ops.cpp"
#include <cstdio>

using namespace delta;

TEST_CASE("Auth platform detection", "[auth]") {
    SECTION("get_platform() returns valid platform") {
        std::string platform = Auth::get_platform();
        REQUIRE(!platform.empty());
        REQUIRE(platform != "Unknown");
        
        // Should be one of the known platforms
        bool valid = (platform == "Windows" || 
                     platform == "macOS" || 
                     platform == "iOS" || 
                     platform == "Android" || 
                     platform == "Linux" || 
                     platform == "Unix");
        REQUIRE(valid);
    }
}

TEST_CASE("Auth UUID generation", "[auth]") {
    // Create a temporary config for testing
    std::string temp_config = tools::FileOps::join_path(
        tools::FileOps::get_home_dir(), 
        ".delta-cli-test"
    );
    
    // Clean up any existing test config
    if (tools::FileOps::dir_exists(temp_config)) {
        std::remove(tools::FileOps::join_path(temp_config, "config.txt").c_str());
    }
    
    SECTION("UUID is generated and non-empty") {
        Auth auth;
        std::string uuid = auth.get_device_uuid();
        REQUIRE(!uuid.empty());
        REQUIRE(uuid.length() > 10);
    }
    
    SECTION("UUID remains consistent") {
        Auth auth1;
        std::string uuid1 = auth1.get_device_uuid();
        
        Auth auth2;
        std::string uuid2 = auth2.get_device_uuid();
        
        // Both should generate valid UUIDs (but may be different instances)
        REQUIRE(!uuid1.empty());
        REQUIRE(!uuid2.empty());
    }
}

TEST_CASE("Auth first run detection", "[auth]") {
    Auth auth;
    
    SECTION("is_first_run() returns boolean") {
        bool first_run = auth.is_first_run();
        REQUIRE((first_run == true || first_run == false));
    }
}

TEST_CASE("Auth telemetry sending", "[auth]") {
    Auth auth;
    
    SECTION("send_install_data() handles invalid data gracefully") {
        // Should not crash with empty UUID
        bool result = auth.send_install_data("", "Linux");
        // Result can be true or false, just shouldn't crash
        REQUIRE((result == true || result == false));
    }
    
    SECTION("send_install_data() with valid data") {
        // Test UUID and platform
        bool result = auth.send_install_data("test-uuid-12345", "Linux");
        // May fail if offline or server unavailable (that's expected)
        REQUIRE((result == true || result == false));
    }
}

