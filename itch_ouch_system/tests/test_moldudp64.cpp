// tests/test_moldudp64.cpp
//
// Comprehensive tests for MoldUDP64 packet parsing and sequence tracking

#include "network/moldudp64.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

using namespace hft::network;

// Helper function to create a test packet
std::vector<uint8_t> create_test_packet(
    const char* session,
    uint64_t sequence,
    uint16_t message_count,
    const std::vector<std::vector<uint8_t>>& messages)
{
    std::vector<uint8_t> packet;
    
    // Session ID (10 bytes)
    for (size_t i = 0; i < 10; ++i) {
        packet.push_back(i < std::strlen(session) ? session[i] : ' ');
    }
    
    // Sequence number (8 bytes big-endian)
    packet.push_back((sequence >> 56) & 0xFF);
    packet.push_back((sequence >> 48) & 0xFF);
    packet.push_back((sequence >> 40) & 0xFF);
    packet.push_back((sequence >> 32) & 0xFF);
    packet.push_back((sequence >> 24) & 0xFF);
    packet.push_back((sequence >> 16) & 0xFF);
    packet.push_back((sequence >> 8) & 0xFF);
    packet.push_back(sequence & 0xFF);
    
    // Message count (2 bytes big-endian)
    packet.push_back((message_count >> 8) & 0xFF);
    packet.push_back(message_count & 0xFF);
    
    // Message blocks
    for (const auto& msg : messages) {
        // Message length (2 bytes big-endian)
        uint16_t len = static_cast<uint16_t>(msg.size());
        packet.push_back((len >> 8) & 0xFF);
        packet.push_back(len & 0xFF);
        
        // Message data
        packet.insert(packet.end(), msg.begin(), msg.end());
    }
    
    return packet;
}

void test_header_parsing() {
    std::cout << "\n=== Test: MoldUDP64 Header Parsing ===\n";
    
    // Create a simple packet
    auto packet_data = create_test_packet("TEST123456", 1000, 0, {});
    
    auto header_opt = MoldUDP64Header::parse(packet_data.data(), packet_data.size());
    assert(header_opt.has_value());
    
    auto header = *header_opt;
    assert(header.get_session() == "TEST123456");
    assert(header.sequence_number == 1000);
    assert(header.message_count == 0);
    
    std::cout << "[OK] Header parsed correctly\n";
    std::cout << "     Session: " << header.get_session() << "\n";
    std::cout << "     Sequence: " << header.sequence_number << "\n";
    std::cout << "     Message Count: " << header.message_count << "\n";
}

void test_header_too_short() {
    std::cout << "\n=== Test: Header Too Short ===\n";
    
    std::vector<uint8_t> short_buffer(19, 0);  // Only 19 bytes
    auto header_opt = MoldUDP64Header::parse(short_buffer.data(), short_buffer.size());
    assert(!header_opt.has_value());
    
    std::cout << "[OK] Rejected too-short header\n";
}

void test_heartbeat_packet() {
    std::cout << "\n=== Test: Heartbeat Packet ===\n";
    
    auto packet_data = create_test_packet("HEARTBEAT1", 5000, 0, {});
    auto packet_opt = MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
    
    assert(packet_opt.has_value());
    auto packet = *packet_opt;
    
    assert(packet.is_heartbeat());
    assert(packet.header.message_count == 0);
    assert(packet.messages.empty());
    assert(packet.first_sequence() == 5000);
    assert(packet.last_sequence() == 5000);
    
    std::cout << "[OK] Heartbeat packet parsed correctly\n";
}

void test_single_message_packet() {
    std::cout << "\n=== Test: Single Message Packet ===\n";
    
    // Create a simple ITCH-like message (SystemEvent)
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01, 0x00, 0x02, 
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'O'};
    
    auto packet_data = create_test_packet("SESSION001", 100, 1, {msg1});
    auto packet_opt = MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
    
    assert(packet_opt.has_value());
    auto packet = *packet_opt;
    
    assert(!packet.is_heartbeat());
    assert(packet.header.message_count == 1);
    assert(packet.messages.size() == 1);
    assert(packet.messages[0].length == 12);
    assert(packet.messages[0].data[0] == 'S');
    assert(packet.first_sequence() == 100);
    assert(packet.last_sequence() == 100);
    
    std::cout << "[OK] Single message packet parsed correctly\n";
    std::cout << "     Message length: " << packet.messages[0].length << "\n";
    std::cout << "     Message type: " << static_cast<char>(packet.messages[0].data[0]) << "\n";
}

void test_multiple_messages_packet() {
    std::cout << "\n=== Test: Multiple Messages Packet ===\n";
    
    // Create multiple ITCH-like messages
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};  // 3 bytes
    std::vector<uint8_t> msg2 = {'A', 0x00, 0x02, 0x00, 0x03};  // 5 bytes
    std::vector<uint8_t> msg3 = {'E', 0x00, 0x04};  // 3 bytes
    
    auto packet_data = create_test_packet("MULTI00001", 200, 3, {msg1, msg2, msg3});
    auto packet_opt = MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
    
    assert(packet_opt.has_value());
    auto packet = *packet_opt;
    
    assert(packet.header.message_count == 3);
    assert(packet.messages.size() == 3);
    
    assert(packet.messages[0].length == 3);
    assert(packet.messages[0].data[0] == 'S');
    
    assert(packet.messages[1].length == 5);
    assert(packet.messages[1].data[0] == 'A');
    
    assert(packet.messages[2].length == 3);
    assert(packet.messages[2].data[0] == 'E');
    
    assert(packet.first_sequence() == 200);
    assert(packet.last_sequence() == 202);  // 200 + 3 - 1
    
    std::cout << "[OK] Multiple messages packet parsed correctly\n";
    std::cout << "     Message count: " << packet.messages.size() << "\n";
    std::cout << "     Sequence range: " << packet.first_sequence() 
              << " - " << packet.last_sequence() << "\n";
}

void test_truncated_packet() {
    std::cout << "\n=== Test: Truncated Packet ===\n";
    
    // Create a packet but truncate it
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01, 0x00, 0x02};
    auto packet_data = create_test_packet("TRUNCATED1", 300, 1, {msg1});
    
    // Truncate the packet (remove last 2 bytes)
    packet_data.resize(packet_data.size() - 2);
    
    auto packet_opt = MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
    assert(!packet_opt.has_value());
    
    std::cout << "[OK] Rejected truncated packet\n";
}

void test_invalid_message_count() {
    std::cout << "\n=== Test: Invalid Message Count ===\n";
    
    // Create packet claiming 200 messages (unreasonable)
    auto packet_data = create_test_packet("INVALID001", 400, 200, {});
    auto packet_opt = MoldUDP64Packet::parse(packet_data.data(), packet_data.size());
    
    assert(!packet_opt.has_value());
    
    std::cout << "[OK] Rejected packet with unreasonable message count\n";
}

void test_sequence_tracker_initialization() {
    std::cout << "\n=== Test: Sequence Tracker Initialization ===\n";
    
    SequenceTracker tracker;
    assert(!tracker.is_initialized());
    
    // First packet initializes the tracker
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    auto packet_data = create_test_packet("SEQ0000001", 1000, 1, {msg1});
    auto packet = MoldUDP64Packet::parse(packet_data.data(), packet_data.size()).value();
    
    auto gap_info = tracker.process_packet(packet);
    assert(!gap_info.has_gap);  // No gap on first packet
    assert(tracker.is_initialized());
    assert(tracker.expected_sequence() == 1001);
    
    std::cout << "[OK] Sequence tracker initialized correctly\n";
    std::cout << "     Expected next sequence: " << tracker.expected_sequence() << "\n";
}

void test_sequence_tracker_no_gap() {
    std::cout << "\n=== Test: Sequence Tracker - No Gap ===\n";
    
    SequenceTracker tracker;
    
    // Packet 1: sequence 100, 2 messages
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    std::vector<uint8_t> msg2 = {'A', 0x00, 0x02};
    auto packet1_data = create_test_packet("NOGAP00001", 100, 2, {msg1, msg2});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);
    assert(tracker.expected_sequence() == 102);  // 100 + 2
    
    // Packet 2: sequence 102, 1 message (no gap)
    auto packet2_data = create_test_packet("NOGAP00001", 102, 1, {msg1});
    auto packet2 = MoldUDP64Packet::parse(packet2_data.data(), packet2_data.size()).value();
    
    auto gap_info2 = tracker.process_packet(packet2);
    assert(!gap_info2.has_gap);
    assert(tracker.expected_sequence() == 103);
    
    std::cout << "[OK] No gap detected in sequential packets\n";
}

void test_sequence_tracker_gap_detection() {
    std::cout << "\n=== Test: Sequence Tracker - Gap Detection ===\n";
    
    SequenceTracker tracker;
    
    // Packet 1: sequence 100, 1 message
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    auto packet1_data = create_test_packet("GAP0000001", 100, 1, {msg1});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);  // First packet, no gap
    assert(tracker.expected_sequence() == 101);
    
    // Packet 2: sequence 105 (gap of 4 messages)
    auto packet2_data = create_test_packet("GAP0000001", 105, 1, {msg1});
    auto packet2 = MoldUDP64Packet::parse(packet2_data.data(), packet2_data.size()).value();
    
    auto gap_info = tracker.process_packet(packet2);
    assert(gap_info.has_gap);
    assert(gap_info.gap_start == 101);  // First missing sequence
    assert(gap_info.gap_count == 4);    // Missing sequences 101, 102, 103, 104
    assert(tracker.expected_sequence() == 106);
    
    std::cout << "[OK] Gap detected correctly\n";
    std::cout << "     Gap range: [" << gap_info.gap_start << ", " 
              << (gap_info.gap_start + gap_info.gap_count) << ")\n";
    std::cout << "     Gap count: " << gap_info.gap_count << " messages\n";
}

void test_sequence_tracker_heartbeat() {
    std::cout << "\n=== Test: Sequence Tracker - Heartbeat (No Gap) ===\n";
    
    SequenceTracker tracker;
    
    // Initialize with a regular packet
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    auto packet1_data = create_test_packet("HEARTBEAT1", 100, 1, {msg1});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);  // First packet, no gap
    assert(tracker.expected_sequence() == 101);
    
    // Heartbeat packet with seq=101 (no gap, carries next expected)
    auto heartbeat_data = create_test_packet("HEARTBEAT1", 101, 0, {});
    auto heartbeat = MoldUDP64Packet::parse(heartbeat_data.data(), heartbeat_data.size()).value();
    
    auto gap_info = tracker.process_packet(heartbeat);
    assert(!gap_info.has_gap);
    assert(tracker.expected_sequence() == 101);  // Heartbeat carries next expected
    
    std::cout << "[OK] Heartbeat with no gap processed correctly\n";
}

void test_sequence_tracker_heartbeat_with_gap() {
    std::cout << "\n=== Test: Sequence Tracker - Heartbeat WITH Gap ===\n";
    
    SequenceTracker tracker;
    
    // Initialize with a regular packet
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    auto packet1_data = create_test_packet("HEARTBEAT2", 100, 1, {msg1});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);  // First packet, no gap
    assert(tracker.expected_sequence() == 101);
    
    // Heartbeat packet with seq=105 (gap of 4 messages!)
    // This is the critical test case that was broken before
    auto heartbeat_data = create_test_packet("HEARTBEAT2", 105, 0, {});
    auto heartbeat = MoldUDP64Packet::parse(heartbeat_data.data(), heartbeat_data.size()).value();
    
    auto gap_info = tracker.process_packet(heartbeat);
    assert(gap_info.has_gap);
    assert(gap_info.gap_start == 101);  // First missing sequence
    assert(gap_info.gap_count == 4);    // Missing sequences 101, 102, 103, 104
    assert(tracker.expected_sequence() == 105);  // Updated to heartbeat's seq
    
    std::cout << "[OK] Heartbeat gap detection works correctly\n";
    std::cout << "     Gap range: [" << gap_info.gap_start << ", " 
              << (gap_info.gap_start + gap_info.gap_count) << ")\n";
}

void test_end_of_session_packet() {
    std::cout << "\n=== Test: End-of-Session Packet ===\n";
    
    // Create end-of-session packet (count = 0xFFFF)
    auto eos_data = create_test_packet("ENDOFSESS1", 500, 0xFFFF, {});
    auto eos = MoldUDP64Packet::parse(eos_data.data(), eos_data.size());
    
    assert(eos.has_value());
    assert(eos->is_end_of_session());
    assert(!eos->is_heartbeat());
    assert(eos->messages.empty());
    assert(eos->first_sequence() == 500);
    assert(eos->last_sequence() == 500);  // No messages delivered
    
    std::cout << "[OK] End-of-session packet parsed correctly\n";
}

void test_sequence_tracker_end_of_session() {
    std::cout << "\n=== Test: Sequence Tracker - End-of-Session ===\n";
    
    SequenceTracker tracker;
    
    // Initialize with a regular packet
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    auto packet1_data = create_test_packet("ENDOFSESS2", 100, 1, {msg1});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);  // First packet, no gap
    assert(tracker.expected_sequence() == 101);
    assert(!tracker.is_end_of_session());
    
    // End-of-session packet with seq=101 (no gap)
    auto eos_data = create_test_packet("ENDOFSESS2", 101, 0xFFFF, {});
    auto eos = MoldUDP64Packet::parse(eos_data.data(), eos_data.size()).value();
    
    auto gap_info = tracker.process_packet(eos);
    assert(!gap_info.has_gap);
    assert(tracker.expected_sequence() == 101);
    assert(tracker.is_end_of_session());
    
    std::cout << "[OK] End-of-session tracked correctly\n";
}

void test_sequence_tracker_end_of_session_with_gap() {
    std::cout << "\n=== Test: Sequence Tracker - End-of-Session WITH Gap ===\n";
    
    SequenceTracker tracker;
    
    // Initialize with a regular packet
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    auto packet1_data = create_test_packet("ENDOFSESS3", 100, 1, {msg1});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);  // First packet, no gap
    assert(tracker.expected_sequence() == 101);
    
    // End-of-session packet with seq=110 (gap of 9 messages)
    auto eos_data = create_test_packet("ENDOFSESS3", 110, 0xFFFF, {});
    auto eos = MoldUDP64Packet::parse(eos_data.data(), eos_data.size()).value();
    
    auto gap_info = tracker.process_packet(eos);
    assert(gap_info.has_gap);
    assert(gap_info.gap_start == 101);  // First missing sequence
    assert(gap_info.gap_count == 9);    // Missing sequences 101-109
    assert(tracker.expected_sequence() == 110);
    assert(tracker.is_end_of_session());
    
    std::cout << "[OK] End-of-session with gap detected correctly\n";
    std::cout << "     Gap range: [" << gap_info.gap_start << ", " 
              << (gap_info.gap_start + gap_info.gap_count) << ")\n";
}

void test_session_rollover() {
    std::cout << "\n=== Test: Session Rollover ===\n";
    
    SequenceTracker tracker;
    
    // Initialize with session "SESSION001"
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    auto packet1_data = create_test_packet("SESSION001", 100, 1, {msg1});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);  // First packet, no gap
    assert(tracker.expected_sequence() == 101);  // Advanced past message
    assert(tracker.current_session() == "SESSION001");
    
    // New session "SESSION002" starts at sequence 1 (session rollover)
    auto packet2_data = create_test_packet("SESSION002", 1, 1, {msg1});
    auto packet2 = MoldUDP64Packet::parse(packet2_data.data(), packet2_data.size()).value();
    
    auto gap_info2 = tracker.process_packet(packet2);
    // Session rollover does NOT report a gap (no underflow!)
    assert(!gap_info2.has_gap);  // Fixed: no bogus gap across sessions
    assert(gap_info2.session_changed);  // NEW: session_changed flag set
    assert(tracker.expected_sequence() == 2);  // New session advanced correctly
    assert(tracker.current_session() == "SESSION002");
    
    std::cout << "[OK] Session rollover handled correctly\n";
    std::cout << "     Old session: SESSION001, expected: 101\n";
    std::cout << "     New session: SESSION002, started at: 1, expected: 2\n";
    std::cout << "     No bogus gap reported (underflow prevented)\n";
    std::cout << "     session_changed flag: true (caller can reset order books)\n";
}

void test_initialization_with_data_packet() {
    std::cout << "\n=== Test: Initialization with Data Packet ===\n";
    
    SequenceTracker tracker;
    
    // First packet is a data packet starting at seq 100 with 3 messages
    std::vector<uint8_t> msg1 = {'S', 0x00, 0x01};
    std::vector<uint8_t> msg2 = {'A', 0x00, 0x02};
    std::vector<uint8_t> msg3 = {'E', 0x00, 0x03};
    auto packet1_data = create_test_packet("INIT000001", 100, 3, {msg1, msg2, msg3});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    
    auto gap_info1 = tracker.process_packet(packet1);
    assert(!gap_info1.has_gap);
    // Fixed: should be 103 (100 + 3), not 100
    assert(tracker.expected_sequence() == 103);
    
    // Next packet at seq 103 should not trigger a gap
    auto packet2_data = create_test_packet("INIT000001", 103, 1, {msg1});
    auto packet2 = MoldUDP64Packet::parse(packet2_data.data(), packet2_data.size()).value();
    
    auto gap_info2 = tracker.process_packet(packet2);
    assert(!gap_info2.has_gap);  // No false gap!
    assert(tracker.expected_sequence() == 104);
    
    std::cout << "[OK] Initialization with data packet works correctly\n";
    std::cout << "     First packet: seq 100, count 3 → expected 103\n";
    std::cout << "     Second packet: seq 103 → no false gap\n";
}

void test_session_id_variations() {
    std::cout << "\n=== Test: Session ID Variations ===\n";
    
    // Short session ID
    auto packet1_data = create_test_packet("ABC", 1, 0, {});
    auto packet1 = MoldUDP64Packet::parse(packet1_data.data(), packet1_data.size()).value();
    assert(packet1.header.get_session() == "ABC");
    
    // Full 10-character session ID
    auto packet2_data = create_test_packet("0123456789", 2, 0, {});
    auto packet2 = MoldUDP64Packet::parse(packet2_data.data(), packet2_data.size()).value();
    assert(packet2.header.get_session() == "0123456789");
    
    // CRITICAL: Session with internal space should NOT be truncated at first space
    // Manually build packet with session "ABCD 1234 " (trailing space)
    std::vector<uint8_t> packet3_data;
    std::array<char, 10> session_with_space = {'A', 'B', 'C', 'D', ' ', '1', '2', '3', '4', ' '};
    for (char c : session_with_space) {
        packet3_data.push_back(static_cast<uint8_t>(c));
    }
    // Sequence: 100
    uint64_t seq = 100;
    packet3_data.push_back((seq >> 56) & 0xFF);
    packet3_data.push_back((seq >> 48) & 0xFF);
    packet3_data.push_back((seq >> 40) & 0xFF);
    packet3_data.push_back((seq >> 32) & 0xFF);
    packet3_data.push_back((seq >> 24) & 0xFF);
    packet3_data.push_back((seq >> 16) & 0xFF);
    packet3_data.push_back((seq >> 8) & 0xFF);
    packet3_data.push_back(seq & 0xFF);
    // Message count: 0 (heartbeat)
    packet3_data.push_back(0);
    packet3_data.push_back(0);
    
    auto packet3 = MoldUDP64Packet::parse(packet3_data.data(), packet3_data.size()).value();
    // Should be "ABCD 1234" (trimmed trailing space), NOT "ABCD"
    assert(packet3.header.get_session() == "ABCD 1234");
    
    std::cout << "[OK] Session ID variations handled correctly\n";
    std::cout << "     Internal spaces preserved (ABCD 1234 stays intact)\n";
}

int main() {
    try {
        std::cout << "================================================\n";
        std::cout << "MoldUDP64 Comprehensive Test Suite\n";
        std::cout << "================================================\n";

        // Header tests
        test_header_parsing();
        test_header_too_short();

        // Packet parsing tests
        test_heartbeat_packet();
        test_single_message_packet();
        test_multiple_messages_packet();
        test_truncated_packet();
        test_invalid_message_count();

        // Sequence tracking tests
        test_sequence_tracker_initialization();
        test_sequence_tracker_no_gap();
        test_sequence_tracker_gap_detection();
        test_sequence_tracker_heartbeat();
        test_sequence_tracker_heartbeat_with_gap();

        // End-of-session tests
        test_end_of_session_packet();
        test_sequence_tracker_end_of_session();
        test_sequence_tracker_end_of_session_with_gap();

        // Session management
        test_session_rollover();
        test_initialization_with_data_packet();

        // Edge cases
        test_session_id_variations();

        std::cout << "\n================================================\n";
        std::cout << "[PASS] ALL MOLDUDP64 TESTS PASSED!\n";
        std::cout << "================================================\n";
    }
    catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Test failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

