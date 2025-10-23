cmake_minimum_required(VERSION 3.20)
project(itch-ouch-trading 
    VERSION 1.0.0
    DESCRIPTION "Low-Latency ITCH/OUCH Trading System"
    LANGUAGES CXX
)

# =============================================================================
# BUILD CONFIGURATION
# =============================================================================

# C++20 Required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# =============================================================================
# COMPILER FLAGS
# =============================================================================

# Platform-specific compiler flags
if(MSVC)
    # MSVC (Windows)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /permissive-")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    
    # Disable specific warnings
    add_compile_options(/wd4267 /wd4244)  # size_t conversions
    
    message(STATUS "Using MSVC compiler flags")
else()
    # GCC/Clang (Linux/macOS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
    
    # Enable sanitizers in debug mode (GCC/Clang only)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        option(ENABLE_ASAN "Enable Address Sanitizer" ON)
        option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" ON)
        
        if(ENABLE_ASAN)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
            message(STATUS "Address Sanitizer enabled")
        endif()
        
        if(ENABLE_UBSAN)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined")
            message(STATUS "Undefined Behavior Sanitizer enabled")
        endif()
    endif()
    
    message(STATUS "Using GCC/Clang compiler flags")
endif()

# =============================================================================
# BUILD OPTIONS
# =============================================================================

option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)
option(BUILD_TOOLS "Build tools and simulators" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(ENABLE_LTO "Enable Link Time Optimization" OFF)

if(ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    message(STATUS "Link Time Optimization enabled")
endif()

# =============================================================================
# DEPENDENCIES
# =============================================================================

# Threads (required)
find_package(Threads REQUIRED)

# Google Test (optional, for tests)
if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        message(STATUS "Google Test found - using GTest framework")
    else()
        message(STATUS "Google Test not found - using simple assert-based tests")
    endif()
endif()

# Google Benchmark (optional, for benchmarks)
if(BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    if(benchmark_FOUND)
        message(STATUS "Google Benchmark found")
    else()
        message(STATUS "Google Benchmark not found - skipping benchmarks")
        set(BUILD_BENCHMARKS OFF)
    endif()
endif()

# =============================================================================
# INCLUDE DIRECTORIES
# =============================================================================

include_directories(${CMAKE_SOURCE_DIR}/include)

# =============================================================================
# HEADER-ONLY LIBRARY
# =============================================================================

# Interface library for header-only components
# (types.hpp, messages.hpp are currently header-only)
add_library(hft_headers INTERFACE)
target_include_directories(hft_headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(hft_headers INTERFACE cxx_std_20)

# =============================================================================
# CORE LIBRARY
# =============================================================================

# Placeholder for future implementation files
# As we add .cpp files, they'll be added here
set(HFT_CORE_SOURCES
    # ITCH
    # src/itch/parser.cpp              # TODO: Phase 2
    
    # Order Book
    # src/book/order_book.cpp          # TODO: Next step
    
    # Network
    # src/network/mold_udp64.cpp       # TODO: Phase 2
    # src/network/soupbintcp.cpp       # TODO: Phase 3
    
    # OUCH
    # src/ouch/builder.cpp             # TODO: Phase 3
    
    # Risk
    # src/risk/risk_manager.cpp        # TODO: Phase 3
    
    # Strategy
    # src/strategy/market_maker.cpp    # TODO: Phase 4
)

# Only create core library if we have source files
if(HFT_CORE_SOURCES)
    add_library(hft_core STATIC ${HFT_CORE_SOURCES})
    target_link_libraries(hft_core PUBLIC hft_headers)
    target_link_libraries(hft_core PRIVATE Threads::Threads)
else()
    # For now, just use header-only library
    add_library(hft_core ALIAS hft_headers)
    message(STATUS "No implementation files yet - using header-only library")
endif()

# =============================================================================
# SUBDIRECTORIES
# =============================================================================

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    message(STATUS "Building tests")
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
    message(STATUS "Building benchmarks")
endif()

# Tools
if(BUILD_TOOLS)
    add_subdirectory(tools)
    message(STATUS "Building tools")
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
    message(STATUS "Building examples")
endif()

# =============================================================================
# INSTALLATION
# =============================================================================

install(TARGETS hft_headers
    EXPORT hft-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Install config files
install(DIRECTORY config/
    DESTINATION etc/hft
    FILES_MATCHING PATTERN "*.yaml"
)

# =============================================================================
# SUMMARY
# =============================================================================

message(STATUS "")
message(STATUS "=== ITCH/OUCH Trading System Configuration ===")
message(STATUS "Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build tests:      ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "Build tools:      ${BUILD_TOOLS}")
message(STATUS "Build examples:   ${BUILD_EXAMPLES}")
message(STATUS "LTO enabled:      ${ENABLE_LTO}")
message(STATUS "============================================")
message(STATUS "")