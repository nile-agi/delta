cmake_minimum_required(VERSION 3.14)
project(delta-cli VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure consistent C++ standard library linking (macOS)
if(APPLE)
    # Force use of libc++ (not libstdc++)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

# Options for llama-cpp features
option(LLAMA_CUDA "Enable CUDA support" OFF)
option(LLAMA_VULKAN "Enable Vulkan support" OFF)
option(LLAMA_HIPBLAS "Enable HIP/ROCm support" OFF)
option(LLAMA_BLAS "Enable BLAS support" OFF)
option(USE_CURL "Enable HTTP requests for telemetry" ON)
option(USE_CROW "Enable Crow HTTP library for server" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_SERVER "Build web dashboard server" ON)
option(GGML_CCACHE "Enable ccache for faster compilation" OFF) # Disable ccache warning
option(GGML_OPENMP "Enable OpenMP support" OFF) # Disable OpenMP warnings

# Detect platform and set Metal support accordingly
if(APPLE)
    option(GGML_METAL "Enable Metal support (macOS)" ON)
    set(GGML_METAL ON) # Force ON on macOS
    add_compile_definitions(TARGET_OS_MAC=1)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_compile_definitions(TARGET_OS_IOS=1)
    endif()
else()
    option(GGML_METAL "Enable Metal support (macOS)" OFF)
    set(GGML_METAL OFF) # Force OFF on non-Apple platforms
endif()

if(ANDROID)
    add_compile_definitions(__ANDROID__=1)
elseif(WIN32)
    add_compile_definitions(_WIN32=1)
elseif(UNIX)
    add_compile_definitions(__linux__=1)
endif()

# Include llama.cpp
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vendor/llama.cpp/CMakeLists.txt")
    message(FATAL_ERROR 
        "llama.cpp not found in vendor/llama.cpp/. Please ensure the vendored copy is present.")
endif()

# Set llama-cpp options before adding subdirectory
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE) # Enable common utils for chat templates
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_METAL ${GGML_METAL} CACHE BOOL "" FORCE) # Pass GGML_METAL to llama-cpp
set(GGML_CCACHE ${GGML_CCACHE} CACHE BOOL "" FORCE) # Pass GGML_CCACHE to llama-cpp
set(GGML_OPENMP ${GGML_OPENMP} CACHE BOOL "" FORCE) # Pass GGML_OPENMP to llama-cpp

# Ensure llama.cpp uses the same C++ standard library (macOS)
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++" CACHE STRING "" FORCE)
endif()

add_subdirectory(vendor/llama.cpp)

# Find dependencies
if(USE_CURL)
    find_package(CURL REQUIRED)
    if(CURL_FOUND)
        add_compile_definitions(USE_CURL=1)
        message(STATUS "libcurl found - HTTP requests enabled")
    else()
        message(FATAL_ERROR "libcurl not found - required for API communication")
    endif()
else()
    message(FATAL_ERROR "CURL is required for API communication")
endif()

# Find UUID library (Unix systems)
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_library(UUID_LIBRARY uuid)
    if(UUID_LIBRARY)
        message(STATUS "libuuid found")
    else()
        message(WARNING "libuuid not found - UUID generation may be limited")
    endif()
endif()

# Source files for Delta CLI with llama.cpp integration
set(DELTA_SOURCES
    src/main.cpp
    src/ui.cpp
    src/auth.cpp
    src/models.cpp
    src/inference.cpp
    src/update.cpp
    src/commands.cpp
    src/history.cpp
    src/tools/file_ops.cpp
    src/tools/dep_protocol.cpp
    src/tools/shell.cpp
    src/tools/browser.cpp
    src/model_api_server.cpp
)

# Main executable
add_executable(delta ${DELTA_SOURCES})

# Include directories (with llama-cpp integration)
target_include_directories(delta PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp/common
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp/tools/server
)

# Link libraries (with llama-cpp integration)
target_link_libraries(delta PRIVATE
    llama
    ggml
    common
    cpp-httplib
    ${CMAKE_THREAD_LIBS_INIT}
)

# Link platform-specific libraries
if(GGML_METAL)
    target_link_libraries(delta PRIVATE ggml-metal)
endif()

if(LLAMA_CUDA)
    target_link_libraries(delta PRIVATE ggml-cuda)
endif()

if(LLAMA_BLAS)
    target_link_libraries(delta PRIVATE ${BLAS_LIBRARIES})
endif()

# Link curl if available
if(USE_CURL AND CURL_FOUND)
    target_link_libraries(delta PRIVATE CURL::libcurl)
endif()

# Ensure C++ standard library is linked (macOS)
if(APPLE)
    # Link C++ standard library explicitly to fix undefined symbols
    target_link_libraries(delta PRIVATE c++)
endif()

# Delta Server Wrapper (customized llama-server)
# Need to include models.cpp for ModelManager and commands.cpp for launch_server_auto
# Also need inference.cpp and tools/browser.cpp for commands.cpp dependencies
add_executable(delta-server 
    src/delta_server_wrapper.cpp 
    src/model_api_server.cpp
    src/models.cpp
    src/commands.cpp
    src/inference.cpp
    src/tools/file_ops.cpp
    src/tools/browser.cpp
    src/ui.cpp
)

# Include directories for delta-server
target_include_directories(delta-server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp/common
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp/tools/server
)

# Link libraries for delta-server
if(NOT APPLE)
    target_link_libraries(delta-server PRIVATE stdc++fs)
endif()

# Link httplib and json for model API server
# Link to the same libraries that llama-server uses
target_link_libraries(delta-server PRIVATE 
    common 
    cpp-httplib 
    ${CMAKE_THREAD_LIBS_INIT}
)

# Link curl for ModelManager (downloads from HuggingFace)
if(USE_CURL AND CURL_FOUND)
    target_link_libraries(delta-server PRIVATE CURL::libcurl)
endif()

# On Windows, link ws2_32 for sockets
if(WIN32)
    target_link_libraries(delta-server PRIVATE ws2_32)
endif()

# Ensure C++ standard library is linked (macOS)
if(APPLE)
    target_link_libraries(delta-server PRIVATE c++)
endif()

# Remove duplicate library warnings and ensure C++ stdlib linking (macOS)
if(APPLE)
    # Explicitly link libc++ and libc++abi
    set_target_properties(delta PROPERTIES
        LINK_FLAGS "-Wl,-no_warn_duplicate_libraries -lc++ -lc++abi"
    )
endif()

# Platform-specific libraries
if(WIN32)
    # Windows: Link shell32 for ShellExecute (browser opening)
    target_link_libraries(delta PRIVATE shell32)
    target_link_libraries(delta PRIVATE rpcrt4) # For UUID
elseif(UNIX AND NOT APPLE)
    if(UUID_LIBRARY)
        target_link_libraries(delta PRIVATE ${UUID_LIBRARY})
    endif()
endif()


# Compiler flags
if(MSVC)
    target_compile_options(delta PRIVATE /W4 /WX-)
else()
    target_compile_options(delta PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Server functionality is provided by llama-server from llama-cpp
# No custom server implementation needed

# Tests
if(BUILD_TESTS)
    # Check if Catch2 is available
    find_package(Catch2 3 QUIET)
    
    if(Catch2_FOUND)
        add_subdirectory(tests)
        message(STATUS "Tests enabled")
    else()
        message(STATUS "Catch2 not found - tests disabled")
    endif()
endif()

# Build Web UI from assets/ directory
# This builds the custom Delta web UI (SvelteKit) and outputs to public/
message(STATUS "")
message(STATUS "Building Web UI from assets/...")

# Check if npm is available
find_program(NPM_EXECUTABLE npm)
if(NPM_EXECUTABLE)
    message(STATUS "  npm found: ${NPM_EXECUTABLE}")
    
    # Check if assets directory exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/package.json")
        message(STATUS "  assets/ directory found")
        
        # Check if public/index.html exists and if it's older than assets/package.json
        set(PUBLIC_INDEX "${CMAKE_SOURCE_DIR}/public/index.html")
        set(ASSETS_PACKAGE_JSON "${CMAKE_SOURCE_DIR}/assets/package.json")
        
        set(SHOULD_BUILD_WEBUI TRUE)
        if(EXISTS "${PUBLIC_INDEX}")
            file(TIMESTAMP "${PUBLIC_INDEX}" INDEX_TIME)
            file(TIMESTAMP "${ASSETS_PACKAGE_JSON}" PACKAGE_TIME)
            if(INDEX_TIME GREATER_EQUAL PACKAGE_TIME)
                message(STATUS "  public/index.html is up to date, skipping build")
                set(SHOULD_BUILD_WEBUI FALSE)
            endif()
        endif()
        
        if(SHOULD_BUILD_WEBUI)
            message(STATUS "  Building web UI (this may take a minute)...")
            
            # Run npm install if node_modules doesn't exist
            if(NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/node_modules")
                message(STATUS "    Installing npm dependencies...")
                execute_process(
                    COMMAND ${NPM_EXECUTABLE} install
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/assets"
                    OUTPUT_QUIET
                    ERROR_QUIET
                    RESULT_VARIABLE NPM_INSTALL_RESULT
                )
                if(NOT NPM_INSTALL_RESULT EQUAL 0)
                    message(WARNING "  npm install failed, web UI may not be available")
                endif()
            endif()
            
            # Run npm run build
            message(STATUS "    Building SvelteKit app...")
            execute_process(
                COMMAND ${NPM_EXECUTABLE} run build
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/assets"
                OUTPUT_QUIET
                ERROR_QUIET
                RESULT_VARIABLE NPM_BUILD_RESULT
            )
            if(NPM_BUILD_RESULT EQUAL 0)
                message(STATUS "  ✓ Web UI built successfully")
            else()
                message(WARNING "  npm build failed, web UI may not be available")
            endif()
        endif()
    else()
        message(WARNING "  assets/package.json not found, skipping web UI build")
    endif()
else()
    message(WARNING "  npm not found, web UI will not be built")
    message(WARNING "  Install Node.js and npm to build the web UI")
endif()

# Installation
install(TARGETS delta delta-server
    RUNTIME DESTINATION bin
)

# Server functionality is provided by llama-server from llama-cpp

# Install models directory structure
install(DIRECTORY DESTINATION share/delta-cli/models)

# Install Web UI from public/ directory (built from assets/)
if(EXISTS "${CMAKE_SOURCE_DIR}/public/index.html.gz")
    message(STATUS "  Installing web UI from public/...")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/public"
        DESTINATION share/delta-cli/webui
        FILES_MATCHING
        PATTERN "*.html"
        PATTERN "*.html.gz"
        PATTERN "*.js"
        PATTERN "*.css"
        PATTERN "*.svg"
        PATTERN "*.png"
        PATTERN "*.jpg"
        PATTERN "*.ico"
        PATTERN "*.woff"
        PATTERN "*.woff2"
        PATTERN "*.ttf"
    )
elseif(EXISTS "${CMAKE_SOURCE_DIR}/public/index.html")
    message(STATUS "  Installing web UI from public/...")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/public"
        DESTINATION share/delta-cli/webui
        FILES_MATCHING
        PATTERN "*.html"
        PATTERN "*.js"
        PATTERN "*.css"
        PATTERN "*.svg"
        PATTERN "*.png"
        PATTERN "*.jpg"
        PATTERN "*.ico"
        PATTERN "*.woff"
        PATTERN "*.woff2"
        PATTERN "*.ttf"
    )
else()
    message(WARNING "  public/index.html not found, web UI will not be installed")
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Delta CLI Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  CUDA support: ${LLAMA_CUDA}")
message(STATUS "  Metal support: ${GGML_METAL}")
message(STATUS "  Vulkan support: ${LLAMA_VULKAN}")
message(STATUS "  HIP/ROCm support: ${LLAMA_HIPBLAS}")
message(STATUS "  BLAS support: ${LLAMA_BLAS}")
message(STATUS "  HTTP telemetry: ${USE_CURL}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build server: ${BUILD_SERVER}")
message(STATUS "  CCache: ${GGML_CCACHE}")
message(STATUS "  OpenMP: ${GGML_OPENMP}")
message(STATUS "")

