/**
 * Delta CLI Web Server - Simple placeholder
 * This provides a basic web interface for managing models and viewing stats
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// Simple HTTP server implementation without external dependencies

#include "../delta_cli.h"
#include "../history.h"  // For get_history_manager()

using namespace delta;

std::string read_template() {
    std::ifstream file("src/web/templates/delta_dashboard.html");
    if (!file) {
        return R"(<!DOCTYPE html>
<html>
<head>
    <title>Delta CLI Dashboard</title>
    <style>
        body { font-family: monospace; background: #0a0e27; color: #00ff00; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        h1 { border-bottom: 2px solid #00ff00; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Delta CLI Dashboard</h1>
        <p>Dashboard loading...</p>
    </div>
</body>
</html>)";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char** argv) {
    int port = 8080;
    
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    UI::init();
    UI::print_banner();
    UI::print_info("Starting Delta CLI Web Server...");
    
    UI::print_error("Server not compiled with Crow support");
    UI::print_info("To enable, install Crow and rebuild with -DUSE_CROW=ON");
    UI::print_info("");
    UI::print_info("Alternative: Use the Node.js dashboard instead:");
    UI::print_info("  cd dashboard && npm install && npm start");
    return 1;
}

