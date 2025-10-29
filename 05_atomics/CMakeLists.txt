cmake_minimum_required(VERSION 3.20)
project(AtomicsDeepDive LANGUAGES CXX)

# C++20 required for better atomic features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific compiler flags
if(MSVC)
    # MSVC flags
    add_compile_options(
        /W4                 # Warning level 4
        /WX                 # Warnings as errors
        /permissive-        # Standards conformance
        /Zc:__cplusplus     # Enable correct __cplusplus macro
    )
    
    # Release optimizations
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/GL>)  # Whole program optimization
    
else()
    # GCC/Clang flags
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -pedantic
        -pthread            # Required for atomics and threading
    )
    
    # Release optimizations
    add_compile_options($<$<CONFIG:Release>:-O3>)
    add_compile_options($<$<CONFIG:Release>:-march=native>)
    
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ============================================================================
# Executable: Atomic Demo
# ============================================================================

add_executable(atomic_demo
    src/atomic_demo.cpp
)

# Link threading library
if(NOT MSVC)
    target_link_libraries(atomic_demo pthread)
endif()

# ============================================================================
# Executable: Atomic Benchmark
# ============================================================================

add_executable(atomic_benchmark
    src/atomic_benchmark.cpp
)

# Link threading library
if(NOT MSVC)
    target_link_libraries(atomic_benchmark pthread)
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS atomic_demo atomic_benchmark
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/atomics
    FILES_MATCHING PATTERN "*.hpp"
)

install(DIRECTORY docs/
    DESTINATION share/doc/atomics
    FILES_MATCHING PATTERN "*.md"
)

# ============================================================================
# Print Configuration
# ============================================================================

message(STATUS "")
message(STATUS "===========================================")
message(STATUS "  Atomics Deep Dive Configuration")
message(STATUS "===========================================")
message(STATUS "C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler:      ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  - atomic_demo       : Interactive demonstrations")
message(STATUS "  - atomic_benchmark  : Performance benchmarks")
message(STATUS "===========================================")
message(STATUS "")

