// tests/test_itch_messages_comprehensive.cpp
//
// Comprehensive test suite for ITCH message parsing
// Tests all edge cases, field boundaries, and timestamp handling

#include "itch/messages.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cassert>
#include <cstring>

using namespace hft;
using namespace hft::itch;

// =============================================================================
// TEST UTILITIES
// =============================================================================

/// Helper to print buffer in hex for debugging
void print_hex(const std::vector<uint8_t>& buffer) {
    for (size_t i = 0; i < buffer.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(buffer[i]) << " ";
        if ((i + 1) % 16 == 0) std::cout << "\n";
    }
    std::cout << std::dec << "\n";
}

/// Write uint16 in big-endian
void write_be16(std::vector<uint8_t>& buf, size_t offset, uint16_t value) {
    buf[offset] = (value >> 8) & 0xFF;
    buf[offset + 1] = value & 0xFF;
}

/// Write uint32 in big-endian
void write_be32(std::vector<uint8_t>& buf, size_t offset, uint32_t value) {
    buf[offset] = (value >> 24) & 0xFF;
    buf[offset + 1] = (value >> 16) & 0xFF;
    buf[offset + 2] = (value >> 8) & 0xFF;
    buf[offset + 3] = value & 0xFF;
}

/// Write uint64 in big-endian
void write_be64(std::vector<uint8_t>& buf, size_t offset, uint64_t value) {
    buf[offset] = (value >> 56) & 0xFF;
    buf[offset + 1] = (value >> 48) & 0xFF;
    buf[offset + 2] = (value >> 40) & 0xFF;
    buf[offset + 3] = (value >> 32) & 0xFF;
    buf[offset + 4] = (value >> 24) & 0xFF;
    buf[offset + 5] = (value >> 16) & 0xFF;
    buf[offset + 6] = (value >> 8) & 0xFF;
    buf[offset + 7] = value & 0xFF;
}

/// Write 48-bit timestamp in big-endian (6 bytes)
void write_be48(std::vector<uint8_t>& buf, size_t offset, uint64_t value) {
    buf[offset] = (value >> 40) & 0xFF;
    buf[offset + 1] = (value >> 32) & 0xFF;
    buf[offset + 2] = (value >> 24) & 0xFF;
    buf[offset + 3] = (value >> 16) & 0xFF;
    buf[offset + 4] = (value >> 8) & 0xFF;
    buf[offset + 5] = value & 0xFF;
}

/// Write string with right-padding
void write_string(std::vector<uint8_t>& buf, size_t offset, const char* str, size_t len) {
    size_t str_len = std::strlen(str);
    for (size_t i = 0; i < len; ++i) {
        buf[offset + i] = (i < str_len) ? str[i] : ' ';
    }
}

// =============================================================================
// TIMESTAMP TESTS (Critical for 48-bit correctness)
// =============================================================================

void test_timestamp_48bit() {
    std::cout << "\n=== Test: 48-bit Timestamp Handling ===\n";

    // Test various timestamp values
    struct TestCase {
        uint64_t value;
        const char* description;
    };

    TestCase cases[] = {
        {0ULL, "Zero timestamp"},
        {1ULL, "Minimum non-zero"},
        {34200000000000ULL, "9:30 AM (market open)"},
        {57600000000000ULL, "4:00 PM (market close)"},
        {0xFFFFFFFFFFFFULL, "Maximum 48-bit value"},
    };

    for (const auto& tc : cases) {
        // Create SystemEvent with specific timestamp
        std::vector<uint8_t> buffer(12, 0);
        buffer[0] = 'S';
        write_be16(buffer, 1, 1);  // stock_locate
        write_be16(buffer, 3, 100);  // tracking
        write_be48(buffer, 5, tc.value);  // 48-bit timestamp
        buffer[11] = 'Q';  // event_code

        auto result = parse_message(buffer.data(), buffer.size());
        assert(result.is_success());

        const auto& msg = std::get<SystemEvent>(*result.message);

        // Verify timestamp was read correctly (only lower 48 bits)
        uint64_t expected = tc.value & 0xFFFFFFFFFFFFULL;
        assert(msg.timestamp == expected);

        std::cout << "[OK] " << tc.description << ": " << msg.timestamp << "\n";
    }

    std::cout << "[OK] All timestamp tests passed!\n";
}

void test_timestamp_boundary() {
    std::cout << "\n=== Test: Timestamp Field Boundaries ===\n";

    // Ensure we don't read into next field
    std::vector<uint8_t> buffer(12, 0);
    buffer[0] = 'S';
    write_be16(buffer, 1, 5);
    write_be16(buffer, 3, 10);
    write_be48(buffer, 5, 123456789ULL);
    buffer[11] = 'M';  // event_code (should not be part of timestamp)

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<SystemEvent>(*result.message);

    // Verify fields are correct
    assert(msg.stock_locate == 5);
    assert(msg.tracking_number == 10);
    assert(msg.timestamp == 123456789ULL);
    assert(msg.event_code == 'M');  // Should be intact

    std::cout << "[OK] Field boundaries respected\n";
}

// =============================================================================
// COMPREHENSIVE MESSAGE TESTS
// =============================================================================

void test_system_event_all_fields() {
    std::cout << "\n=== Test: SystemEvent All Fields ===\n";

    std::vector<uint8_t> buffer(12, 0);
    buffer[0] = 'S';
    write_be16(buffer, 1, 1234);
    write_be16(buffer, 3, 5678);
    write_be48(buffer, 5, 34200987654321ULL);
    buffer[11] = 'O';

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<SystemEvent>(*result.message);
    assert(msg.stock_locate == 1234);
    assert(msg.tracking_number == 5678);
    assert(msg.timestamp == 34200987654321ULL);
    assert(msg.event_code == 'O');

    std::cout << "[OK] All SystemEvent fields correct\n";
}

void test_add_order_all_fields() {
    std::cout << "\n=== Test: AddOrder All Fields ===\n";

    std::vector<uint8_t> buffer(36, 0);
    buffer[0] = 'A';
    write_be16(buffer, 1, 100);
    write_be16(buffer, 3, 200);
    write_be48(buffer, 5, 34200000000000ULL);
    write_be64(buffer, 11, 999888777666ULL);
    buffer[19] = 'S';  // Sell
    write_be32(buffer, 20, 500);
    write_string(buffer, 24, "MSFT", 8);
    write_be32(buffer, 32, 3501234);  // $350.1234

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<AddOrder>(*result.message);
    assert(msg.stock_locate == 100);
    assert(msg.tracking_number == 200);
    assert(msg.timestamp == 34200000000000ULL);
    assert(msg.order_reference == 999888777666ULL);
    assert(msg.buy_sell_indicator == 'S');
    assert(msg.side() == Side::SELL);
    assert(msg.shares == 500);
    assert(msg.get_symbol() == "MSFT");
    assert(msg.price == 3501234);
    assert(msg.get_price() == 3501234);

    std::cout << "[OK] All AddOrder fields correct\n";
}

void test_order_executed_all_fields() {
    std::cout << "\n=== Test: OrderExecuted All Fields ===\n";

    std::vector<uint8_t> buffer(31, 0);
    buffer[0] = 'E';
    write_be16(buffer, 1, 10);
    write_be16(buffer, 3, 20);
    write_be48(buffer, 5, 57600000000000ULL);
    write_be64(buffer, 11, 123456789012ULL);
    write_be32(buffer, 19, 100);
    write_be64(buffer, 23, 987654321098ULL);

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<OrderExecuted>(*result.message);
    assert(msg.stock_locate == 10);
    assert(msg.tracking_number == 20);
    assert(msg.timestamp == 57600000000000ULL);
    assert(msg.order_reference == 123456789012ULL);
    assert(msg.executed_shares == 100);
    assert(msg.match_number == 987654321098ULL);

    std::cout << "[OK] All OrderExecuted fields correct\n";
}

void test_order_replace_all_fields() {
    std::cout << "\n=== Test: OrderReplace All Fields ===\n";

    std::vector<uint8_t> buffer(35, 0);
    buffer[0] = 'U';
    write_be16(buffer, 1, 50);
    write_be16(buffer, 3, 60);
    write_be48(buffer, 5, 40000000000000ULL);
    write_be64(buffer, 11, 111222333444ULL);  // original
    write_be64(buffer, 19, 555666777888ULL);  // new
    write_be32(buffer, 27, 250);
    write_be32(buffer, 31, 1505000);

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<OrderReplace>(*result.message);
    assert(msg.stock_locate == 50);
    assert(msg.tracking_number == 60);
    assert(msg.timestamp == 40000000000000ULL);
    assert(msg.original_order_reference == 111222333444ULL);
    assert(msg.new_order_reference == 555666777888ULL);
    assert(msg.shares == 250);
    assert(msg.price == 1505000);

    std::cout << "[OK] All OrderReplace fields correct\n";
}

void test_cross_trade_all_fields() {
    std::cout << "\n=== Test: CrossTrade All Fields ===\n";

    std::vector<uint8_t> buffer(CrossTrade::SIZE, 0);
    buffer[0] = 'Q';
    write_be16(buffer, CrossTrade::OFF_STOCK_LOCATE, 70);
    write_be16(buffer, CrossTrade::OFF_TRACKING_NUM, 80);
    write_be48(buffer, CrossTrade::OFF_TIMESTAMP, 45000000000000ULL);
    write_be64(buffer, CrossTrade::OFF_SHARES, 1000);
    write_string(buffer, CrossTrade::OFF_SYMBOL, "XYZ", 8);
    write_be32(buffer, CrossTrade::OFF_CROSS_PRICE, 2005000);
    write_be64(buffer, CrossTrade::OFF_MATCH_NUMBER, 1234512345ULL);
    buffer[CrossTrade::OFF_CROSS_TYPE] = 'O';

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<CrossTrade>(*result.message);
    assert(msg.stock_locate == 70);
    assert(msg.tracking_number == 80);
    assert(msg.timestamp == 45000000000000ULL);
    assert(msg.shares == 1000);
    assert(msg.get_symbol() == "XYZ");
    assert(msg.cross_price == 2005000);
    assert(msg.match_number == 1234512345ULL);
    assert(msg.cross_type == 'O');

    std::cout << "[OK] All CrossTrade fields correct\n";
}

void test_noii_all_fields() {
    std::cout << "\n=== Test: NOII All Fields ===\n";
    
    std::vector<uint8_t> buffer(NOII::SIZE, 0);
    buffer[0] = 'I';
    write_be16(buffer, NOII::OFF_STOCK_LOCATE, 90);
    write_be16(buffer, NOII::OFF_TRACKING_NUM, 100);
    write_be48(buffer, NOII::OFF_TIMESTAMP, 50000000000000ULL);
    write_be64(buffer, NOII::OFF_PAIRED_SHARES, 50000);
    write_be64(buffer, NOII::OFF_IMBALANCE_SHARES, 10000);
    buffer[NOII::OFF_IMBALANCE_DIRECTION] = 'B';
    write_string(buffer, NOII::OFF_SYMBOL, "SPY", 8);
    write_be32(buffer, NOII::OFF_FAR_PRICE, 4500000);
    write_be32(buffer, NOII::OFF_NEAR_PRICE, 4510000);
    write_be32(buffer, NOII::OFF_CURRENT_REFERENCE_PRICE, 4505000);
    buffer[NOII::OFF_CROSS_TYPE] = 'O';
    buffer[NOII::OFF_PRICE_VARIATION_INDICATOR] = ' ';

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<NOII>(*result.message);
    assert(msg.stock_locate == 90);
    assert(msg.tracking_number == 100);
    assert(msg.timestamp == 50000000000000ULL);
    assert(msg.paired_shares == 50000);
    assert(msg.imbalance_shares == 10000);
    assert(msg.imbalance_direction == 'B');
    assert(msg.get_symbol() == "SPY");
    assert(msg.far_price == 4500000);
    assert(msg.near_price == 4510000);
    assert(msg.current_reference_price == 4505000);
    assert(msg.cross_type == 'O');
    assert(msg.price_variation_indicator == ' ');

    std::cout << "[OK] All NOII fields correct\n";
}

void test_rpii_all_fields() {
    std::cout << "\n=== Test: RPII All Fields ===\n";

    std::vector<uint8_t> buffer(RPII::SIZE, 0);
    buffer[0] = 'N';
    write_be16(buffer, RPII::OFF_STOCK_LOCATE, 110);
    write_be16(buffer, RPII::OFF_TRACKING_NUM, 120);
    write_be48(buffer, RPII::OFF_TIMESTAMP, 55000000000000ULL);
    write_string(buffer, RPII::OFF_SYMBOL, "AAPL", 8);
    buffer[RPII::OFF_INTEREST_FLAG] = 'A';

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<RPII>(*result.message);
    assert(msg.stock_locate == 110);
    assert(msg.tracking_number == 120);
    assert(msg.timestamp == 55000000000000ULL);
    assert(msg.get_symbol() == "AAPL");
    assert(msg.interest_flag == 'A');

    std::cout << "[OK] All RPII fields correct\n";
}

void test_market_participant_position_all_fields() {
    std::cout << "\n=== Test: MarketParticipantPosition All Fields ===\n";

    std::vector<uint8_t> buffer(MarketParticipantPosition::SIZE, 0);
    buffer[0] = 'L';
    write_be16(buffer, MarketParticipantPosition::OFF_STOCK_LOCATE, 130);
    write_be16(buffer, MarketParticipantPosition::OFF_TRACKING_NUM, 140);
    write_be48(buffer, MarketParticipantPosition::OFF_TIMESTAMP, 60000000000000ULL);
    write_string(buffer, MarketParticipantPosition::OFF_MPID, "ARCA", 4);
    write_string(buffer, MarketParticipantPosition::OFF_SYMBOL, "GOOG", 8);
    buffer[MarketParticipantPosition::OFF_PRIMARY_MARKET_MAKER] = 'Y';
    buffer[MarketParticipantPosition::OFF_MARKET_MAKER_MODE] = 'N';
    buffer[MarketParticipantPosition::OFF_MARKET_PARTICIPANT_STATE] = 'A';

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<MarketParticipantPosition>(*result.message);
    assert(msg.stock_locate == 130);
    assert(msg.tracking_number == 140);
    assert(msg.timestamp == 60000000000000ULL);
    assert(msg.get_mpid() == "ARCA");
    assert(msg.get_symbol() == "GOOG");
    assert(msg.primary_market_maker == 'Y');
    assert(msg.market_maker_mode == 'N');
    assert(msg.market_participant_state == 'A');
    
    std::cout << "[OK] All MarketParticipantPosition fields correct\n";
}

void test_mwcb_decline_level_all_fields() {
    std::cout << "\n=== Test: MWCBDeclineLevel All Fields ===\n";

    std::vector<uint8_t> buffer(MWCBDeclineLevel::SIZE, 0);
    buffer[0] = 'V';
    write_be16(buffer, MWCBDeclineLevel::OFF_STOCK_LOCATE, 150);
    write_be16(buffer, MWCBDeclineLevel::OFF_TRACKING_NUM, 230);
    write_be48(buffer, MWCBDeclineLevel::OFF_TIMESTAMP, 60000000000000ULL);
    write_be64(buffer, MWCBDeclineLevel::OFF_LEVEL1, 100000000);
    write_be64(buffer, MWCBDeclineLevel::OFF_LEVEL2, 200000000);
    write_be64(buffer, MWCBDeclineLevel::OFF_LEVEL3, 300000000);

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<MWCBDeclineLevel>(*result.message);
    assert(msg.stock_locate == 150);
    assert(msg.tracking_number == 230);
    assert(msg.timestamp == 60000000000000ULL);
    assert(msg.level_1 == 100000000);
    assert(msg.level_2 == 200000000);
    assert(msg.level_3 == 300000000);

    std::cout << "[OK] All MWCBDeclineLevel fields correct\n";
}

void test_mwcb_status_all_fields() {
    std::cout << "\n=== Test: MWCBStatus All Fields ===\n";

    std::vector<uint8_t> buffer(MWCBStatus::SIZE, 0);
    buffer[0] = 'W';
    write_be16(buffer, MWCBStatus::OFF_STOCK_LOCATE, 170);
    write_be16(buffer, MWCBStatus::OFF_TRACKING_NUM, 180);
    write_be48(buffer, MWCBStatus::OFF_TIMESTAMP, 70000000000000ULL);
    buffer[MWCBStatus::OFF_BREACHED_LEVEL] = '1';

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<MWCBStatus>(*result.message);
    assert(msg.stock_locate == 170);
    assert(msg.tracking_number == 180);
    assert(msg.timestamp == 70000000000000ULL);
    assert(msg.breached_level == '1');

    std::cout << "[OK] All MWCBStatus fields correct\n";
}

void test_ipo_quoting_period_update_all_fields() {
    std::cout << "\n=== Test: IPOQuotingPeriodUpdate All Fields ===\n";

    std::vector<uint8_t> buffer(IPOQuotingPeriodUpdate::SIZE, 0);
    buffer[0] = 'K';
    write_be16(buffer, IPOQuotingPeriodUpdate::OFF_STOCK_LOCATE, 190);
    write_be16(buffer, IPOQuotingPeriodUpdate::OFF_TRACKING_NUM, 200);
    write_be48(buffer, IPOQuotingPeriodUpdate::OFF_TIMESTAMP, 75000000000000ULL);
    write_string(buffer, IPOQuotingPeriodUpdate::OFF_SYMBOL, "FB", 8);
    write_be32(buffer, IPOQuotingPeriodUpdate::OFF_IPO_QUOTATION_RELEASE_TIME, 34200);
    buffer[IPOQuotingPeriodUpdate::OFF_IPO_QUOTATION_RELEASE_QUALIFIER] = 'A';
    write_be32(buffer, IPOQuotingPeriodUpdate::OFF_IPO_PRICE, 380000);

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<IPOQuotingPeriodUpdate>(*result.message);
    assert(msg.stock_locate == 190);
    assert(msg.tracking_number == 200);
    assert(msg.timestamp == 75000000000000ULL);
    assert(msg.get_symbol() == "FB");
    assert(msg.ipo_quotation_release_time == 34200);
    assert(msg.ipo_quotation_release_qualifier == 'A');
    assert(msg.ipo_price == 380000);
    
    std::cout << "[OK] All IPOQuotingPeriodUpdate fields correct\n";
}

void test_luld_auction_collar_all_fields() {
    std::cout << "\n=== Test: LULDAuctionCollar All Fields ===\n";

    std::vector<uint8_t> buffer(LULDAuctionCollar::SIZE, 0);
    buffer[0] = 'J';
    write_be16(buffer, LULDAuctionCollar::OFF_STOCK_LOCATE, 210);
    write_be16(buffer, LULDAuctionCollar::OFF_TRACKING_NUM, 220);
    write_be48(buffer, LULDAuctionCollar::OFF_TIMESTAMP, 80000000000000ULL);
    write_string(buffer, LULDAuctionCollar::OFF_SYMBOL, "LULD", 8);
    write_be32(buffer, LULDAuctionCollar::OFF_AUCTION_COLLAR_REFERENCE_PRICE, 1000000);
    write_be32(buffer, LULDAuctionCollar::OFF_UPPER_COLLAR_PRICE, 1100000);
    write_be32(buffer, LULDAuctionCollar::OFF_LOWER_COLLAR_PRICE, 900000);
    write_be32(buffer, LULDAuctionCollar::OFF_AUCTION_COLLAR_EXTENSION, 5);

    auto result = parse_message(buffer.data(), buffer.size());
    assert(result.is_success());

    const auto& msg = std::get<LULDAuctionCollar>(*result.message);
    assert(msg.stock_locate == 210);
    assert(msg.tracking_number == 220);
    assert(msg.timestamp == 80000000000000ULL);
    assert(msg.get_symbol() == "LULD");
    assert(msg.auction_collar_reference_price == 1000000);
    assert(msg.upper_collar_price == 1100000);
    assert(msg.lower_collar_price == 900000);
    assert(msg.auction_collar_extension == 5);
    
    std::cout << "[OK] All LULDAuctionCollar fields correct\n";
}

// =============================================================================
// EDGE CASE TESTS
// =============================================================================

void test_symbol_padding() {
    std::cout << "\n=== Test: Symbol Padding Handling ===\n";

    // Test various symbol lengths
    struct TestCase {
        const char* symbol;
        const char* expected;
    };

    TestCase cases[] = {
        {"AAPL", "AAPL"},
        {"A", "A"},
        {"ABCDEFGH", "ABCDEFGH"},  // Full 8 chars
        {"TST", "TST"},
    };

    for (const auto& tc : cases) {
        std::vector<uint8_t> buffer(36, 0);
        buffer[0] = 'A';
        write_be16(buffer, 1, 1);
        write_be16(buffer, 3, 1);
        write_be48(buffer, 5, 1000000);
        write_be64(buffer, 11, 123);
        buffer[19] = 'B';
        write_be32(buffer, 20, 100);
        write_string(buffer, 24, tc.symbol, 8);
        write_be32(buffer, 32, 1000000);

        auto result = parse_message(buffer.data(), buffer.size());
        assert(result.is_success());

        const auto& msg = std::get<AddOrder>(*result.message);
        assert(msg.get_symbol() == tc.expected);

        std::cout << "[OK] Symbol '" << tc.symbol << "' -> '" << msg.get_symbol() << "'\n";
    }
}

void test_price_boundaries() {
    std::cout << "\n=== Test: Price Field Boundaries ===\n";

    uint32_t prices[] = {
        0,           // Zero (invalid but should parse)
        1,           // Minimum ($0.0001)
        10000,       // $1.00
        1505000,     // $150.50
        999999999,   // Maximum (~$99,999.9999)
    };

    for (uint32_t price : prices) {
        std::vector<uint8_t> buffer(36, 0);
        buffer[0] = 'A';
        write_be16(buffer, 1, 1);
        write_be16(buffer, 3, 1);
        write_be48(buffer, 5, 1000000);
        write_be64(buffer, 11, 999);
        buffer[19] = 'B';
        write_be32(buffer, 20, 100);
        write_string(buffer, 24, "TEST", 8);
        write_be32(buffer, 32, price);

        auto result = parse_message(buffer.data(), buffer.size());
        assert(result.is_success());

        const auto& msg = std::get<AddOrder>(*result.message);
        assert(msg.price == price);

        std::cout << "[OK] Price " << price << " = " << format_price(msg.get_price()) << "\n";
    }
}

void test_quantity_boundaries() {
    std::cout << "\n=== Test: Quantity Boundaries ===\n";

    uint32_t quantities[] = {
        1,           // Minimum
        100,         // Typical round lot
        999999,      // Large order
        4294967295,  // Maximum uint32
    };

    for (uint32_t qty : quantities) {
        std::vector<uint8_t> buffer(36, 0);
        buffer[0] = 'A';
        write_be16(buffer, 1, 1);
        write_be16(buffer, 3, 1);
        write_be48(buffer, 5, 1000000);
        write_be64(buffer, 11, 777);
        buffer[19] = 'B';
        write_be32(buffer, 20, qty);
        write_string(buffer, 24, "TEST", 8);
        write_be32(buffer, 32, 1000000);

        auto result = parse_message(buffer.data(), buffer.size());
        assert(result.is_success());

        const auto& msg = std::get<AddOrder>(*result.message);
        assert(msg.shares == qty);

        std::cout << "[OK] Quantity " << qty << "\n";
    }
}

// =============================================================================
// ERROR HANDLING TESTS
// =============================================================================

void test_message_size_validation() {
    std::cout << "\n=== Test: Message Size Validation ===\n";

    // SystemEvent is 12 bytes
    {
        std::vector<uint8_t> too_short(11, 0);
        too_short[0] = 'S';
        auto result = parse_message(too_short.data(), too_short.size());
        assert(!result.is_success());
        std::cout << "[OK] Rejected too-short SystemEvent\n";
    }

    // AddOrder is 36 bytes
    {
        std::vector<uint8_t> too_short(35, 0);
        too_short[0] = 'A';
        auto result = parse_message(too_short.data(), too_short.size());
        assert(!result.is_success());
        std::cout << "[OK] Rejected too-short AddOrder\n";
    }

    // OrderExecuted is 31 bytes
    {
        std::vector<uint8_t> too_short(30, 0);
        too_short[0] = 'E';
        auto result = parse_message(too_short.data(), too_short.size());
        assert(!result.is_success());
        std::cout << "[OK] Rejected too-short OrderExecuted\n";
    }

    // Test for oversized messages
    {
        std::vector<uint8_t> too_long(13, 0);
        too_long[0] = 'S';
        auto result = parse_message(too_long.data(), too_long.size());
        assert(!result.is_success());
        std::cout << "[OK] Rejected too-long SystemEvent\n";
    }
}

void test_invalid_message_types() {
    std::cout << "\n=== Test: Invalid Message Types ===\n";

    uint8_t invalid_types[] = { 0x00, 0xFF, 'Z', '1', '@' };

    for (uint8_t type : invalid_types) {
        std::vector<uint8_t> buffer(50, 0);
        buffer[0] = type;

        auto result = parse_message(buffer.data(), buffer.size());
        assert(!result.is_success());
        assert(result.error_code == ErrorCode::PARSE_INVALID_TYPE);
    }

    std::cout << "[OK] All invalid types rejected\n";
}

// =============================================================================
// MAIN
// =============================================================================

int main() {
    std::cout << "================================================\n";
    std::cout << "ITCH 5.0 Comprehensive Test Suite\n";
    std::cout << "================================================\n";

    try {
        // Timestamp tests (CRITICAL for 48-bit correctness)
        test_timestamp_48bit();
        test_timestamp_boundary();

        // Comprehensive field tests
        test_system_event_all_fields();
        test_add_order_all_fields();
        test_order_executed_all_fields();
        test_order_replace_all_fields();
        test_cross_trade_all_fields();
        test_noii_all_fields();
        test_rpii_all_fields();
        test_market_participant_position_all_fields();
        test_mwcb_decline_level_all_fields();
        test_mwcb_status_all_fields();
        test_ipo_quoting_period_update_all_fields();
        test_luld_auction_collar_all_fields();

        // Edge cases
        test_symbol_padding();
        test_price_boundaries();
        test_quantity_boundaries();

        // Error handling
        test_message_size_validation();
        test_invalid_message_types();

        std::cout << "\n================================================\n";
        std::cout << "[PASS] ALL COMPREHENSIVE TESTS PASSED!\n";
        std::cout << "================================================\n";

    }
    catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Test failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}