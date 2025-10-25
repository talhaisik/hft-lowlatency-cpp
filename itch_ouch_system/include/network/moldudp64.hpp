#pragma once
// include/network/moldudp64.hpp
//
// NASDAQ MoldUDP64 Protocol Handler
// Specification: MoldUDP64 version 1.00 (August 2, 2024)
// https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf
//
// MoldUDP64 is NASDAQ's UDP-based multicast protocol for delivering sequenced
// messages (ITCH) with gap detection and recovery support.
//
// Packet Structure (Downstream - Multicast):
// - Session ID: 10 bytes (ASCII numeric)
// - Sequence Number: 8 bytes (big-endian uint64_t) - first message's sequence
// - Message Count: 2 bytes (big-endian uint16_t)
//   * 0 = Heartbeat (carries next expected sequence)
//   * 0xFFFF = End-of-session (carries next expected sequence)
//   * 1-N = Normal packet with N messages
// - Message Blocks: Variable length (only if count > 0 and count != 0xFFFF)
//   Each block: Message Length (2 bytes big-endian) + Message Data
//
// IMPLEMENTATION STATUS:
// ✅ Downstream packet parsing (multicast reception)
// ✅ Heartbeat handling with gap detection
// ✅ End-of-session support
// ✅ Sequence tracking and gap detection
// ✅ Zero-copy message extraction
// ❌ Upstream request packets (retransmission requests) - TODO Phase 2b
// ❌ Out-of-order packet buffering - TODO Phase 2b
// ❌ Session validation (verify session ID matches expected) - TODO Phase 2b
//
// Design Philosophy:
// - Zero-copy message extraction where possible
// - Explicit sequence gap detection (including heartbeats)
// - No heap allocations in hot path (except vector reserve)
// - Fail-fast validation (length checks, bounds checks)

#include "common/types.hpp"
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>
#include <optional>
#include <string_view>

namespace hft::network {

    // ============================================================================
    // PROTOCOL CONSTANTS
    // ============================================================================

    namespace protocol {
        constexpr size_t SESSION_ID_LENGTH = 10;
        constexpr size_t HEADER_SIZE = 20;  // 10 (session) + 8 (seq) + 2 (count)
        constexpr size_t MAX_PACKET_SIZE = 1500;  // Standard MTU
        constexpr size_t MAX_MESSAGES_PER_PACKET = 100;  // Reasonable limit
        constexpr uint16_t MAX_MESSAGE_LENGTH = 256;  // ITCH messages are < 100 bytes
        constexpr uint16_t HEARTBEAT_COUNT = 0;      // Heartbeat marker
        constexpr uint16_t END_OF_SESSION = 0xFFFF;  // End-of-session marker
        
        // Compile-time sanity check
        static_assert(HEADER_SIZE == SESSION_ID_LENGTH + 8 + 2, 
                      "HEADER_SIZE must equal session(10) + sequence(8) + count(2)");
    } // namespace protocol

    // ============================================================================
    // ENDIAN CONVERSION (reuse from ITCH)
    // ============================================================================

    namespace detail {
        /// Normalize session ID by trimming trailing spaces and NULs
        /// NOTE: Trims from the END, not at first space, so "ABCD 1234" stays intact
        inline std::string_view normalize_session(const std::array<char, protocol::SESSION_ID_LENGTH>& raw) {
            size_t len = raw.size();
            while (len > 0 && (raw[len - 1] == ' ' || raw[len - 1] == '\0')) {
                --len;
            }
            return std::string_view(raw.data(), len);
        }

        // Detect host endianness at compile time
        #if defined(_MSC_VER)
            // MSVC only ships little-endian targets in practice
            constexpr bool is_little_endian = true;
        #elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
              (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
            constexpr bool is_little_endian = true;
        #else
            constexpr bool is_little_endian = false;
        #endif

        /// Read big-endian uint64_t from buffer
        inline uint64_t read_be64(const uint8_t* buffer) {
            uint64_t value;
            std::memcpy(&value, buffer, sizeof(value));
            
            if constexpr (is_little_endian) {
                #if defined(_MSC_VER)
                    return _byteswap_uint64(value);
                #elif defined(__GNUC__) || defined(__clang__)
                    return __builtin_bswap64(value);
                #else
                    // Fallback for other compilers
                    return ((value & 0xFF00000000000000ULL) >> 56) |
                           ((value & 0x00FF000000000000ULL) >> 40) |
                           ((value & 0x0000FF0000000000ULL) >> 24) |
                           ((value & 0x000000FF00000000ULL) >> 8)  |
                           ((value & 0x00000000FF000000ULL) << 8)  |
                           ((value & 0x0000000000FF0000ULL) << 24) |
                           ((value & 0x000000000000FF00ULL) << 40) |
                           ((value & 0x00000000000000FFULL) << 56);
                #endif
            } else {
                // Big-endian host: no swap needed
                return value;
            }
        }

        /// Read big-endian uint32_t from buffer
        inline uint32_t read_be32(const uint8_t* buffer) {
            uint32_t value;
            std::memcpy(&value, buffer, sizeof(value));
            
            if constexpr (is_little_endian) {
                #if defined(_MSC_VER)
                    return _byteswap_ulong(value);
                #elif defined(__GNUC__) || defined(__clang__)
                    return __builtin_bswap32(value);
                #else
                    return ((value & 0xFF000000u) >> 24) |
                           ((value & 0x00FF0000u) >> 8 ) |
                           ((value & 0x0000FF00u) << 8 ) |
                           ((value & 0x000000FFu) << 24);
                #endif
            } else {
                // Big-endian host: no swap needed
                return value;
            }
        }

        /// Read big-endian uint16_t from buffer
        inline uint16_t read_be16(const uint8_t* buffer) {
            uint16_t value;
            std::memcpy(&value, buffer, sizeof(value));
            
            if constexpr (is_little_endian) {
                #if defined(_MSC_VER)
                    return _byteswap_ushort(value);
                #elif defined(__GNUC__) || defined(__clang__)
                    return __builtin_bswap16(value);
                #else
                    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
                #endif
            } else {
                // Big-endian host: no swap needed
                return value;
            }
        }
    } // namespace detail

    // ============================================================================
    // MOLDUDP64 PACKET HEADER
    // ============================================================================

    /// MoldUDP64 packet header (20 bytes)
    struct MoldUDP64Header {
        std::array<char, protocol::SESSION_ID_LENGTH> session;  // 10 bytes ASCII numeric
        
        /// Sequence number semantics (8 bytes big-endian):
        /// - Normal packet (count > 0, count != 0xFFFF): sequence of FIRST message in packet
        /// - Heartbeat (count == 0): NEXT EXPECTED sequence number (no messages carried)
        /// - End-of-session (count == 0xFFFF): NEXT EXPECTED sequence number (session done)
        uint64_t sequence_number;
        
        uint16_t message_count;    // 2 bytes big-endian

        /// Get session ID as string view (normalized: trimmed)
        std::string_view get_session() const {
            return detail::normalize_session(session);
        }

        /// Parse header from buffer
        [[nodiscard]] static std::optional<MoldUDP64Header> parse(const uint8_t* buffer, size_t length) {
            if (length < protocol::HEADER_SIZE) {
                // TODO(metrics): Increment truncated_header_count
                return std::nullopt;
            }

            MoldUDP64Header header;
            
            // Session ID (10 bytes ASCII)
            std::memcpy(header.session.data(), buffer, protocol::SESSION_ID_LENGTH);
            
            // Sequence number (8 bytes big-endian)
            header.sequence_number = detail::read_be64(buffer + protocol::SESSION_ID_LENGTH);
            
            // Message count (2 bytes big-endian)
            header.message_count = detail::read_be16(buffer + protocol::SESSION_ID_LENGTH + 8);

            return header;
        }
    };

    // ============================================================================
    // MESSAGE BLOCK
    // ============================================================================

    /// A single message block within a MoldUDP64 packet
    /// 
    /// ⚠️ ZERO-COPY LIFETIME WARNING:
    /// The 'data' pointer points directly into the original UDP receive buffer.
    /// This MessageBlock is ONLY VALID as long as the source buffer remains alive.
    /// 
    /// Safe usage patterns:
    /// 1. Single-thread hot path: Process packet immediately, don't store MessageBlock
    /// 2. Multi-thread pipeline: memcpy message data into owned storage before queuing
    /// 
    /// UNSAFE: Storing MessageBlock in a queue while the UDP buffer is reused
    struct MessageBlock {
        uint16_t length;           // Message length
        const uint8_t* data;       // Pointer to message data (zero-copy, see warning above)
        uint64_t sequence;         // Absolute MoldUDP64 sequence number for this message

        /// Check if message length is valid
        bool is_valid() const {
            return length > 0 && length <= protocol::MAX_MESSAGE_LENGTH;
        }
    };

    // ============================================================================
    // MOLDUDP64 PACKET
    // ============================================================================

    /// Complete MoldUDP64 packet with header and messages
    struct MoldUDP64Packet {
        MoldUDP64Header header;
        std::vector<MessageBlock> messages;

        /// Parse packet from buffer
        /// 
        /// ⚠️ LIFETIME WARNING: The returned MoldUDP64Packet contains MessageBlocks
        /// that point directly into `buffer`. The caller MUST ensure `buffer` stays
        /// alive at least until it finishes consuming `packet.messages`. Do not stash
        /// these MessageBlocks beyond that scope.
        /// 
        /// NOTE: Malformed packets that fail to parse are dropped. The next valid packet
        /// will trigger a gap in SequenceTracker, which will request retransmit for the
        /// dropped sequences. This is intentional and correct behavior.
        [[nodiscard]] static std::optional<MoldUDP64Packet> parse(const uint8_t* buffer, size_t length) {
            // Parse header
            auto header_opt = MoldUDP64Header::parse(buffer, length);
            if (!header_opt) {
                return std::nullopt;
            }

            MoldUDP64Packet packet;
            packet.header = *header_opt;

            // Handle special cases
            if (packet.header.message_count == 0) {
                // Heartbeat packet (valid, no messages, carries next expected seq)
                return packet;
            }

            if (packet.header.message_count == protocol::END_OF_SESSION) {
                // End-of-session marker (valid, no messages, allows brief retransmit window)
                return packet;
            }

            if (packet.header.message_count > protocol::MAX_MESSAGES_PER_PACKET) {
                // TODO(metrics): Increment excessive_message_count
                // NOTE: MAX_MESSAGES_PER_PACKET is a defensive limit, not a spec limit
                return std::nullopt;  // Unreasonable message count
            }

            // Extract message blocks
            size_t offset = protocol::HEADER_SIZE;
            packet.messages.reserve(packet.header.message_count);
            
            // Base sequence number for this packet's messages
            uint64_t base_seq = packet.header.sequence_number;

            for (uint16_t i = 0; i < packet.header.message_count; ++i) {
                // Need at least 2 bytes for message length
                if (offset + 2 > length) {
                    // TODO(metrics): Increment truncated_packet_count
                    return std::nullopt;  // Truncated packet
                }

                // Read message length (2 bytes big-endian)
                uint16_t msg_length = detail::read_be16(buffer + offset);
                offset += 2;

                // Validate message length
                if (msg_length == 0 || msg_length > protocol::MAX_MESSAGE_LENGTH) {
                    // TODO(metrics): Increment invalid_message_length_count
                    // NOTE: MAX_MESSAGE_LENGTH is a defensive limit (ITCH messages are <100 bytes)
                    return std::nullopt;  // Invalid message length
                }

                // Check if we have enough data for this message
                if (offset + msg_length > length) {
                    // TODO(metrics): Increment truncated_message_count
                    return std::nullopt;  // Truncated message
                }

                // Create message block (zero-copy: just point to buffer)
                MessageBlock block;
                block.length = msg_length;
                block.data = buffer + offset;
                block.sequence = base_seq + i;  // Absolute sequence number for this message
                packet.messages.push_back(block);

                offset += msg_length;
            }

            // Verify we consumed the expected number of messages
            if (packet.messages.size() != packet.header.message_count) {
                // TODO(metrics): Increment malformed_packet_count
                return std::nullopt;
            }

            // Check for trailing garbage bytes (diagnostic/security)
            // In production, this detects spec version mismatches or NIC padding issues
            if (offset != length) {
                // TODO(metrics): Increment trailing_bytes_count
                // For now, we accept the packet but note the anomaly
                // In strict mode, you might: return std::nullopt;
            }

            return packet;
        }

        /// Check if this is a heartbeat packet (no messages)
        bool is_heartbeat() const {
            return header.message_count == protocol::HEARTBEAT_COUNT;
        }

        /// Check if this is an end-of-session packet
        bool is_end_of_session() const {
            return header.message_count == protocol::END_OF_SESSION;
        }
        
        /// Check if this packet carries actual data messages (not heartbeat/EOS)
        bool carries_data() const {
            return !(is_heartbeat() || is_end_of_session());
        }

        /// Get the sequence number of the first message in this packet
        /// For heartbeats/end-of-session, this is the next expected sequence
        uint64_t first_sequence() const {
            return header.sequence_number;
        }

        /// Get the sequence number of the last message in this packet
        uint64_t last_sequence() const {
            if (header.message_count == 0 || header.message_count == protocol::END_OF_SESSION) {
                // Heartbeat or end-of-session: sequence is next expected, no messages delivered
                return header.sequence_number;
            }
            return header.sequence_number + header.message_count - 1;
        }
    };

    // ============================================================================
    // SEQUENCE GAP DETECTOR
    // ============================================================================

    /// Information about a detected gap in the message sequence
    struct GapInfo {
        bool has_gap;          // True if a gap was detected
        bool out_of_order;     // True if packet arrived out of order (seq < expected)
        bool session_changed;  // True if session ID changed (caller should reset state)
        uint64_t gap_start;    // First missing sequence number
        uint64_t gap_count;    // Number of missing messages
        
        /// Constructor for no gap, in-order
        GapInfo() : has_gap(false), out_of_order(false), session_changed(false), gap_start(0), gap_count(0) {}
        
        /// Constructor for gap
        GapInfo(uint64_t start, uint64_t count) 
            : has_gap(true), out_of_order(false), session_changed(false), gap_start(start), gap_count(count) {}
        
        /// Constructor for out-of-order/duplicate
        static GapInfo out_of_order_packet() {
            GapInfo info;
            info.out_of_order = true;
            return info;
        }
        
        /// Constructor for session change
        static GapInfo session_rollover() {
            GapInfo info;
            info.session_changed = true;
            return info;
        }
    };

    /// Tracks sequence numbers and detects gaps
    /// 
    /// IMPORTANT: Per MoldUDP64 spec, heartbeat packets (count=0) and end-of-session
    /// packets (count=0xFFFF) carry the NEXT EXPECTED sequence number and MUST be
    /// used for gap detection. The sequence field indicates what the next message
    /// sequence would be, allowing gap detection even during low-traffic periods.
    class SequenceTracker {
    public:
        SequenceTracker() 
            : expected_sequence_(0)
            , initialized_(false)
            , end_of_session_(false)
            , current_session_{}
        {}

        /// Process a packet and detect gaps
        /// 
        /// @param packet The MoldUDP64 packet to process
        /// @return GapInfo containing gap details (start sequence and count) if gap detected
        /// 
        /// IMPORTANT: If a gap is detected, use gap_start and gap_count to issue
        /// retransmit requests. The gap range is [gap_start, gap_start + gap_count).
        /// 
        /// Session rollover: If the session ID changes, this returns no-gap and resets
        /// the tracker. The caller should handle this as a session boundary (e.g., reset
        /// order books, symbol tables, etc.).
        /// 
        /// NOTE: Session comparison uses normalized (trimmed) session IDs, so
        /// "ABCDEF    " and "ABCDEF\0\0\0\0" are considered the same session.
        [[nodiscard]] GapInfo process_packet(const MoldUDP64Packet& packet) {
            uint64_t first_seq = packet.first_sequence();
            auto new_session = detail::normalize_session(packet.header.session);

            // First packet initializes the tracker
            if (!initialized_) {
                current_session_ = packet.header.session;
                initialized_ = true;
                
                // Set expected_sequence based on packet type
                if (packet.is_heartbeat() || packet.is_end_of_session()) {
                    // Heartbeat/EOS: sequence_number IS the next expected
                    expected_sequence_ = first_seq;
                    end_of_session_ = packet.is_end_of_session();
                } else {
                    // Normal packet: advance past all messages in this packet
                    expected_sequence_ = packet.last_sequence() + 1;
                }
                
                return GapInfo();  // No gap on initialization
            }

            // Check for session rollover (NASDAQ resets sequence and bumps session ID)
            auto cur_session = detail::normalize_session(current_session_);
            if (new_session != cur_session) {
                // Session changed - this is a regime change
                // Do NOT compute gap across session boundaries (would underflow)
                // Instead, re-initialize tracker with new session
                
                current_session_ = packet.header.session;
                end_of_session_ = false;
                
                // Re-initialize expected_sequence based on packet type
                if (packet.is_heartbeat() || packet.is_end_of_session()) {
                    expected_sequence_ = first_seq;
                    end_of_session_ = packet.is_end_of_session();
                } else {
                    expected_sequence_ = packet.last_sequence() + 1;
                }
                
                // Return session_changed flag so caller can reset order books/state
                return GapInfo::session_rollover();
            }

            // Check for gap (works for all packet types: normal, heartbeat, end-of-session)
            GapInfo gap_info;
            if (first_seq > expected_sequence_) {
                // Gap detected: compute BEFORE updating expected_sequence_
                gap_info = GapInfo(expected_sequence_, first_seq - expected_sequence_);
            } else if (first_seq < expected_sequence_) {
                // Out-of-order or duplicate packet
                // Don't advance expected_sequence_, don't report gap
                // TODO: Implement out-of-order buffering in Phase 2b
                return GapInfo::out_of_order_packet();
            }

            // Update expected sequence for next packet
            // IMPORTANT: Do this AFTER computing gap info so caller has correct gap_start
            if (packet.is_heartbeat()) {
                // Heartbeat: sequence field IS the next expected, don't advance
                expected_sequence_ = first_seq;
            } else if (packet.is_end_of_session()) {
                // End-of-session: sequence field IS the next expected, mark session ended
                expected_sequence_ = first_seq;
                end_of_session_ = true;
            } else {
                // Normal packet: advance by number of messages
                expected_sequence_ = packet.last_sequence() + 1;
            }

            return gap_info;
        }

        /// Get the next expected sequence number
        uint64_t expected_sequence() const {
            return expected_sequence_;
        }

        /// Get the current session ID (normalized, trimmed)
        std::string_view current_session() const {
            return detail::normalize_session(current_session_);
        }

        /// Check if tracker is initialized
        bool is_initialized() const {
            return initialized_;
        }

        /// Check if end-of-session has been received
        bool is_end_of_session() const {
            return end_of_session_;
        }

        /// Reset the tracker (e.g., for new session)
        void reset() {
            expected_sequence_ = 0;
            initialized_ = false;
            end_of_session_ = false;
            current_session_.fill('\0');
        }

    private:
        uint64_t expected_sequence_;
        bool initialized_;
        bool end_of_session_;
        std::array<char, protocol::SESSION_ID_LENGTH> current_session_;
    };

} // namespace hft::network

