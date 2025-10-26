# tools/CMakeLists.txt
#
# Build configuration for tools and utilities

# =============================================================================
# REPLAY FEED TOOL
# =============================================================================

add_executable(replay_feed
    replay_feed.cpp
)

target_link_libraries(replay_feed
    PRIVATE
        hft_core
        hft_headers
        Threads::Threads
)

target_include_directories(replay_feed
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# Set output directory
set_target_properties(replay_feed
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)

message(STATUS "Added tool: replay_feed")
