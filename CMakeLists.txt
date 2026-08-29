cmake_minimum_required(VERSION 3.14)

option(FORCE_SYSTEM_CLANG "Force /usr/bin/clang on macOS to avoid Homebrew ABI mismatch" ON)
if(APPLE AND FORCE_SYSTEM_CLANG)
    set(CMAKE_C_COMPILER "/usr/bin/clang" CACHE FILEPATH "" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE FILEPATH "" FORCE)
endif()

# Read version from version.txt
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" DELTA_VERSION_STRING)
string(STRIP "${DELTA_VERSION_STRING}" DELTA_VERSION_STRING)

project(delta-cli VERSION ${DELTA_VERSION_STRING} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure consistent C++ standard library linking (macOS)
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
endif()

# Options for llama-cpp features
option(LLAMA_CUDA "Enable CUDA support" OFF)
option(GGML_METAL "Enable Metal support (macOS)" ON)
option(LLAMA_VULKAN "Enable Vulkan support" OFF)
option(LLAMA_HIPBLAS "Enable HIP/ROCm support" OFF)
option(LLAMA_BLAS "Enable BLAS support" OFF)
option(USE_CURL "Enable HTTP requests for telemetry" ON)
option(USE_CROW "Enable Crow HTTP library for server" OFF)
option(BUILD_TESTS "Build test suite" ON)
option(GGML_CCACHE "Enable ccache for faster compilation" OFF)
option(GGML_OPENMP "Enable OpenMP support" OFF)

# Add DHATS Options
option(DELTA_BUILD_RPC "Enable Distributed RPC Support" ON)

# Ensure llama.cpp builds with RPC if enabled
if(DELTA_BUILD_RPC)
    set(GGML_RPC ON CACHE BOOL "Enable ggml RPC" FORCE)
    add_compile_definitions(DELTA_RPC_ENABLED)
endif()

option(DELTA_BUILD_CUDA "Build CUDA sidecar (Dynamic Backend Loading)" OFF)
option(DELTA_BUILD_VULKAN "Build Vulkan sidecar (Dynamic Backend Loading)" OFF)

if(DELTA_BUILD_CUDA)
    set(GGML_CUDA ON CACHE BOOL "Enable CUDA" FORCE)
    set(GGML_BACKEND_DL ON CACHE BOOL "Build backends as shared libraries for DTL" FORCE)
    add_compile_definitions(DELTA_CUDA_ENABLED)
    message(STATUS "  DHATS: CUDA sidecar enabled (libggml-cuda will be dynamically loaded)")
endif()

if(DELTA_BUILD_VULKAN)
    set(GGML_VULKAN ON CACHE BOOL "Enable Vulkan" FORCE)
    set(GGML_BACKEND_DL ON CACHE BOOL "Build backends as shared libraries for DTL" FORCE)
    add_compile_definitions(DELTA_VULKAN_ENABLED)
    message(STATUS "  DHATS: Vulkan sidecar enabled (libggml-vulkan will be dynamically loaded)")
endif()

# Detect platform
if(APPLE)
    set(GGML_METAL ON)
    add_compile_definitions(TARGET_OS_MAC=1)
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        add_compile_definitions(TARGET_OS_IOS=1)
    endif()
elseif(ANDROID)
    add_compile_definitions(__ANDROID__=1)
elseif(WIN32)
    add_compile_definitions(_WIN32=1)
elseif(UNIX)
    add_compile_definitions(__linux__=1)
endif()

# Include llama.cpp
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/engine/vendor/llama.cpp/CMakeLists.txt")
    message(FATAL_ERROR
        "llama.cpp not found in engine/vendor/llama.cpp/. Please ensure the vendored copy is present.")
endif()

# Set llama-cpp options before adding subdirectory
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_UI ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_METAL ${GGML_METAL} CACHE BOOL "" FORCE)
set(GGML_CCACHE ${GGML_CCACHE} CACHE BOOL "" FORCE)
set(GGML_OPENMP ${GGML_OPENMP} CACHE BOOL "" FORCE)

# Ensure llama.cpp uses the same C++ standard library (macOS)
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++" CACHE STRING "" FORCE)
endif()

add_subdirectory(engine/vendor/llama.cpp)

# SQLite amalgamation (embedded, no external dependency)
add_library(sqlite3 STATIC engine/vendor/sqlite3/sqlite3.c)
target_include_directories(sqlite3 PUBLIC engine/vendor/sqlite3)
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_JSON1=1
    SQLITE_OMIT_LOAD_EXTENSION=1
)
if(UNIX AND NOT APPLE)
    target_link_libraries(sqlite3 PUBLIC dl pthread)
elseif(APPLE)
    target_link_libraries(sqlite3 PUBLIC pthread)
endif()

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
    engine/main.cpp
    engine/ui.cpp
    engine/auth.cpp
    engine/models.cpp
    engine/inference.cpp
    engine/update.cpp
    engine/commands.cpp
    engine/history.cpp
    engine/tools/hardware_monitor.cpp
    engine/tools/file_ops.cpp
    engine/tools/dep_protocol.cpp
    engine/tools/shell.cpp
    engine/tools/browser.cpp
    engine/model_api_server.cpp
    engine/api/note_routes.cpp
    engine/agent/agent_database.cpp
    engine/agent/memory_store.cpp
    engine/agent/tool_registry.cpp
    engine/agent/tool_calendar.cpp
    engine/agent/tool_notes.cpp
    engine/agent/tool_memory.cpp
    engine/agent/tool_task.cpp
    engine/agent/tool_files.cpp
    engine/agent/tool_shell.cpp
    engine/agent/tool_web.cpp
    engine/agent/llm_client.cpp
    engine/agent/context_manager.cpp
    engine/agent/policy.cpp
    engine/agent/harness.cpp
)

# Common include directories
# Explicitly adding vendor paths to resolve <nlohmann/json.hpp> and <cpp-httplib/httplib.h> correctly
set(DELTA_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/common
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/tools/server
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/vendor/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/vendor/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/agent
)

# Main executable
add_executable(delta ${DELTA_SOURCES})
target_compile_definitions(delta PRIVATE
    DELTA_VERSION="${PROJECT_VERSION}"
)
target_include_directories(delta PRIVATE 
    ${DELTA_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/vendor
)

# Link libraries (with llama-cpp integration)
target_link_libraries(delta PRIVATE
    llama
    ggml
    llama-common
    cpp-httplib
    sqlite3
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

# Ensure C++ standard library and required Apple frameworks are linked (macOS)
if(APPLE)
    target_link_libraries(delta PRIVATE c++)
    target_link_libraries(delta PRIVATE "-framework IOKit" "-framework CoreFoundation")
endif()

# Delta Server Wrapper (customized llama-server)
add_executable(delta-server
    engine/delta_server_wrapper.cpp
    engine/model_api_server.cpp
    engine/models.cpp
    engine/tools/hardware_monitor.cpp
    engine/tools/file_ops.cpp
    engine/ui.cpp
    engine/api/note_routes.cpp
    engine/agent/agent_database.cpp
    engine/agent/memory_store.cpp
    engine/agent/tool_registry.cpp
    engine/agent/tool_calendar.cpp
    engine/agent/tool_notes.cpp
    engine/agent/tool_memory.cpp
    engine/agent/tool_task.cpp
    engine/agent/tool_files.cpp
    engine/agent/tool_shell.cpp
    engine/agent/tool_web.cpp
    engine/agent/llm_client.cpp
    engine/agent/context_manager.cpp
    engine/agent/policy.cpp
    engine/agent/harness.cpp
)
target_compile_definitions(delta-server PRIVATE
    DELTA_VERSION="${PROJECT_VERSION}"
)

if(APPLE)
    target_link_libraries(delta-server PRIVATE "-framework Metal" "-framework IOKit" "-framework CoreFoundation")
endif()

# windows.h (pulled in by curl/winsock) defines min/max macros that break std::min/std::max.
if(WIN32)
    target_compile_definitions(delta PRIVATE NOMINMAX)
    target_compile_definitions(delta-server PRIVATE NOMINMAX)
endif()

# Include directories for delta-server
target_include_directories(delta-server PRIVATE 
    ${DELTA_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/vendor/llama.cpp/vendor
)

# Link libraries for delta-server
if(NOT APPLE AND NOT MSVC)
    target_link_libraries(delta-server PRIVATE stdc++fs)
endif()

# Link the correct vendor targets to automatically inherit their include directories
target_link_libraries(delta-server PRIVATE
    llama
    ggml
    llama-common
    cpp-httplib
    sqlite3
    ${CMAKE_THREAD_LIBS_INIT}
)

# Link server-context if available (used by llama-server wrappers)
if(TARGET server-context)
    target_link_libraries(delta-server PRIVATE server-context)
endif()

# Link curl for ModelManager (downloads from HuggingFace)
if(USE_CURL AND CURL_FOUND)
    target_link_libraries(delta-server PRIVATE CURL::libcurl)
endif()

# On Windows, link ws2_32 for sockets
if(WIN32)
    target_link_libraries(delta-server PRIVATE ws2_32)
endif()

# Ensure C++ standard library and required Apple frameworks are linked (macOS)
if(APPLE)
    target_link_libraries(delta-server PRIVATE c++)
    target_link_libraries(delta-server PRIVATE "-framework IOKit" "-framework CoreFoundation")
endif()

# Remove duplicate library warnings and ensure C++ stdlib linking (macOS)
if(APPLE)
    set_target_properties(delta PROPERTIES
        LINK_FLAGS "-Wl,-no_warn_duplicate_libraries -lc++ -lc++abi"
    )
    set_target_properties(delta-server PROPERTIES
        LINK_FLAGS "-Wl,-no_warn_duplicate_libraries -lc++ -lc++abi"
    )
endif()

# Platform-specific libraries
if(WIN32)
    target_link_libraries(delta PRIVATE shell32)
    target_link_libraries(delta PRIVATE rpcrt4)
elseif(UNIX AND NOT APPLE)
    if(UUID_LIBRARY)
        target_link_libraries(delta PRIVATE ${UUID_LIBRARY})
    endif()
    # Link dynamic loader for Telemetry Engine / general UNIX dl support
    target_link_libraries(delta PRIVATE dl)
    target_link_libraries(delta-server PRIVATE dl)
endif()

# Compiler flags
if(MSVC)
    target_compile_options(delta PRIVATE /W4 /WX-)
    target_compile_options(delta-server PRIVATE /W4 /WX-)
else()
    target_compile_options(delta PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(delta-server PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Harness tests: a standalone binary (no Catch2 dependency) that runs the agent loop against a
# scripted stand-in for llama-server. Build and run with:
#   cmake --build build --target delta-harness-tests && ./build/delta-harness-tests
add_executable(delta-harness-tests EXCLUDE_FROM_ALL
    tests/test_harness.cpp
    engine/agent/agent_database.cpp
    engine/agent/memory_store.cpp
    engine/agent/tool_registry.cpp
    engine/agent/tool_calendar.cpp
    engine/agent/tool_notes.cpp
    engine/agent/tool_memory.cpp
    engine/agent/tool_task.cpp
    engine/agent/tool_files.cpp
    engine/agent/tool_shell.cpp
    engine/agent/tool_web.cpp
    engine/agent/llm_client.cpp
    engine/agent/context_manager.cpp
    engine/agent/policy.cpp
    engine/agent/harness.cpp
)
target_include_directories(delta-harness-tests PRIVATE ${DELTA_INCLUDE_DIRS})
target_link_libraries(delta-harness-tests PRIVATE
    cpp-httplib
    sqlite3
    ${CMAKE_THREAD_LIBS_INIT}
)
if(USE_CURL AND CURL_FOUND)
    target_link_libraries(delta-harness-tests PRIVATE CURL::libcurl)
endif()
if(WIN32)
    target_compile_definitions(delta-harness-tests PRIVATE NOMINMAX)
    target_link_libraries(delta-harness-tests PRIVATE ws2_32)
endif()
if(APPLE)
    target_link_libraries(delta-harness-tests PRIVATE c++)
endif()

# Tests
if(BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if(Catch2_FOUND)
        add_subdirectory(tests)
        message(STATUS "Tests enabled")
    else()
        message(STATUS "Catch2 not found - tests disabled")
    endif()
endif()

# Build Web UI from web/app/ directory
option(FORCE_BUILD_WEBUI "Always run pnpm run build for web UI (ignores timestamp)" OFF)
message(STATUS "")
message(STATUS "Building Web UI from web/app/...")

find_program(PNPM_EXECUTABLE pnpm)
find_program(NPM_EXECUTABLE npm)
if(PNPM_EXECUTABLE)
    set(PKG_MANAGER ${PNPM_EXECUTABLE})
    message(STATUS "  pnpm found: ${PNPM_EXECUTABLE}")
elseif(NPM_EXECUTABLE)
    set(PKG_MANAGER ${NPM_EXECUTABLE})
    message(STATUS "  npm found: ${NPM_EXECUTABLE} (pnpm preferred)")
endif()

if(PKG_MANAGER)
    if(EXISTS "${CMAKE_SOURCE_DIR}/web/app/package.json")
        message(STATUS "  app/ directory found")

        set(PUBLIC_INDEX "${CMAKE_SOURCE_DIR}/public/index.html")
        set(APP_PACKAGE_JSON "${CMAKE_SOURCE_DIR}/web/app/package.json")

        set(SHOULD_BUILD_WEBUI TRUE)
        if(FORCE_BUILD_WEBUI)
            message(STATUS "  FORCE_BUILD_WEBUI=ON, rebuilding web UI")
        elseif(EXISTS "${PUBLIC_INDEX}")
            file(TIMESTAMP "${PUBLIC_INDEX}" INDEX_TIME)
            file(TIMESTAMP "${APP_PACKAGE_JSON}" PACKAGE_TIME)
            if(INDEX_TIME GREATER_EQUAL PACKAGE_TIME)
                message(STATUS "  public/index.html is up to date, skipping build")
                set(SHOULD_BUILD_WEBUI FALSE)
            endif()
        endif()

        if(SHOULD_BUILD_WEBUI)
            message(STATUS "  Building web UI (this may take a minute)...")

            if(NOT EXISTS "${CMAKE_SOURCE_DIR}/web/app/node_modules")
                message(STATUS "    Installing dependencies...")
                execute_process(
                    COMMAND ${PKG_MANAGER} install
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/web/app"
                    OUTPUT_QUIET
                    ERROR_QUIET
                    RESULT_VARIABLE PKG_INSTALL_RESULT
                )
                if(NOT PKG_INSTALL_RESULT EQUAL 0)
                    message(WARNING "  Install failed, web UI may not be available")
                endif()
            endif()

            message(STATUS "    Building SvelteKit app...")
            execute_process(
                COMMAND ${PKG_MANAGER} run build
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/web/app"
                OUTPUT_QUIET
                ERROR_QUIET
                RESULT_VARIABLE PKG_BUILD_RESULT
            )
            if(PKG_BUILD_RESULT EQUAL 0)
                message(STATUS "  Web UI built successfully")
            else()
                message(WARNING "  Build failed, web UI may not be available")
            endif()
        endif()
    else()
        message(WARNING "  app/package.json not found, skipping web UI build")
    endif()
else()
    message(WARNING "  Neither pnpm nor npm found, web UI will not be built")
    message(WARNING "  Install pnpm (https://pnpm.io) to build the web UI")
endif()

# Installation
install(TARGETS delta delta-server
    RUNTIME DESTINATION bin
)

# Install llama.cpp HTTP server binary
if(TARGET server)
  install(TARGETS server RUNTIME DESTINATION bin)
  message(STATUS "  Will install llama.cpp 'server' to bin")
elseif(TARGET llama-server)
  install(TARGETS llama-server RUNTIME DESTINATION bin)
  message(STATUS "  Will install llama.cpp 'llama-server' to bin")
else()
  message(STATUS "  Note: llama.cpp server target not found; ensure LLAMA_BUILD_SERVER=ON and engine/vendor/llama.cpp is present")
endif()

# Install models directory structure
install(DIRECTORY DESTINATION share/delta-cli/models)

# Install Web UI from public/ directory (built from app/)
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
message(STATUS "  CCache: ${GGML_CCACHE}")
message(STATUS "  OpenMP: ${GGML_OPENMP}")
message(STATUS "")