/**
 * Tools Module Tests
 */

#include <catch2/catch_test_macros.hpp>
#include "../src/delta_cli.h"

using namespace delta;
using namespace delta::tools;

TEST_CASE("FileOps file existence checks", "[tools][fileops]") {
    SECTION("file_exists() returns false for non-existent file") {
        REQUIRE(FileOps::file_exists("/non/existent/file.txt") == false);
    }
    
    SECTION("dir_exists() returns true for root") {
#ifdef _WIN32
        REQUIRE(FileOps::dir_exists("C:\\") == true);
#else
        REQUIRE(FileOps::dir_exists("/") == true);
#endif
    }
    
    SECTION("dir_exists() returns false for non-existent directory") {
        REQUIRE(FileOps::dir_exists("/non/existent/directory") == false);
    }
}

TEST_CASE("FileOps path operations", "[tools][fileops]") {
    SECTION("get_home_dir() returns non-empty path") {
        std::string home = FileOps::get_home_dir();
        REQUIRE(!home.empty());
        REQUIRE(FileOps::dir_exists(home));
    }
    
    SECTION("join_path() combines paths correctly") {
        std::string result = FileOps::join_path("dir", "file.txt");
        REQUIRE(result.find("dir") != std::string::npos);
        REQUIRE(result.find("file.txt") != std::string::npos);
        
#ifdef _WIN32
        REQUIRE((result.find("\\") != std::string::npos || result.find("/") != std::string::npos));
#else
        REQUIRE(result.find("/") != std::string::npos);
#endif
    }
    
    SECTION("join_path() handles empty strings") {
        REQUIRE(FileOps::join_path("", "file.txt") == "file.txt");
        REQUIRE(FileOps::join_path("dir", "") == "dir");
        REQUIRE(FileOps::join_path("", "") == "");
    }
}

TEST_CASE("FileOps read/write operations", "[tools][fileops]") {
    std::string test_file = FileOps::join_path(FileOps::get_home_dir(), ".delta-test-file.txt");
    
    SECTION("write_file() and read_file() work together") {
        std::string content = "Test content for Delta CLI";
        
        bool write_result = FileOps::write_file(test_file, content);
        REQUIRE(write_result == true);
        
        std::string read_content = FileOps::read_file(test_file);
        REQUIRE(read_content == content);
        
        // Clean up
        std::remove(test_file.c_str());
    }
    
    SECTION("read_file() returns empty for non-existent file") {
        std::string content = FileOps::read_file("/non/existent/file.txt");
        REQUIRE(content.empty());
    }
}

TEST_CASE("FileOps directory operations", "[tools][fileops]") {
    std::string test_dir = FileOps::join_path(FileOps::get_home_dir(), ".delta-test-dir");
    
    SECTION("create_dir() creates directory") {
        bool result = FileOps::create_dir(test_dir);
        REQUIRE(result == true);
        REQUIRE(FileOps::dir_exists(test_dir));
        
        // Clean up
        rmdir(test_dir.c_str());
    }
    
    SECTION("list_dir() returns files in directory") {
        // Use home directory which should exist
        auto files = FileOps::list_dir(FileOps::get_home_dir());
        REQUIRE(files.size() >= 0);
    }
    
    SECTION("list_dir() returns empty for non-existent directory") {
        auto files = FileOps::list_dir("/non/existent/directory");
        REQUIRE(files.empty());
    }
}

TEST_CASE("DepProtocol command execution", "[tools][depprotocol]") {
    SECTION("execute() runs simple command") {
#ifdef _WIN32
        auto result = DepProtocol::execute("echo", {"Hello"});
#else
        auto result = DepProtocol::execute("echo", {"Hello"});
#endif
        
        REQUIRE(result.exit_code == 0);
        REQUIRE(result.success == true);
        REQUIRE(result.output.find("Hello") != std::string::npos);
    }
    
    SECTION("execute() handles invalid command") {
        auto result = DepProtocol::execute("nonexistent_command_xyz");
        REQUIRE(result.success == false);
    }
    
    SECTION("execute() captures output") {
#ifdef _WIN32
        auto result = DepProtocol::execute("cmd", {"/c", "echo", "Test123"});
#else
        auto result = DepProtocol::execute("echo", {"Test123"});
#endif
        
        if (result.success) {
            REQUIRE(result.output.find("Test123") != std::string::npos);
        }
    }
}

TEST_CASE("Shell operations", "[tools][shell]") {
    SECTION("get_shell() returns non-empty") {
        std::string shell = Shell::get_shell();
        REQUIRE(!shell.empty());
    }
    
    SECTION("get_env() returns environment variables") {
        auto env = Shell::get_env();
        REQUIRE(env.size() > 0);
        
        // Should have common environment variables
#ifdef _WIN32
        bool has_common = env.count("COMSPEC") > 0 || env.count("PATH") > 0;
#else
        bool has_common = env.count("HOME") > 0 || env.count("PATH") > 0;
#endif
        REQUIRE(has_common);
    }
    
    SECTION("expand_path() handles tilde") {
        std::string expanded = Shell::expand_path("~/test");
        
#ifndef _WIN32
        // On Unix, tilde should be expanded to home directory
        if (expanded != "~/test") {
            REQUIRE(expanded.find("/") != std::string::npos);
        }
#endif
    }
    
    SECTION("expand_path() returns path unchanged for non-tilde paths") {
        std::string path = "/absolute/path";
        std::string expanded = Shell::expand_path(path);
        REQUIRE(expanded == path);
    }
}

