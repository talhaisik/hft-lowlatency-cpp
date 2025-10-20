cmake_minimum_required(VERSION 3.20)

# Smart Pointers Module
project(SmartPointers LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
    # Allow specific warnings
    add_compile_options(/wd4324)  # Padding due to alignment
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

# Minimal test executable
add_executable(minimal_test
    src/minimal_test.cpp
)

# Combined examples executable
add_executable(smart_ptr_demo
    src/smart_ptr_demo.cpp
)

# Set output directory
set_target_properties(minimal_test smart_ptr_demo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(minimal_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Display build configuration
message(STATUS "Smart Pointers Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")