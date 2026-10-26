option(DELTA_PORTABLE_RELEASE "Build redistributable binaries for the platform baseline" OFF)
# x86_64 ships two llama-servers: the baseline runs anywhere, x86-64-v3 (AVX2, FMA, F16C, BMI2;
# Intel Haswell / AMD Excavator, 2013 on) is several times faster and is picked at launch when supported.
set(DELTA_CPU_LEVEL "baseline" CACHE STRING "x86_64 release CPU level: baseline or x86-64-v3")

# Windows releases carry their own C++ runtime: an older system msvcp140.dll crashes binaries built
# with a newer MSVC (0xC0000005 on the first mutex lock) before they print anything.
if(DELTA_PORTABLE_RELEASE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

# Set this before project() so compiler probes use the same deployment contract.
if(DELTA_PORTABLE_RELEASE AND APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS" FORCE)
    set(GGML_METAL_MACOSX_VERSION_MIN "11.0" CACHE STRING "Minimum Metal runtime" FORCE)
endif()

function(delta_configure_release_cpu)
    if(NOT DELTA_PORTABLE_RELEASE)
        return()
    endif()
    set(GGML_NATIVE OFF CACHE BOOL "Never target the release runner CPU" FORCE)
    if(CMAKE_OSX_ARCHITECTURES)
        set(arch "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(arch "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    if(arch MATCHES "^(x86_64|AMD64|amd64)$")
        foreach(feature SSE42 AVX AVX2 AVX_VNNI AVX_VNNI_INT8 AVX512 AVX512_VBMI AVX512_VNNI AVX512_BF16 FMA F16C BMI2 AMX_TILE AMX_INT8 AMX_BF16)
            set(GGML_${feature} OFF CACHE BOOL "Optional CPU feature disabled in portable releases" FORCE)
        endforeach()
        set(policy "x86-64-baseline")
        if(DELTA_CPU_LEVEL STREQUAL "x86-64-v3")
            foreach(feature SSE42 AVX AVX2 FMA F16C BMI2)
                set(GGML_${feature} ON CACHE BOOL "x86-64-v3 CPU feature" FORCE)
            endforeach()
            set(policy "x86-64-v3")
        elseif(NOT DELTA_CPU_LEVEL STREQUAL "baseline")
            message(FATAL_ERROR "DELTA_CPU_LEVEL must be baseline or x86-64-v3, not ${DELTA_CPU_LEVEL}")
        endif()
    else()
        set(policy "${arch}-baseline")
    endif()
    execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE commit OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    file(WRITE "${CMAKE_BINARY_DIR}/release-build-info.json"
        "{\"commit\":\"${commit}\",\"tag\":\"$ENV{RELEASE_TAG}\",\"cpu_policy\":\"${policy}\",\"macos_minimum\":\"${CMAKE_OSX_DEPLOYMENT_TARGET}\"}\n")
    message(STATUS "Delta portable release CPU policy: ${policy}")
endfunction()
