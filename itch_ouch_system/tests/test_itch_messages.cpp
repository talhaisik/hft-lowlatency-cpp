// tests/test_itch_messages.cpp
//
// Example usage and unit tests for ITCH message parsing
// Demonstrates how to create, parse, and inspect ITCH messages

#include "itch/messages.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cassert>

using namespace hft;
using namespace hft::itch;

// ============================================================================
// HELPER: Create test message buffer
// ============================================================================

/// Helper to build an AddOrder message buffer (for testing)
std::vector<uint8_t> create_add_order_message() {
    // AddOrder message structure (36 bytes total):
    // Offset 0: 'A' (message type) - 1 byte
    // Offset 1-2: stock_locate (big-endian uint16) - 2 bytes
    // Offset 3-4: tracking_number (big-endian uint16) - 2 bytes
    // Offset 5-12: timestamp (big-endian uint64) - 8 bytes
    // Offset 11-18: order_reference (big-endian uint64) - 8 bytes
    // Offset 19: buy_sell_indicator ('B' or 'S') - 1 byte
    // Offset 20-23: shares (big-endian uint32) - 4 bytes
    // Offset 24-31: symbol (8 bytes, ASCII) - 8 bytes
    // Offset 32-35: price (big-endian uint32) - 4 bytes
    // Total: 36 bytes

    std::vector<uint8_t> buffer(36, 0);

    buffer[0] = 'A';  // Message type

    // Stock locate (1) - offset 1-2
    buffer[1] = 0x00;
    buffer[2] = 0x01;

    // Tracking number (100) - offset 3-4
    buffer[3] = 0x00;
    buffer[4] = 0x64;

    // Timestamp (34200000000000) - offset 5-12
    uint64_t timestamp = 34200000000000ULL;
    buffer[5] = (timestamp >> 56) & 0xFF;
    buffer[6] = (timestamp >> 48) & 0xFF;
    buffer[7] = (timestamp >> 40) & 0xFF;
    buffer[8] = (timestamp >> 32) & 0xFF;
    buffer[9] = (timestamp >> 24) & 0xFF;
    buffer[10] = (timestamp >> 16) & 0xFF;
    buffer[11] = (timestamp >> 8) & 0xFF;
    buffer[12] = timestamp & 0xFF;

    // Order reference (123456789) - offset 11-18
    uint64_t order_ref = 123456789ULL;
    buffer[11] = (order_ref >> 56) & 0xFF;
    buffer[12] = (order_ref >> 48) & 0xFF;
    buffer[13] = (order_ref >> 40) & 0xFF;
    buffer[14] = (order_ref >> 32) & 0xFF;
    buffer[15] = (order_ref >> 24) & 0xFF;
    buffer[16] = (order_ref >> 16) & 0xFF;
    buffer[17] = (order_ref >> 8) & 0xFF;
    buffer[18] = order_ref & 0xFF;

    // Buy/Sell indicator - offset 19
    buffer[19] = 'B';

    // Shares (100) - offset 20-23
    uint32_t shares = 100;
    buffer[20] = (shares >> 24) & 0xFF;
    buffer[21] = (shares >> 16) & 0xFF;
    buffer[22] = (shares >> 8) & 0xFF;
    buffer[23] = shares & 0xFF;

    // Symbol "AAPL    " - offset 24-31
    buffer[24] = 'A';
    buffer[25] = 'A';
    buffer[26] = 'P';
    buffer[27] = 'L';
    buffer[28] = ' ';
    buffer[29] = ' ';
    buffer[30] = ' ';
    buffer[31] = ' ';

    // Price ($150.25 = 1502500) - offset 32-35
    uint32_t price = 1502500;
    buffer[32] = (price >> 24) & 0xFF;
    buffer[33] = (price >> 16) & 0xFF;
    buffer[34] = (price >> 8) & 0xFF;
    buffer[35] = price & 0xFF;

    return buffer;
}

/// Helper to create a SystemEvent message  
std::vector<uint8_t> create_system_event_message(char event_code) {
    // SystemEvent message structure (12 bytes total):
    // Offset 0: 'S' (message type) - 1 byte
    // Offset 1-2: stock_locate (uint16) - 2 bytes
    // Offset 3-4: tracking_number (uint16) - 2 bytes
    // Offset 5-10: timestamp (uint64, only 6 bytes used) - 6 bytes
    // Offset 11: event_code - 1 byte
    // Total: 12 bytes

    std::vector<uint8_t> buffer(12, 0);

    buffer[0] = 'S';  // Message type

    // Stock locate (0)
    buffer[1] = 0x00;
    buffer[2] = 0x00;

    // Tracking number (0)
    buffer[3] = 0x00;
    buffer[4] = 0x00;

    // Timestamp (34200000000000) - 6 bytes only (48-bit timestamp)
    uint64_t timestamp = 34200000000000ULL;
    buffer[5] = (timestamp >> 40) & 0xFF;
    buffer[6] = (timestamp >> 32) & 0xFF;
    buffer[7] = (timestamp >> 24) & 0xFF;
    buffer[8] = (timestamp >> 16) & 0xFF;
    buffer[9] = (timestamp >> 8) & 0xFF;
    buffer[10] = timestamp & 0xFF;

    // Event code
    buffer[11] = event_code;

    return buffer;
}

// ============================================================================
// TEST FUNCTIONS
// ============================================================================

void test_add_order_parsing() {
    std::cout << "\n=== Test: AddOrder Parsing ===\n";

    auto buffer = create_add_order_message();
    auto result = parse_message(buffer.data(), buffer.size());

    assert(result.is_success());
    assert(std::holds_alternative<AddOrder>(*result.message));

    const auto& msg = std::get<AddOrder>(*result.message);

    std::cout << "[OK] Message type: " << message_type_to_string(AddOrder::TYPE) << "\n";
    std::cout << "[OK] Stock locate: " << msg.stock_locate << "\n";
    std::cout << "[OK] Order reference: " << msg.order_reference << "\n";
    std::cout << "[OK] Symbol: '" << msg.get_symbol() << "'\n";
    std::cout << "[OK] Side: " << side_to_string(msg.side()) << "\n";
    std::cout << "[OK] Shares: " << msg.shares << "\n";
    std::cout << "[OK] Price: " << format_price(msg.get_price()) << "\n";

    // Validate values
    assert(msg.stock_locate == 1);
    assert(msg.order_reference == 123456789);
    assert(msg.get_symbol() == "AAPL");
    assert(msg.side() == Side::BUY);
    assert(msg.shares == 100);
    assert(msg.get_price() == 1502500);

    std::cout << "[OK] All assertions passed!\n";
}

void test_system_event_parsing() {
    std::cout << "\n=== Test: SystemEvent Parsing ===\n";

    auto buffer = create_system_event_message('Q');  // Market open
    auto result = parse_message(buffer.data(), buffer.size());

    assert(result.is_success());
    assert(std::holds_alternative<SystemEvent>(*result.message));

    const auto& msg = std::get<SystemEvent>(*result.message);

    std::cout << "[OK] Message type: " << message_type_to_string(SystemEvent::TYPE) << "\n";
    std::cout << "[OK] Event code: " << msg.event_code << "\n";
    std::cout << "[OK] Market status: ";

    switch (msg.to_market_status()) {
    case MarketStatus::OPEN:
        std::cout << "OPEN\n";
        break;
    case MarketStatus::CLOSED:
        std::cout << "CLOSED\n";
        break;
    default:
        std::cout << "OTHER\n";
    }

    assert(msg.event_code == 'Q');
    assert(msg.to_market_status() == MarketStatus::OPEN);

    std::cout << "[OK] All assertions passed!\n";
}

void test_error_handling() {
    std::cout << "\n=== Test: Error Handling ===\n";

    // Test 1: Empty buffer
    {
        std::vector<uint8_t> empty_buffer;
        auto result = parse_message(empty_buffer.data(), 0);

        assert(!result.is_success());
        assert(result.error_code == ErrorCode::PARSE_INVALID_SIZE);
        std::cout << "[OK] Empty buffer rejected: " << result.error_detail << "\n";
    }

    // Test 2: Invalid message type
    {
        std::vector<uint8_t> bad_buffer = { 0xFF, 0x00, 0x00 };  // Invalid type
        auto result = parse_message(bad_buffer.data(), bad_buffer.size());

        assert(!result.is_success());
        assert(result.error_code == ErrorCode::PARSE_INVALID_TYPE);
        std::cout << "[OK] Invalid message type rejected: " << result.error_detail << "\n";
    }

    // Test 3: Truncated message
    {
        std::vector<uint8_t> truncated = { 'A', 0x00, 0x01 };  // AddOrder needs 36 bytes
        auto result = parse_message(truncated.data(), truncated.size());

        assert(!result.is_success());
        std::cout << "[OK] Truncated message rejected\n";
    }

    std::cout << "[OK] All error handling tests passed!\n";
}

void test_message_helpers() {
    std::cout << "\n=== Test: Message Helper Functions ===\n";

    auto buffer = create_add_order_message();
    auto result = parse_message(buffer.data(), buffer.size());

    assert(result.is_success());

    // Test visitor helpers
    uint64_t timestamp = get_timestamp(*result.message);
    uint16_t stock_locate = get_stock_locate(*result.message);
    const char* type_name = get_message_type_name(*result.message);

    std::cout << "[OK] Timestamp: " << timestamp << "\n";
    std::cout << "[OK] Stock locate: " << stock_locate << "\n";
    std::cout << "[OK] Type name: " << type_name << "\n";

    // Test message classification
    assert(is_order_book_message(*result.message));
    assert(!is_trade_message(*result.message));
    assert(!is_system_message(*result.message));

    std::cout << "[OK] Message classification correct\n";
    std::cout << "[OK] All helper function tests passed!\n";
}

void test_message_stats() {
    std::cout << "\n=== Test: Message Statistics ===\n";

    MessageStats stats;

    // Parse several messages
    auto add_order = create_add_order_message();
    auto sys_event = create_system_event_message('Q');

    for (int i = 0; i < 10; i++) {
        auto result1 = parse_message(add_order.data(), add_order.size());
        if (result1.is_success()) {
            stats.record_message(*result1.message);
        }

        auto result2 = parse_message(sys_event.data(), sys_event.size());
        if (result2.is_success()) {
            stats.record_message(*result2.message);
        }
    }

    stats.print_summary();

    assert(stats.total_messages == 20);
    assert(stats.add_orders == 10);
    assert(stats.system_events == 10);

    std::cout << "[OK] Statistics tracking works correctly!\n";
}

void test_price_utilities() {
    std::cout << "\n=== Test: Price Utilities ===\n";

    // Test price conversion
    Price price = to_price(150.25);
    std::cout << "[OK] $150.25 = " << price << " (internal)\n";

    double dollars = to_dollars(price);
    std::cout << "[OK] " << price << " (internal) = $" << dollars << "\n";

    assert(price == 1502500);
    assert(std::abs(dollars - 150.25) < 0.0001);

    // Test formatting
    std::string formatted = format_price(price);
    std::cout << "[OK] Formatted: " << formatted << "\n";

    std::cout << "[OK] All price utility tests passed!\n";
}

void demo_real_world_usage() {
    std::cout << "\n=== Demo: Real-World Usage Pattern ===\n";

    // Simulate receiving messages from network
    std::vector<std::vector<uint8_t>> message_stream = {
        create_system_event_message('Q'),  // Market open
        create_add_order_message(),         // New order
        create_add_order_message(),         // Another order
        create_system_event_message('M'),  // Market close
    };

    MessageStats stats;

    std::cout << "\nProcessing message stream...\n\n";

    for (const auto& buffer : message_stream) {
        auto result = parse_message(buffer.data(), buffer.size());

        if (!result.is_success()) {
            std::cerr << "[ERROR] Parse error: " << result.error_detail << "\n";
            stats.record_error();
            continue;
        }

        stats.record_message(*result.message);

        // Handle message using std::visit
        std::visit([](const auto& msg) {
            using T = std::remove_cvref_t<decltype(msg)>;

            if constexpr (std::is_same_v<T, SystemEvent>) {
                std::cout << "[SYSTEM] System Event: code=" << msg.event_code << "\n";
            }
            else if constexpr (std::is_same_v<T, AddOrder>) {
                std::cout << "[ORDER] Add Order: " << msg.get_symbol()
                    << " " << side_to_string(msg.side())
                    << " " << msg.shares << "@" << format_price(msg.get_price()) << "\n";
            }
            else {
                std::cout << "[MSG] Message: " << message_type_to_string(T::TYPE) << "\n";
            }
            }, *result.message);
    }

    std::cout << "\n";
    stats.print_summary();
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "====================================\n";
    std::cout << "ITCH 5.0 Message Parser Test Suite\n";
    std::cout << "====================================\n";

    try {
        test_add_order_parsing();
        test_system_event_parsing();
        test_error_handling();
        test_message_helpers();
        test_message_stats();
        test_price_utilities();
        demo_real_world_usage();

        std::cout << "\n";
        std::cout << "====================================\n";
        std::cout << "[PASS] ALL TESTS PASSED!\n";
        std::cout << "====================================\n";

    }
    catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Test failed with exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}