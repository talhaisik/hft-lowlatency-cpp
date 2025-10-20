# CMakeLists.txt for Ring Buffer Project
cmake_minimum_required(VERSION 3.20)

# Project definition
project(hft_ring_buffer 
    VERSION 0.1.0
    DESCRIPTION "High-Frequency Trading Ring Buffer Benchmark"
    LANGUAGES CXX
)

# Ensure C++20 support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
    # Allow alignment padding warnings - intentional for performance
    add_compile_options(/wd4324)
    # Allow discarding [[nodiscard]] in benchmarks
    add_compile_options(/wd4834)
    # Allow size_t to int conversions in benchmarks
    add_compile_options(/wd4267)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Benchmark executable
add_executable(ring_buffer_benchmark 
    src/ring_buffer_benchmark.cpp
)

# Include directories
target_include_directories(ring_buffer_benchmark 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Enable testing
enable_testing()
add_test(NAME RingBufferBenchmark COMMAND ring_buffer_benchmark)

# Optional: Set output directory
set_target_properties(ring_buffer_benchmark
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)