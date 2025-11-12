cmake_minimum_required(VERSION 3.14)

# Find Catch2
find_package(Catch2 3 REQUIRED)

# Test sources
set(TEST_SOURCES
    test_main.cpp
    test_ui.cpp
    test_auth.cpp
    test_models.cpp
    test_inference.cpp
    test_tools.cpp
    test_default_flow.cpp
    test_interactive_commands.cpp
)

# Create test executable
add_executable(delta_tests ${TEST_SOURCES})

# Include directories
target_include_directories(delta_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp/include
    ${CMAKE_SOURCE_DIR}/vendor/llama.cpp/common
)

# Link libraries
target_link_libraries(delta_tests PRIVATE
    Catch2::Catch2WithMain
    llama
    common
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(delta_tests PRIVATE rpcrt4)
elseif(UNIX AND NOT APPLE)
    if(UUID_LIBRARY)
        target_link_libraries(delta_tests PRIVATE ${UUID_LIBRARY})
    endif()
endif()

# Link curl if available
if(USE_CURL AND CURL_FOUND)
    target_link_libraries(delta_tests PRIVATE CURL::libcurl)
endif()

# Discover tests
include(CTest)
include(Catch)
catch_discover_tests(delta_tests)

# Add test data directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data 
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

