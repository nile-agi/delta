/**
 * UI Module Tests
 */

#include <catch2/catch_test_macros.hpp>
#include "../src/delta_cli.h"
#include <sstream>
#include <iostream>

using namespace delta;

TEST_CASE("UI initialization", "[ui]") {
    SECTION("init() should not throw") {
        REQUIRE_NOTHROW(UI::init());
    }
}

TEST_CASE("UI output methods", "[ui]") {
    // Redirect cout to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    
    SECTION("print_banner() produces output") {
        UI::print_banner();
        std::string output = buffer.str();
        REQUIRE(output.length() > 0);
        REQUIRE(output.find("DELTA") != std::string::npos);
    }
    
    SECTION("print_error() displays error message") {
        buffer.str("");
        UI::print_error("Test error");
        std::string output = buffer.str();
        REQUIRE(output.find("Test error") != std::string::npos);
        REQUIRE(output.find("Error") != std::string::npos);
    }
    
    SECTION("print_info() displays info message") {
        buffer.str("");
        UI::print_info("Test info");
        std::string output = buffer.str();
        REQUIRE(output.find("Test info") != std::string::npos);
    }
    
    SECTION("print_response() displays text") {
        buffer.str("");
        UI::print_response("Test response");
        std::string output = buffer.str();
        REQUIRE(output.find("Test response") != std::string::npos);
    }
    
    SECTION("print_prompt() displays prompt") {
        buffer.str("");
        UI::print_prompt();
        std::string output = buffer.str();
        REQUIRE(output.find("Δ>") != std::string::npos);
    }
    
    // Restore cout
    std::cout.rdbuf(old);
}

TEST_CASE("UI border printing", "[ui]") {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
    
    SECTION("print_border() without title") {
        UI::print_border();
        std::string output = buffer.str();
        REQUIRE(output.length() > 0);
        REQUIRE((output.find("╔") != std::string::npos || output.find("=") != std::string::npos));
    }
    
    SECTION("print_border() with title") {
        buffer.str("");
        UI::print_border("Test Title");
        std::string output = buffer.str();
        REQUIRE(output.find("Test Title") != std::string::npos);
    }
    
    std::cout.rdbuf(old);
}

