# tests/CMakeLists.txt
# Test configuration for ITCH/OUCH Trading System

# =============================================================================
# TEST CONFIGURATION
# =============================================================================

# Determine if we're using Google Test or simple asserts
if(GTest_FOUND)
    set(TEST_LIBS GTest::GTest GTest::Main)
    add_compile_definitions(USE_GTEST)
    message(STATUS "Using Google Test framework for tests")
else()
    set(TEST_LIBS "")
    message(STATUS "Using simple assert-based tests (Google Test not found)")
endif()

# =============================================================================
# TEST HELPER FUNCTION
# =============================================================================

function(add_hft_test TEST_NAME)
    # Create test executable
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
    
    # Link libraries
    target_link_libraries(${TEST_NAME} PRIVATE hft_core ${TEST_LIBS})
    
    # Add to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    
    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 30
        LABELS "unit"
    )
    
    # Enable verbose output on failure
    set_tests_properties(${TEST_NAME} PROPERTIES
        ENVIRONMENT "CTEST_OUTPUT_ON_FAILURE=1"
    )
endfunction()

# =============================================================================
# UNIT TESTS
# =============================================================================

# ITCH Message Parsing (COMPLETED)
add_hft_test(test_itch_messages)

# ITCH Comprehensive Tests (thorough field validation)
add_hft_test(test_itch_messages_comprehensive)

add_hft_test(test_timestamp_debug)

# Seqlock Concurrency Test
add_hft_test(test_seqlock)

# Order Book
add_hft_test(test_order_book)

# MoldUDP64 (TODO - Phase 2)
# add_hft_test(test_mold_udp64)

# OUCH Builder (TODO - Phase 3)
# add_hft_test(test_ouch_builder)

# Risk Manager (TODO - Phase 3)
# add_hft_test(test_risk_manager)

# Strategy (TODO - Phase 4)
# add_hft_test(test_market_maker)

# =============================================================================
# INTEGRATION TESTS (TODO - Phase 4)
# =============================================================================

# Full end-to-end integration test
# add_executable(test_integration test_integration.cpp)
# target_link_libraries(test_integration PRIVATE hft_core ${TEST_LIBS})
# add_test(NAME test_integration COMMAND test_integration)
# set_tests_properties(test_integration PROPERTIES
#     TIMEOUT 120
#     LABELS "integration"
# )

# =============================================================================
# CUSTOM TEST TARGETS
# =============================================================================

# Run only fast unit tests
add_custom_target(test_fast
    COMMAND ${CMAKE_CTEST_COMMAND} -L unit --output-on-failure
    DEPENDS test_itch_messages
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running fast unit tests..."
)

# Run all tests with verbose output
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS test_itch_messages
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all tests (verbose)..."
)

# =============================================================================
# TEST DATA
# =============================================================================

# Copy test data files to build directory if they exist
file(GLOB TEST_DATA_FILES "${CMAKE_SOURCE_DIR}/data/*.bin")
if(TEST_DATA_FILES)
    file(COPY ${TEST_DATA_FILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)
    message(STATUS "Copied test data files to build directory")
endif()

# =============================================================================
# SUMMARY
# =============================================================================

message(STATUS "")
message(STATUS "=== Tests Configuration ===")
message(STATUS "Active tests: test_itch_messages")
message(STATUS "Test framework: ${GTest_FOUND}")
message(STATUS "===========================")
message(STATUS "")