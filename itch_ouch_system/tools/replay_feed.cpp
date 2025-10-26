// tools/replay_feed.cpp
//
// Integration test: MoldUDP64 → ITCH → OrderBook pipeline
// Demonstrates end-to-end market data processing with synthetic feed

#include "network/moldudp64.hpp"
#include "itch/messages.hpp"
#include "book/order_book.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

using namespace hft;

// ============================================================================
// SYNTHETIC PACKET GENERATOR
// ============================================================================

/// Helper to create MoldUDP64 packets with ITCH messages for testing
class SyntheticFeedGenerator {
public:
    SyntheticFeedGenerator(const std::string& session_id, uint64_t start_seq)
        : session_id_(session_id)
        , next_sequence_(start_seq)
    {}

    /// Create a packet with multiple ITCH messages
    std::vector<uint8_t> create_packet(const std::vector<std::vector<uint8_t>>& itch_messages) {
        std::vector<uint8_t> packet;
        
        // Session ID (10 bytes, space-padded)
        for (size_t i = 0; i < 10; ++i) {
            packet.push_back(i < session_id_.size() ? session_id_[i] : ' ');
        }
        
        // Sequence number (8 bytes big-endian) - sequence of first message
        uint64_t seq = next_sequence_;
        packet.push_back((seq >> 56) & 0xFF);
        packet.push_back((seq >> 48) & 0xFF);
        packet.push_back((seq >> 40) & 0xFF);
        packet.push_back((seq >> 32) & 0xFF);
        packet.push_back((seq >> 24) & 0xFF);
        packet.push_back((seq >> 16) & 0xFF);
        packet.push_back((seq >> 8) & 0xFF);
        packet.push_back(seq & 0xFF);
        
        // Message count (2 bytes big-endian)
        uint16_t count = static_cast<uint16_t>(itch_messages.size());
        packet.push_back((count >> 8) & 0xFF);
        packet.push_back(count & 0xFF);
        
        // Message blocks (length-prefixed)
        for (const auto& msg : itch_messages) {
            uint16_t len = static_cast<uint16_t>(msg.size());
            packet.push_back((len >> 8) & 0xFF);
            packet.push_back(len & 0xFF);
            packet.insert(packet.end(), msg.begin(), msg.end());
        }
        
        next_sequence_ += count;
        return packet;
    }

    /// Create a heartbeat packet (count=0)
    std::vector<uint8_t> create_heartbeat() {
        std::vector<uint8_t> packet;
        
        // Session ID
        for (size_t i = 0; i < 10; ++i) {
            packet.push_back(i < session_id_.size() ? session_id_[i] : ' ');
        }
        
        // Sequence number (next expected)
        uint64_t seq = next_sequence_;
        packet.push_back((seq >> 56) & 0xFF);
        packet.push_back((seq >> 48) & 0xFF);
        packet.push_back((seq >> 40) & 0xFF);
        packet.push_back((seq >> 32) & 0xFF);
        packet.push_back((seq >> 24) & 0xFF);
        packet.push_back((seq >> 16) & 0xFF);
        packet.push_back((seq >> 8) & 0xFF);
        packet.push_back(seq & 0xFF);
        
        // Message count = 0 (heartbeat)
        packet.push_back(0);
        packet.push_back(0);
        
        return packet;
    }

    uint64_t current_sequence() const { return next_sequence_; }

private:
    std::string session_id_;
    uint64_t next_sequence_;
};

// ============================================================================
// GLOBAL TIMESTAMP (Unified across all ITCH messages)
// ============================================================================

/// Global timestamp generator for ITCH messages
/// Ensures monotonic timestamps across all message types
class GlobalClock {
public:
    static uint64_t next() {
        timestamp_ += 1000;  // 1 microsecond increments
        return timestamp_;
    }
    
    static void reset() {
        timestamp_ = 0;
    }
    
private:
    static inline uint64_t timestamp_ = 0;
};

// ============================================================================
// ITCH MESSAGE BUILDERS
// ============================================================================

/// Build a SystemEvent ITCH message
std::vector<uint8_t> build_system_event(uint16_t stock_locate, char event_code) {
    std::vector<uint8_t> msg(12);  // SystemEvent is 12 bytes
    
    msg[0] = 'S';  // Message type
    
    // Stock locate (2 bytes big-endian)
    msg[1] = (stock_locate >> 8) & 0xFF;
    msg[2] = stock_locate & 0xFF;
    
    // Tracking number (2 bytes) - always 0 for synthetic feed
    // NOTE: Real NASDAQ increments this per UDP packet, not per message
    msg[3] = 0;
    msg[4] = 0;
    
    // Timestamp (6 bytes) - unified monotonic timestamp
    uint64_t timestamp = GlobalClock::next();
    msg[5] = (timestamp >> 40) & 0xFF;
    msg[6] = (timestamp >> 32) & 0xFF;
    msg[7] = (timestamp >> 24) & 0xFF;
    msg[8] = (timestamp >> 16) & 0xFF;
    msg[9] = (timestamp >> 8) & 0xFF;
    msg[10] = timestamp & 0xFF;
    
    msg[11] = event_code;  // Event code
    
    return msg;
}

/// Build an AddOrder ITCH message
std::vector<uint8_t> build_add_order(
    uint16_t stock_locate,
    uint64_t order_ref,
    char side,
    uint32_t shares,
    const char* stock,
    uint32_t price)
{
    std::vector<uint8_t> msg(36);  // AddOrder is 36 bytes
    
    msg[0] = 'A';  // Message type
    
    // Stock locate
    msg[1] = (stock_locate >> 8) & 0xFF;
    msg[2] = stock_locate & 0xFF;
    
    // Tracking number - always 0 for synthetic feed
    msg[3] = 0;
    msg[4] = 0;
    
    // Timestamp - unified monotonic timestamp
    uint64_t timestamp = GlobalClock::next();
    msg[5] = (timestamp >> 40) & 0xFF;
    msg[6] = (timestamp >> 32) & 0xFF;
    msg[7] = (timestamp >> 24) & 0xFF;
    msg[8] = (timestamp >> 16) & 0xFF;
    msg[9] = (timestamp >> 8) & 0xFF;
    msg[10] = timestamp & 0xFF;
    
    // Order reference number (8 bytes big-endian)
    msg[11] = (order_ref >> 56) & 0xFF;
    msg[12] = (order_ref >> 48) & 0xFF;
    msg[13] = (order_ref >> 40) & 0xFF;
    msg[14] = (order_ref >> 32) & 0xFF;
    msg[15] = (order_ref >> 24) & 0xFF;
    msg[16] = (order_ref >> 16) & 0xFF;
    msg[17] = (order_ref >> 8) & 0xFF;
    msg[18] = order_ref & 0xFF;
    
    msg[19] = side;  // Buy/Sell
    
    // Shares (4 bytes big-endian)
    msg[20] = (shares >> 24) & 0xFF;
    msg[21] = (shares >> 16) & 0xFF;
    msg[22] = (shares >> 8) & 0xFF;
    msg[23] = shares & 0xFF;
    
    // Stock symbol (8 bytes, space-padded)
    for (int i = 0; i < 8; ++i) {
        msg[24 + i] = (i < std::strlen(stock)) ? stock[i] : ' ';
    }
    
    // Price (4 bytes big-endian, in 1/10,000 increments)
    msg[32] = (price >> 24) & 0xFF;
    msg[33] = (price >> 16) & 0xFF;
    msg[34] = (price >> 8) & 0xFF;
    msg[35] = price & 0xFF;
    
    return msg;
}

// ============================================================================
// STATISTICS TRACKER
// ============================================================================

struct ReplayStats {
    uint64_t packets_processed = 0;
    uint64_t messages_processed = 0;
    uint64_t gaps_detected = 0;
    uint64_t out_of_order = 0;
    uint64_t session_changes = 0;
    uint64_t parse_errors = 0;
    
    void print() const {
        std::cout << "\n=== Replay Statistics ===\n";
        std::cout << "Packets processed:  " << packets_processed << "\n";
        std::cout << "Messages processed: " << messages_processed << "\n";
        std::cout << "Gaps detected:      " << gaps_detected << "\n";
        std::cout << "Out-of-order:       " << out_of_order << "\n";
        std::cout << "Session changes:    " << session_changes << "\n";
        std::cout << "Parse errors:       " << parse_errors << "\n";
    }
};

// ============================================================================
// MAIN INTEGRATION TEST
// ============================================================================

int main() {
    std::cout << "===============================================\n";
    std::cout << "MoldUDP64 -> ITCH -> OrderBook Integration Test\n";
    std::cout << "===============================================\n\n";

    // Initialize components
    network::SequenceTracker tracker;
    OrderBook book(1, "AAPL");  // stock_locate=1, symbol="AAPL"
    ReplayStats stats;
    
    // Create synthetic feed generator
    SyntheticFeedGenerator feed("SESSION001", 1);
    
    std::cout << "=== Phase 1: System Event ===\n";
    {
        // Create packet with system event (market open)
        auto system_event = build_system_event(1, 'Q');  // 'Q' = Start of market hours
        auto packet_data = feed.create_packet({system_event});
        
        // Parse MoldUDP64 packet
        auto packet = network::MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
        if (!packet) {
            std::cerr << "ERROR: Failed to parse packet\n";
            return 1;
        }
        
        // Track sequence and check for anomalies
        auto gap_info = tracker.process_packet(*packet);
        if (gap_info.has_gap) {
            std::cerr << "ERROR: Unexpected gap in Phase 1: " << gap_info.gap_count << " messages\n";
            return 1;
        }
        if (gap_info.out_of_order) {
            std::cerr << "ERROR: Out-of-order packet in Phase 1\n";
            return 1;
        }
        if (gap_info.session_changed) {
            stats.session_changes++;
        }
        
        stats.packets_processed++;
        stats.messages_processed += packet->messages.size();
        
        std::cout << "  Processed system event packet\n";
        std::cout << "  Session: " << packet->header.get_session() << "\n";
        std::cout << "  Sequence: " << packet->header.sequence_number << "\n";
        std::cout << "  Messages: " << packet->messages.size() << "\n";
    }
    
    std::cout << "\n=== Phase 2: Build Order Book ===\n";
    {
        // Create multiple orders to build a book
        std::vector<std::vector<uint8_t>> orders;
        
        // Add buy orders at different price levels
        orders.push_back(build_add_order(1, 1001, 'B', 100, "AAPL", 1500000));  // $150.00, 100 shares
        orders.push_back(build_add_order(1, 1002, 'B', 200, "AAPL", 1499500));  // $149.95, 200 shares
        orders.push_back(build_add_order(1, 1003, 'B', 150, "AAPL", 1499000));  // $149.90, 150 shares
        
        // Add sell orders at different price levels
        orders.push_back(build_add_order(1, 2001, 'S', 100, "AAPL", 1500500));  // $150.05, 100 shares
        orders.push_back(build_add_order(1, 2002, 'S', 200, "AAPL", 1501000));  // $150.10, 200 shares
        orders.push_back(build_add_order(1, 2003, 'S', 150, "AAPL", 1501500));  // $150.15, 150 shares
        
        auto packet_data = feed.create_packet(orders);
        
        // Parse MoldUDP64
        auto packet = network::MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
        if (!packet) {
            std::cerr << "ERROR: Failed to parse packet\n";
            return 1;
        }
        
        // Track sequence and check for anomalies
        auto gap_info = tracker.process_packet(*packet);
        if (gap_info.has_gap) {
            std::cerr << "ERROR: Unexpected gap in Phase 2: " << gap_info.gap_count << " messages\n";
            return 1;
        }
        if (gap_info.out_of_order) {
            std::cerr << "ERROR: Out-of-order packet in Phase 2\n";
            stats.out_of_order++;
            return 1;
        }
        if (gap_info.session_changed) {
            stats.session_changes++;
        }
        
        stats.packets_processed++;
        
        // Process each ITCH message
        for (const auto& msg_block : packet->messages) {
            auto parse_result = itch::parse_message(msg_block.data, msg_block.length);
            if (!parse_result.is_success()) {
                std::cerr << "ERROR: Failed to parse ITCH message: " 
                          << parse_result.error_detail << "\n";
                stats.parse_errors++;
                continue;
            }
            
            // Update order book
            std::visit([&](auto&& msg) {
                using T = std::decay_t<decltype(msg)>;
                if constexpr (std::is_same_v<T, itch::AddOrder>) {
                    book.add_order(msg);
                    std::cout << "    Added order: " << msg.order_reference 
                              << " " << msg.buy_sell_indicator 
                              << " " << msg.shares << " @ " << msg.price << "\n";
                }
            }, parse_result.message.value());
            
            stats.messages_processed++;
        }
        
        std::cout << "  Processed " << packet->messages.size() << " orders\n";
    }
    
    std::cout << "\n=== Phase 3: Validate Order Book State ===\n";
    {
        auto tob = book.get_top_of_book();
        
        // CRITICAL: Assert expected book state
        if (tob.bid_price != 1500000) {
            std::cerr << "ERROR: Expected best bid $150.00, got $" 
                      << (tob.bid_price / 10000.0) << "\n";
            return 1;
        }
        if (tob.bid_quantity != 100) {
            std::cerr << "ERROR: Expected bid quantity 100, got " << tob.bid_quantity << "\n";
            return 1;
        }
        if (tob.ask_price != 1500500) {
            std::cerr << "ERROR: Expected best ask $150.05, got $" 
                      << (tob.ask_price / 10000.0) << "\n";
            return 1;
        }
        if (tob.ask_quantity != 100) {
            std::cerr << "ERROR: Expected ask quantity 100, got " << tob.ask_quantity << "\n";
            return 1;
        }
        
        std::cout << "  OrderBook state validated!\n";
        std::cout << "  Best Bid: $" << std::fixed << std::setprecision(2) 
                  << (tob.bid_price / 10000.0) << " x " << tob.bid_quantity << " shares\n";
        std::cout << "  Best Ask: $" << std::fixed << std::setprecision(2)
                  << (tob.ask_price / 10000.0) << " x " << tob.ask_quantity << " shares\n";
        std::cout << "  Spread:   $" << std::fixed << std::setprecision(2)
                  << ((tob.ask_price - tob.bid_price) / 10000.0) << "\n";
    }
    
    std::cout << "\n=== Phase 4: Test Heartbeat ===\n";
    {
        auto heartbeat_data = feed.create_heartbeat();
        auto packet = network::MoldUDP64Packet::parse(heartbeat_data.data(), heartbeat_data.size());
        
        if (!packet || !packet->is_heartbeat()) {
            std::cerr << "ERROR: Failed to parse heartbeat\n";
            return 1;
        }
        
        auto gap_info = tracker.process_packet(*packet);
        
        // Heartbeat should not create gaps
        if (gap_info.has_gap) {
            std::cerr << "ERROR: Heartbeat created unexpected gap\n";
            return 1;
        }
        if (gap_info.out_of_order) {
            std::cerr << "ERROR: Heartbeat marked as out-of-order\n";
            return 1;
        }
        
        stats.packets_processed++;
        
        std::cout << "  Processed heartbeat\n";
        std::cout << "  Next expected sequence: " << tracker.expected_sequence() << "\n";
    }
    
    std::cout << "\n=== Phase 5: Test Gap Detection ===\n";
    {
        // Skip some sequences to create a gap
        uint64_t old_seq = feed.current_sequence();
        feed = SyntheticFeedGenerator("SESSION001", old_seq + 10);  // Skip 10 sequences
        
        auto order = build_add_order(1, 3001, 'B', 100, "AAPL", 1498500);
        auto packet_data = feed.create_packet({order});
        auto packet = network::MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
        
        auto gap_info = tracker.process_packet(*packet);
        
        if (gap_info.has_gap) {
            std::cout << "  Gap detected as expected!\n";
            std::cout << "  Gap start:  " << gap_info.gap_start << "\n";
            std::cout << "  Gap count:  " << gap_info.gap_count << "\n";
            std::cout << "  -> Would request retransmit for sequences [" 
                      << gap_info.gap_start << ", " 
                      << (gap_info.gap_start + gap_info.gap_count) << ")\n";
            stats.gaps_detected++;
        } else {
            std::cerr << "ERROR: Gap not detected!\n";
            return 1;
        }
        
        stats.packets_processed++;
        stats.messages_processed++;
    }
    
    // Print final statistics
    stats.print();
    
    std::cout << "\n================================================\n";
    std::cout << "[SUCCESS] Integration test passed!\n";
    std::cout << "================================================\n";
    
    return 0;
}

