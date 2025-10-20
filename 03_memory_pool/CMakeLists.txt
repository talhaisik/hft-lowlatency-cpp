cmake_minimum_required(VERSION 3.20)

# Memory Pool Module
project(MemoryPool LANGUAGES CXX)

# C++20 required for concepts and other modern features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    # Enable most warnings, but allow specific ones
    add_compile_options(/W4)
    # Allow alignment padding warnings (C4324) - this is intentional for performance
    add_compile_options(/wd4324)
    # Allow discarding [[nodiscard]] return values in benchmarks (C4834)
    add_compile_options(/wd4834)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Optimization flags for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /Oi /Ot /GL)
        add_link_options(/LTCG)
    else()
        add_compile_options(-O3 -march=native -flto)
        add_link_options(-flto)
    endif()
endif()

# Include directory for headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Example executable
add_executable(memory_pool_example
    src/memory_pool_example.cpp
)

# Benchmark executable
add_executable(memory_pool_benchmark
    src/memory_pool_benchmark.cpp
)

# Set output directory
set_target_properties(memory_pool_example memory_pool_benchmark
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    target_compile_definitions(memory_pool_example PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(memory_pool_benchmark PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Display build configuration
message(STATUS "Memory Pool Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")