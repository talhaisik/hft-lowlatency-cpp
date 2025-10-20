cmake_minimum_required(VERSION 3.20)

project(HFT_LowLatency_CPP 
    VERSION 1.0.0
    DESCRIPTION "Modern C++ exploration for HFT and low-latency systems"
    LANGUAGES CXX
)

# Global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global compiler options
if(MSVC)
    add_compile_options(/W4)
    # Allow specific warnings that are intentional
    add_compile_options(/wd4324)  # Padding due to alignment
    add_compile_options(/wd4834)  # Discarding [[nodiscard]]
    add_compile_options(/wd4267)  # size_t to int conversion in tests
    # Optimization for Release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /Oi /Ot /GL)
        add_link_options(/LTCG)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Optimization for Release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native -flto)
        add_link_options(-flto)
    endif()
endif()

# Build configuration display
message(STATUS "===========================================")
message(STATUS "HFT Low-Latency C++ Project Configuration")
message(STATUS "===========================================")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "===========================================")

# Add subdirectories for each module
add_subdirectory(01_modern_cpp_basics)
add_subdirectory(02_ring_buffer)
add_subdirectory(03_memory_pool)
add_subdirectory(04_smart_pointers)

# Custom targets for convenience
add_custom_target(run_all_examples
    COMMENT "Running all examples"
)

add_custom_target(run_all_benchmarks
    COMMENT "Running all benchmarks"
)

message(STATUS "Modules configured:")
message(STATUS "  - 01_modern_cpp_basics")
message(STATUS "  - 02_ring_buffer")
message(STATUS "  - 03_memory_pool")
message(STATUS "===========================================")