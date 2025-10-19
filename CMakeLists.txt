# Top-level CMakeLists.txt for HFT Low-Latency C++ Project
cmake_minimum_required(VERSION 3.20)

project(hft_lowlatency_cpp 
    VERSION 0.1.0
    DESCRIPTION "High-Frequency Trading Low-Latency C++ Project"
    LANGUAGES CXX
)

# Global settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add subdirectories
add_subdirectory(01_modern_cpp_basics)
add_subdirectory(02_ring_buffer)