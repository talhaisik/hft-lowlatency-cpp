#pragma once
// include/itch/messages.hpp
//
// NASDAQ ITCH 5.0 Protocol Message Definitions
// Specification: https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf
//
// Design Philosophy:
// - Binary-accurate layout matching specification
// - Network byte order (big-endian) for multi-byte fields
// - Safe parsing with explicit memcpy (no aliasing UB)
// - std::variant for type-safe message dispatch
// - Zero-copy where safe, copy when necessary

#include "common/types.hpp"
#include <variant>
#include <optional>
#include <array>
#include <cstring>
#include <string_view>
#include <cstdio>
#include <bit>

namespace hft::itch {

    // ============================================================================
    // PROTOCOL CONSTANTS
    // ============================================================================

    namespace protocol {
        constexpr uint8_t ITCH_VERSION = 50;  // ITCH 5.0
        constexpr size_t MAX_MESSAGE_SIZE = 64;
        constexpr size_t SYMBOL_LENGTH = 8;
    } // namespace protocol

    // ============================================================================
    // TIMESTAMP SEMANTICS
    // ============================================================================
    //
    // All ITCH 5.0 messages contain a 48-bit (6-byte) timestamp field representing
    // nanoseconds since midnight (00:00:00.000000000).
    //
    // Wire format: 6 bytes, big-endian, at offset 5 in most messages
    // In-memory:   uint64_t (upper 16 bits always zero)
    // Range:       0 to 86,399,999,999,999 (23:59:59.999999999)
    // Rollover:    Resets to 0 at midnight (session boundary)
    //
    // Usage notes:
    // - Use for message sequencing within a trading day
    // - Delta calculations valid only within same session
    // - For cross-session replay, track session date separately
    // - Monotonic within a session (except for retransmits)
    //
    // ============================================================================

    // ============================================================================
    // ENDIAN CONVERSION UTILITIES
    // ============================================================================

    namespace detail {

        /// Convert big-endian to host byte order (16-bit)
        inline uint16_t be16toh(uint16_t val) {
            if constexpr (std::endian::native == std::endian::big) {
                return val;
            }
#ifdef _MSC_VER
            return _byteswap_ushort(val);
#else // Assume GCC/Clang
            return __builtin_bswap16(val);
#endif
        }

        /// Convert big-endian to host byte order (32-bit)
        inline uint32_t be32toh(uint32_t val) {
            if constexpr (std::endian::native == std::endian::big) {
                return val;
            }
#ifdef _MSC_VER
            return _byteswap_ulong(val);
#else // Assume GCC/Clang
            return __builtin_bswap32(val);
#endif
        }

        /// Convert big-endian to host byte order (64-bit)
        inline uint64_t be64toh(uint64_t val) {
            if constexpr (std::endian::native == std::endian::big) {
                return val;
            }
#ifdef _MSC_VER
            return _byteswap_uint64(val);
#else // Assume GCC/Clang
            return __builtin_bswap64(val);
#endif
        }

        /// Read 48-bit big-endian timestamp (6 bytes) into uint64_t
        /// See TIMESTAMP SEMANTICS section above for field interpretation
        inline uint64_t read_be48(const uint8_t* buffer) {
            uint64_t value = 0;
            value |= static_cast<uint64_t>(buffer[0]) << 40;
            value |= static_cast<uint64_t>(buffer[1]) << 32;
            value |= static_cast<uint64_t>(buffer[2]) << 24;
            value |= static_cast<uint64_t>(buffer[3]) << 16;
            value |= static_cast<uint64_t>(buffer[4]) << 8;
            value |= static_cast<uint64_t>(buffer[5]);
            return value;
        }

        /// Safe read of multi-byte value from buffer (avoids aliasing UB)
        template<typename T>
        inline T read_big_endian(const uint8_t* buffer) {
            T value;
            std::memcpy(&value, buffer, sizeof(T));

            if constexpr (sizeof(T) == 2) {
                return be16toh(value);
            }
            else if constexpr (sizeof(T) == 4) {
                return be32toh(value);
            }
            else if constexpr (sizeof(T) == 8) {
                return be64toh(value);
            }
            else {
                return value;  // Single byte, no conversion
            }
        }

        /// Create a string_view from a char array, trimming trailing spaces
        template<size_t N>
        inline std::string_view view_trimmed(const std::array<char, N>& arr) {
            size_t actual_len = N;
            while (actual_len > 0 && arr[actual_len - 1] == ' ') {
                actual_len--;
            }
            return std::string_view(arr.data(), actual_len);
        }

    } // namespace detail

    // ============================================================================
    // MESSAGE TYPE ENUM
    // ============================================================================

    enum class MessageType : uint8_t {
        // System Events
        SYSTEM_EVENT = 'S',
        STOCK_DIRECTORY = 'R',
        STOCK_TRADING_ACTION = 'H',
        REG_SHO_RESTRICTION = 'Y',
        MARKET_PARTICIPANT_POSITION = 'L',
        MWCB_DECLINE_LEVEL = 'V',
        MWCB_STATUS = 'W',
        IPO_QUOTING_PERIOD_UPDATE = 'K',
        LULD_AUCTION_COLLAR = 'J',
        OPERATIONAL_HALT = 'h',

        // Order Book - Add Orders
        ADD_ORDER = 'A',
        ADD_ORDER_MPID = 'F',

        // Order Book - Modify Orders
        ORDER_EXECUTED = 'E',
        ORDER_EXECUTED_WITH_PRICE = 'C',
        ORDER_CANCEL = 'X',
        ORDER_DELETE = 'D',
        ORDER_REPLACE = 'U',

        // Trades
        TRADE_NON_CROSS = 'P',
        TRADE_CROSS = 'Q',
        BROKEN_TRADE = 'B',

        // Net Order Imbalance Indicator (NOII)
        NOII = 'I',

        // Retail Price Improvement Indicator (RPII)
        RPII = 'N',

        // Direct Listing with Capital Raise Price Discovery
        DLCR = 'O',

        UNKNOWN = 0
    };

    inline const char* message_type_to_string(MessageType type) {
        switch (type) {
        case MessageType::SYSTEM_EVENT: return "SYSTEM_EVENT";
        case MessageType::STOCK_DIRECTORY: return "STOCK_DIRECTORY";
        case MessageType::STOCK_TRADING_ACTION: return "STOCK_TRADING_ACTION";
        case MessageType::REG_SHO_RESTRICTION: return "REG_SHO_RESTRICTION";
        case MessageType::MARKET_PARTICIPANT_POSITION: return "MARKET_PARTICIPANT_POSITION";
        case MessageType::MWCB_DECLINE_LEVEL: return "MWCB_DECLINE_LEVEL";
        case MessageType::MWCB_STATUS: return "MWCB_STATUS";
        case MessageType::IPO_QUOTING_PERIOD_UPDATE: return "IPO_QUOTING_PERIOD_UPDATE";
        case MessageType::LULD_AUCTION_COLLAR: return "LULD_AUCTION_COLLAR";
        case MessageType::OPERATIONAL_HALT: return "OPERATIONAL_HALT";
        case MessageType::ADD_ORDER: return "ADD_ORDER";
        case MessageType::ADD_ORDER_MPID: return "ADD_ORDER_MPID";
        case MessageType::ORDER_EXECUTED: return "ORDER_EXECUTED";
        case MessageType::ORDER_EXECUTED_WITH_PRICE: return "ORDER_EXECUTED_WITH_PRICE";
        case MessageType::ORDER_CANCEL: return "ORDER_CANCEL";
        case MessageType::ORDER_DELETE: return "ORDER_DELETE";
        case MessageType::ORDER_REPLACE: return "ORDER_REPLACE";
        case MessageType::TRADE_NON_CROSS: return "TRADE_NON_CROSS";
        case MessageType::TRADE_CROSS: return "TRADE_CROSS";
        case MessageType::BROKEN_TRADE: return "BROKEN_TRADE";
        case MessageType::NOII: return "NOII";
        case MessageType::RPII: return "RPII";
        case MessageType::DLCR: return "DLCR";
        default: return "UNKNOWN";
        }
    }

    // ============================================================================
    // SYSTEM EVENT MESSAGES
    // ============================================================================

    /// System Event (Type 'S') - Length: 12 bytes
    struct SystemEvent {
        static constexpr MessageType TYPE = MessageType::SYSTEM_EVENT;
        static constexpr size_t SIZE = 12;

        static constexpr size_t OFF_STOCK_LOCATE  = 1;
        static constexpr size_t OFF_TRACKING_NUM  = 3;
        static constexpr size_t OFF_TIMESTAMP     = 5;
        static constexpr size_t OFF_EVENT_CODE    = 11;

        uint16_t stock_locate;      // Offset 1-2
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        char event_code;            // Offset 11

        // Event codes
        static constexpr char EVENT_START_OF_MESSAGES = 'O';
        static constexpr char EVENT_START_OF_SYSTEM_HOURS = 'S';
        static constexpr char EVENT_START_OF_MARKET_HOURS = 'Q';
        static constexpr char EVENT_END_OF_MARKET_HOURS = 'M';
        static constexpr char EVENT_END_OF_SYSTEM_HOURS = 'E';
        static constexpr char EVENT_END_OF_MESSAGES = 'C';

        MarketStatus to_market_status() const {
            switch (event_code) {
            case EVENT_START_OF_MARKET_HOURS: return MarketStatus::OPEN;
            case EVENT_END_OF_MARKET_HOURS: return MarketStatus::CLOSED;
            default: return MarketStatus::CLOSED;
            }
        }

        static std::optional<SystemEvent> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            SystemEvent msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.event_code = buffer[OFF_EVENT_CODE];

            return msg;
        }
    };

    /// Stock Directory (Type 'R') - Length: 39 bytes
    struct StockDirectory {
        static constexpr MessageType TYPE = MessageType::STOCK_DIRECTORY;
        static constexpr size_t SIZE = 39;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_MARKET_CATEGORY = 19;
        static constexpr size_t OFF_FINANCIAL_STATUS = 20;
        static constexpr size_t OFF_ROUND_LOT_SIZE = 21;
        static constexpr size_t OFF_ROUND_LOTS_ONLY = 25;
        static constexpr size_t OFF_ISSUE_CLASSIFICATION = 26;
        static constexpr size_t OFF_ISSUE_SUBTYPE = 27;
        static constexpr size_t OFF_AUTHENTICITY = 29;
        static constexpr size_t OFF_SHORT_SALE_THRESHOLD = 30;
        static constexpr size_t OFF_IPO_FLAG = 31;
        static constexpr size_t OFF_LULD_PRICE_TIER = 32;
        static constexpr size_t OFF_ETP_FLAG = 33;
        static constexpr size_t OFF_ETP_LEVERAGE_FACTOR = 34;
        static constexpr size_t OFF_INVERSE_INDICATOR = 38;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;          // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;  // 8 bytes (right-padded)
        char market_category;
        char financial_status;
        uint32_t round_lot_size;
        char round_lots_only;
        char issue_classification;
        std::array<char, 2> issue_subtype;  // 2 bytes
        char authenticity;
        char short_sale_threshold;
        char ipo_flag;
        char luld_price_tier;
        char etp_flag;
        uint32_t etp_leverage_factor;
        char inverse_indicator;

        std::string_view get_symbol() const {
            return detail::view_trimmed(symbol);
        }
        std::string_view get_issue_subtype() const {
            return detail::view_trimmed(issue_subtype);
        }

        static std::optional<StockDirectory> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            StockDirectory msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.market_category = buffer[OFF_MARKET_CATEGORY];
            msg.financial_status = buffer[OFF_FINANCIAL_STATUS];
            msg.round_lot_size = detail::read_big_endian<uint32_t>(buffer + OFF_ROUND_LOT_SIZE);
            msg.round_lots_only = buffer[OFF_ROUND_LOTS_ONLY];
            msg.issue_classification = buffer[OFF_ISSUE_CLASSIFICATION];
            std::memcpy(msg.issue_subtype.data(), buffer + OFF_ISSUE_SUBTYPE, 2);
            msg.authenticity = buffer[OFF_AUTHENTICITY];
            msg.short_sale_threshold = buffer[OFF_SHORT_SALE_THRESHOLD];
            msg.ipo_flag = buffer[OFF_IPO_FLAG];
            msg.luld_price_tier = buffer[OFF_LULD_PRICE_TIER];
            msg.etp_flag = buffer[OFF_ETP_FLAG];
            msg.etp_leverage_factor = detail::read_big_endian<uint32_t>(buffer + OFF_ETP_LEVERAGE_FACTOR);
            msg.inverse_indicator = buffer[OFF_INVERSE_INDICATOR];

            return msg;
        }
    };

    /// Stock Trading Action (Type 'H') - Length: 25 bytes
    struct StockTradingAction {
        static constexpr MessageType TYPE = MessageType::STOCK_TRADING_ACTION;
        static constexpr size_t SIZE = 25;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_TRADING_STATE = 19;
        static constexpr size_t OFF_RESERVED = 20;
        static constexpr size_t OFF_REASON = 21;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;         // 8 bytes
        char trading_state;
        char reserved;
        std::array<char, 4> reason;         // 4 bytes

        // Trading states
        static constexpr char STATE_HALTED = 'H';
        static constexpr char STATE_PAUSED = 'P';
        static constexpr char STATE_QUOTATION_ONLY = 'Q';
        static constexpr char STATE_TRADING = 'T';

        bool is_halted() const {
            return trading_state == STATE_HALTED || trading_state == STATE_PAUSED;
        }

        std::string_view get_symbol() const {
            return detail::view_trimmed(symbol);
        }
        std::string_view get_reason() const {
            return detail::view_trimmed(reason);
        }

        static std::optional<StockTradingAction> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            StockTradingAction msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.trading_state = buffer[OFF_TRADING_STATE];
            msg.reserved = buffer[OFF_RESERVED];
            std::memcpy(msg.reason.data(), buffer + OFF_REASON, 4);

            return msg;
        }
    };

    /// Reg SHO Short Sale Price Test Restriction (Type 'Y') - Length: 20 bytes
    struct RegSHORestriction {
        static constexpr MessageType TYPE = MessageType::REG_SHO_RESTRICTION;
        static constexpr size_t SIZE = 20;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_REG_SHO_ACTION = 19;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;         // 8 bytes
        char reg_sho_action;

        // Reg SHO Actions
        static constexpr char ACTION_NO_RESTRICTION = '0';
        static constexpr char ACTION_RESTRICTION_IN_EFFECT = '1';
        static constexpr char ACTION_RESTRICTION_REMAINS = '2';

        bool is_restricted() const {
            return reg_sho_action == ACTION_RESTRICTION_IN_EFFECT ||
                reg_sho_action == ACTION_RESTRICTION_REMAINS;
        }

        std::string_view get_symbol() const {
            return detail::view_trimmed(symbol);
        }

        static std::optional<RegSHORestriction> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            RegSHORestriction msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.reg_sho_action = buffer[OFF_REG_SHO_ACTION];

            return msg;
        }
    };

    /// Market Participant Position (Type 'L') - Length: 26 bytes
    struct MarketParticipantPosition {
        static constexpr MessageType TYPE = MessageType::MARKET_PARTICIPANT_POSITION;
        static constexpr size_t SIZE = 26;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_MPID = 11;
        static constexpr size_t OFF_SYMBOL = 15;
        static constexpr size_t OFF_PRIMARY_MARKET_MAKER = 23;
        static constexpr size_t OFF_MARKET_MAKER_MODE = 24;
        static constexpr size_t OFF_MARKET_PARTICIPANT_STATE = 25;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 4> mpid;
        std::array<char, 8> symbol;
        char primary_market_maker;
        char market_maker_mode;
        char market_participant_state;

        std::string_view get_mpid() const { return detail::view_trimmed(mpid); }
        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }

        static std::optional<MarketParticipantPosition> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            MarketParticipantPosition msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            std::memcpy(msg.mpid.data(), buffer + OFF_MPID, 4);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.primary_market_maker = buffer[OFF_PRIMARY_MARKET_MAKER];
            msg.market_maker_mode = buffer[OFF_MARKET_MAKER_MODE];
            msg.market_participant_state = buffer[OFF_MARKET_PARTICIPANT_STATE];
            return msg;
        }
    };

    /// MWCB Decline Level (Type 'V') - Length: 35 bytes
    struct MWCBDeclineLevel {
        static constexpr MessageType TYPE = MessageType::MWCB_DECLINE_LEVEL;
        static constexpr size_t SIZE = 35;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_LEVEL1 = 11;
        static constexpr size_t OFF_LEVEL2 = 19;
        static constexpr size_t OFF_LEVEL3 = 27;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t level1;
        uint64_t level2;
        uint64_t level3;

        static std::optional<MWCBDeclineLevel> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            MWCBDeclineLevel msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            msg.level1 = detail::read_big_endian<uint64_t>(buffer + OFF_LEVEL1);
            msg.level2 = detail::read_big_endian<uint64_t>(buffer + OFF_LEVEL2);
            msg.level3 = detail::read_big_endian<uint64_t>(buffer + OFF_LEVEL3);
            return msg;
        }
    };

    /// MWCB Status (Type 'W') - Length: 12 bytes
    struct MWCBStatus {
        static constexpr MessageType TYPE = MessageType::MWCB_STATUS;
        static constexpr size_t SIZE = 12;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_BREACHED_LEVEL = 11;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        char breached_level;

        static std::optional<MWCBStatus> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            MWCBStatus msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            msg.breached_level = buffer[OFF_BREACHED_LEVEL];
            return msg;
        }
    };

    /// IPO Quoting Period Update (Type 'K') - Length: 28 bytes
    struct IPOQuotingPeriodUpdate {
        static constexpr MessageType TYPE = MessageType::IPO_QUOTING_PERIOD_UPDATE;
        static constexpr size_t SIZE = 28;
        
        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_IPO_QUOTATION_RELEASE_TIME = 19;
        static constexpr size_t OFF_IPO_QUOTATION_RELEASE_QUALIFIER = 23;
        static constexpr size_t OFF_IPO_PRICE = 24;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;
        uint32_t ipo_quotation_release_time;
        char ipo_quotation_release_qualifier;
        uint32_t ipo_price;

        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }

        static std::optional<IPOQuotingPeriodUpdate> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            IPOQuotingPeriodUpdate msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.ipo_quotation_release_time = detail::read_big_endian<uint32_t>(buffer + OFF_IPO_QUOTATION_RELEASE_TIME);
            msg.ipo_quotation_release_qualifier = buffer[OFF_IPO_QUOTATION_RELEASE_QUALIFIER];
            msg.ipo_price = detail::read_big_endian<uint32_t>(buffer + OFF_IPO_PRICE);
            return msg;
        }
    };

    /// LULD Auction Collar (Type 'J') - Length: 35 bytes
    struct LULDAuctionCollar {
        static constexpr MessageType TYPE = MessageType::LULD_AUCTION_COLLAR;
        static constexpr size_t SIZE = 35;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_AUCTION_COLLAR_REFERENCE_PRICE = 19;
        static constexpr size_t OFF_UPPER_COLLAR_PRICE = 23;
        static constexpr size_t OFF_LOWER_COLLAR_PRICE = 27;
        static constexpr size_t OFF_AUCTION_COLLAR_EXTENSION = 31;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;
        uint32_t auction_collar_reference_price;
        uint32_t upper_collar_price;
        uint32_t lower_collar_price;
        uint32_t auction_collar_extension;

        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }
        
        static std::optional<LULDAuctionCollar> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            LULDAuctionCollar msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.auction_collar_reference_price = detail::read_big_endian<uint32_t>(buffer + OFF_AUCTION_COLLAR_REFERENCE_PRICE);
            msg.upper_collar_price = detail::read_big_endian<uint32_t>(buffer + OFF_UPPER_COLLAR_PRICE);
            msg.lower_collar_price = detail::read_big_endian<uint32_t>(buffer + OFF_LOWER_COLLAR_PRICE);
            msg.auction_collar_extension = detail::read_big_endian<uint32_t>(buffer + OFF_AUCTION_COLLAR_EXTENSION);
            return msg;
        }
    };

    /// Operational Halt (Type 'h') - Length: 21 bytes
    struct OperationalHalt {
        static constexpr MessageType TYPE = MessageType::OPERATIONAL_HALT;
        static constexpr size_t SIZE = 21;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_MARKET_CODE = 19;
        static constexpr size_t OFF_OPERATIONAL_HALT_ACTION = 20;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;
        char market_code;
        char operational_halt_action;

        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }

        static std::optional<OperationalHalt> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            OperationalHalt msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.market_code = buffer[OFF_MARKET_CODE];
            msg.operational_halt_action = buffer[OFF_OPERATIONAL_HALT_ACTION];
            return msg;
        }
    };

    // ============================================================================
    // ORDER BOOK - ADD ORDERS
    // ============================================================================

    /// Add Order (Type 'A') - Length: 36 bytes
    struct AddOrder {
        static constexpr MessageType TYPE = MessageType::ADD_ORDER;
        static constexpr size_t SIZE = 36;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORDER_REFERENCE = 11;
        static constexpr size_t OFF_BUY_SELL_INDICATOR = 19;
        static constexpr size_t OFF_SHARES = 20;
        static constexpr size_t OFF_SYMBOL = 24;
        static constexpr size_t OFF_PRICE = 32;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t order_reference;   // Unique order ID
        char buy_sell_indicator;    // 'B' or 'S'
        uint32_t shares;
        std::array<char, 8> symbol;         // 8 bytes
        uint32_t price;             // Price in 1/10,000 dollars

        Side side() const {
            return (buy_sell_indicator == 'B') ? Side::BUY : Side::SELL;
        }

        Price get_price() const {
            return static_cast<Price>(price);
        }

        std::string_view get_symbol() const {
            return detail::view_trimmed(symbol);
        }

        static std::optional<AddOrder> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            AddOrder msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORDER_REFERENCE);
            msg.buy_sell_indicator = buffer[OFF_BUY_SELL_INDICATOR];
            msg.shares = detail::read_big_endian<uint32_t>(buffer + OFF_SHARES);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.price = detail::read_big_endian<uint32_t>(buffer + OFF_PRICE);

            return msg;
        }
    };

    /// Add Order with MPID (Type 'F') - Length: 40 bytes
    struct AddOrderMPID {
        static constexpr MessageType TYPE = MessageType::ADD_ORDER_MPID;
        static constexpr size_t SIZE = 40;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORDER_REFERENCE = 11;
        static constexpr size_t OFF_BUY_SELL_INDICATOR = 19;
        static constexpr size_t OFF_SHARES = 20;
        static constexpr size_t OFF_SYMBOL = 24;
        static constexpr size_t OFF_PRICE = 32;
        static constexpr size_t OFF_ATTRIBUTION = 36;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t order_reference;
        char buy_sell_indicator;
        uint32_t shares;
        std::array<char, 8> symbol;         // 8 bytes
        uint32_t price;
        std::array<char, 4> attribution;    // 4 bytes (Market Participant ID)

        Side side() const {
            return (buy_sell_indicator == 'B') ? Side::BUY : Side::SELL;
        }

        Price get_price() const {
            return static_cast<Price>(price);
        }

        std::string_view get_symbol() const {
            return detail::view_trimmed(symbol);
        }
        std::string_view get_attribution() const {
            return detail::view_trimmed(attribution);
        }

        static std::optional<AddOrderMPID> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            AddOrderMPID msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORDER_REFERENCE);
            msg.buy_sell_indicator = buffer[OFF_BUY_SELL_INDICATOR];
            msg.shares = detail::read_big_endian<uint32_t>(buffer + OFF_SHARES);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.price = detail::read_big_endian<uint32_t>(buffer + OFF_PRICE);
            std::memcpy(msg.attribution.data(), buffer + OFF_ATTRIBUTION, 4);

            return msg;
        }
    };

    // ============================================================================
    // ORDER BOOK - MODIFY ORDERS
    // ============================================================================

    /// Order Executed (Type 'E') - Length: 31 bytes
    struct OrderExecuted {
        static constexpr MessageType TYPE = MessageType::ORDER_EXECUTED;
        static constexpr size_t SIZE = 31;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORDER_REFERENCE = 11;
        static constexpr size_t OFF_EXECUTED_SHARES = 19;
        static constexpr size_t OFF_MATCH_NUMBER = 23;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t order_reference;
        uint32_t executed_shares;
        uint64_t match_number;      // Unique trade ID

        static std::optional<OrderExecuted> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            OrderExecuted msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORDER_REFERENCE);
            msg.executed_shares = detail::read_big_endian<uint32_t>(buffer + OFF_EXECUTED_SHARES);
            msg.match_number = detail::read_big_endian<uint64_t>(buffer + OFF_MATCH_NUMBER);

            return msg;
        }
    };

    /// Order Executed with Price (Type 'C') - Length: 36 bytes
    struct OrderExecutedWithPrice {
        static constexpr MessageType TYPE = MessageType::ORDER_EXECUTED_WITH_PRICE;
        static constexpr size_t SIZE = 36;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORDER_REFERENCE = 11;
        static constexpr size_t OFF_EXECUTED_SHARES = 19;
        static constexpr size_t OFF_MATCH_NUMBER = 23;
        static constexpr size_t OFF_PRINTABLE = 31;
        static constexpr size_t OFF_EXECUTION_PRICE = 32;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t order_reference;
        uint32_t executed_shares;
        uint64_t match_number;
        char printable;             // 'Y' or 'N'
        uint32_t execution_price;

        Price get_price() const {
            return static_cast<Price>(execution_price);
        }

        static std::optional<OrderExecutedWithPrice> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            OrderExecutedWithPrice msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORDER_REFERENCE);
            msg.executed_shares = detail::read_big_endian<uint32_t>(buffer + OFF_EXECUTED_SHARES);
            msg.match_number = detail::read_big_endian<uint64_t>(buffer + OFF_MATCH_NUMBER);
            msg.printable = buffer[OFF_PRINTABLE];
            msg.execution_price = detail::read_big_endian<uint32_t>(buffer + OFF_EXECUTION_PRICE);

            return msg;
        }
    };

    /// Order Cancel (Type 'X') - Length: 23 bytes
    struct OrderCancel {
        static constexpr MessageType TYPE = MessageType::ORDER_CANCEL;
        static constexpr size_t SIZE = 23;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORDER_REFERENCE = 11;
        static constexpr size_t OFF_CANCELLED_SHARES = 19;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t order_reference;
        uint32_t cancelled_shares;

        static std::optional<OrderCancel> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            OrderCancel msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORDER_REFERENCE);
            msg.cancelled_shares = detail::read_big_endian<uint32_t>(buffer + OFF_CANCELLED_SHARES);

            return msg;
        }
    };

    /// Order Delete (Type 'D') - Length: 19 bytes
    struct OrderDelete {
        static constexpr MessageType TYPE = MessageType::ORDER_DELETE;
        static constexpr size_t SIZE = 19;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORDER_REFERENCE = 11;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t order_reference;

        static std::optional<OrderDelete> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            OrderDelete msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORDER_REFERENCE);

            return msg;
        }
    };

    /// Order Replace (Type 'U') - Length: 35 bytes
    struct OrderReplace {
        static constexpr MessageType TYPE = MessageType::ORDER_REPLACE;
        static constexpr size_t SIZE = 35;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORIGINAL_ORDER_REFERENCE = 11;
        static constexpr size_t OFF_NEW_ORDER_REFERENCE = 19;
        static constexpr size_t OFF_SHARES = 27;
        static constexpr size_t OFF_PRICE = 31;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t original_order_reference;
        uint64_t new_order_reference;
        uint32_t shares;
        uint32_t price;

        Price get_price() const {
            return static_cast<Price>(price);
        }

        static std::optional<OrderReplace> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            OrderReplace msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            msg.original_order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORIGINAL_ORDER_REFERENCE);
            msg.new_order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_NEW_ORDER_REFERENCE);
            msg.shares = detail::read_big_endian<uint32_t>(buffer + OFF_SHARES);
            msg.price = detail::read_big_endian<uint32_t>(buffer + OFF_PRICE);

            return msg;
        }
    };

    // ============================================================================
    // TRADE MESSAGES
    // ============================================================================

    /// Trade (Non-Cross) (Type 'P') - Length: 44 bytes
    struct TradeNonCross {
        static constexpr MessageType TYPE = MessageType::TRADE_NON_CROSS;
        static constexpr size_t SIZE = 44;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_ORDER_REFERENCE = 11;
        static constexpr size_t OFF_BUY_SELL_INDICATOR = 19;
        static constexpr size_t OFF_SHARES = 20;
        static constexpr size_t OFF_SYMBOL = 24;
        static constexpr size_t OFF_PRICE = 32;
        static constexpr size_t OFF_MATCH_NUMBER = 36;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t order_reference;
        char buy_sell_indicator;
        uint32_t shares;
        std::array<char, 8> symbol;         // 8 bytes
        uint32_t price;
        uint64_t match_number;

        Side side() const {
            return (buy_sell_indicator == 'B') ? Side::BUY : Side::SELL;
        }

        Price get_price() const {
            return static_cast<Price>(price);
        }

        std::string_view get_symbol() const {
            return detail::view_trimmed(symbol);
        }

        static std::optional<TradeNonCross> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            TradeNonCross msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            msg.order_reference = detail::read_big_endian<uint64_t>(buffer + OFF_ORDER_REFERENCE);
            msg.buy_sell_indicator = buffer[OFF_BUY_SELL_INDICATOR];
            msg.shares = detail::read_big_endian<uint32_t>(buffer + OFF_SHARES);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.price = detail::read_big_endian<uint32_t>(buffer + OFF_PRICE);
            msg.match_number = detail::read_big_endian<uint64_t>(buffer + OFF_MATCH_NUMBER);

            return msg;
        }
    };

    /// Trade (Cross) (Type 'Q') - Length: 35 bytes
    struct CrossTrade {
        static constexpr MessageType TYPE = MessageType::TRADE_CROSS;
        static constexpr size_t SIZE = 35;
        
        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SHARES = 11;
        static constexpr size_t OFF_SYMBOL = 19;
        static constexpr size_t OFF_CROSS_PRICE = 27;
        static constexpr size_t OFF_MATCH_NUMBER = 31;
        static constexpr size_t OFF_CROSS_TYPE = 39;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t shares;
        std::array<char, 8> symbol;
        uint32_t cross_price;
        uint64_t match_number;
        char cross_type;

        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }

        static std::optional<CrossTrade> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            CrossTrade msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            msg.shares = detail::read_big_endian<uint64_t>(buffer + OFF_SHARES);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.cross_price = detail::read_big_endian<uint32_t>(buffer + OFF_CROSS_PRICE);
            msg.match_number = detail::read_big_endian<uint64_t>(buffer + OFF_MATCH_NUMBER);
            msg.cross_type = buffer[OFF_CROSS_TYPE];
            return msg;
        }
    };

    /// Broken Trade (Type 'B') - Length: 19 bytes
    struct BrokenTrade {
        static constexpr MessageType TYPE = MessageType::BROKEN_TRADE;
        static constexpr size_t SIZE = 19;

        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_MATCH_NUMBER = 11;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t match_number;      // Trade to break

        static std::optional<BrokenTrade> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            BrokenTrade msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            msg.match_number = detail::read_big_endian<uint64_t>(buffer + OFF_MATCH_NUMBER);

            return msg;
        }
    };

    /// NOII (Type 'I') - Length: 49 bytes
    struct NOII {
        static constexpr MessageType TYPE = MessageType::NOII;
        static constexpr size_t SIZE = 50;
        
        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_PAIRED_SHARES = 11;
        static constexpr size_t OFF_IMBALANCE_SHARES = 19;
        static constexpr size_t OFF_IMBALANCE_DIRECTION = 27;
        static constexpr size_t OFF_SYMBOL = 28;
        static constexpr size_t OFF_FAR_PRICE = 36;
        static constexpr size_t OFF_NEAR_PRICE = 40;
        static constexpr size_t OFF_CURRENT_REFERENCE_PRICE = 44;
        static constexpr size_t OFF_CROSS_TYPE = 48;
        static constexpr size_t OFF_PRICE_VARIATION_INDICATOR = 49;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        uint64_t paired_shares;
        uint64_t imbalance_shares;
        char imbalance_direction;
        std::array<char, 8> symbol;
        uint32_t far_price;
        uint32_t near_price;
        uint32_t current_reference_price;
        char cross_type;
        char price_variation_indicator;

        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }

        static std::optional<NOII> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }

            NOII msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            msg.paired_shares = detail::read_big_endian<uint64_t>(buffer + OFF_PAIRED_SHARES);
            msg.imbalance_shares = detail::read_big_endian<uint64_t>(buffer + OFF_IMBALANCE_SHARES);
            msg.imbalance_direction = buffer[OFF_IMBALANCE_DIRECTION];
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.far_price = detail::read_big_endian<uint32_t>(buffer + OFF_FAR_PRICE);
            msg.near_price = detail::read_big_endian<uint32_t>(buffer + OFF_NEAR_PRICE);
            msg.current_reference_price = detail::read_big_endian<uint32_t>(buffer + OFF_CURRENT_REFERENCE_PRICE);
            msg.cross_type = buffer[OFF_CROSS_TYPE];
            msg.price_variation_indicator = buffer[OFF_PRICE_VARIATION_INDICATOR];
            return msg;
        }
    };

    /// RPII (Type 'N') - Length: 20 bytes
    struct RPII {
        static constexpr MessageType TYPE = MessageType::RPII;
        static constexpr size_t SIZE = 20;
        
        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_INTEREST_FLAG = 19;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;
        char interest_flag;

        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }

        static std::optional<RPII> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }
            
            RPII msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.interest_flag = buffer[OFF_INTEREST_FLAG];
            return msg;
        }
    };

    /// DLCR (Type 'O') - Direct Listing with Capital Raise Price Discovery - Length: 48 bytes
    struct DLCR {
        static constexpr MessageType TYPE = MessageType::DLCR;
        static constexpr size_t SIZE = 48;
        
        static constexpr size_t OFF_STOCK_LOCATE = 1;
        static constexpr size_t OFF_TRACKING_NUM = 3;
        static constexpr size_t OFF_TIMESTAMP = 5;
        static constexpr size_t OFF_SYMBOL = 11;
        static constexpr size_t OFF_OPEN_ELIGIBILITY_STATUS = 19;
        static constexpr size_t OFF_MIN_ALLOWABLE_PRICE = 20;
        static constexpr size_t OFF_MAX_ALLOWABLE_PRICE = 24;
        static constexpr size_t OFF_NEAR_EXECUTION_PRICE = 28;
        static constexpr size_t OFF_NEAR_EXECUTION_TIME = 32;
        static constexpr size_t OFF_LOWER_PRICE_RANGE_COLLAR = 40;
        static constexpr size_t OFF_UPPER_PRICE_RANGE_COLLAR = 44;

        uint16_t stock_locate;
        uint16_t tracking_number;
        uint64_t timestamp;         // 48-bit (6 bytes on wire)
        std::array<char, 8> symbol;
        char open_eligibility_status;  // 'N' = Not Eligible, 'Y' = Eligible
        uint32_t min_allowable_price;  // 20% below Registration Statement Lower Price
        uint32_t max_allowable_price;  // 80% above Registration Statement Highest Price
        uint32_t near_execution_price; // Current reference price when DLCR volatility test passed
        uint64_t near_execution_time;  // Time at which near execution price was set
        uint32_t lower_price_range_collar; // 10% below Near Execution Price
        uint32_t upper_price_range_collar; // 10% above Near Execution Price

        std::string_view get_symbol() const { return detail::view_trimmed(symbol); }

        static std::optional<DLCR> parse(const uint8_t* buffer, size_t length) {
            if (length != SIZE || buffer[0] != static_cast<uint8_t>(TYPE)) {
                return std::nullopt;
            }
            
            DLCR msg;
            msg.stock_locate = detail::read_big_endian<uint16_t>(buffer + OFF_STOCK_LOCATE);
            msg.tracking_number = detail::read_big_endian<uint16_t>(buffer + OFF_TRACKING_NUM);
            msg.timestamp = detail::read_be48(buffer + OFF_TIMESTAMP);  // 6 bytes (48-bit)
            std::memcpy(msg.symbol.data(), buffer + OFF_SYMBOL, 8);
            msg.open_eligibility_status = buffer[OFF_OPEN_ELIGIBILITY_STATUS];
            msg.min_allowable_price = detail::read_big_endian<uint32_t>(buffer + OFF_MIN_ALLOWABLE_PRICE);
            msg.max_allowable_price = detail::read_big_endian<uint32_t>(buffer + OFF_MAX_ALLOWABLE_PRICE);
            msg.near_execution_price = detail::read_big_endian<uint32_t>(buffer + OFF_NEAR_EXECUTION_PRICE);
            msg.near_execution_time = detail::read_big_endian<uint64_t>(buffer + OFF_NEAR_EXECUTION_TIME);
            msg.lower_price_range_collar = detail::read_big_endian<uint32_t>(buffer + OFF_LOWER_PRICE_RANGE_COLLAR);
            msg.upper_price_range_collar = detail::read_big_endian<uint32_t>(buffer + OFF_UPPER_PRICE_RANGE_COLLAR);
            return msg;
        }
    };

    // ============================================================================
    // MESSAGE VARIANT (Type-Safe Dispatch)
    // ============================================================================

    using ITCHMessage = std::variant<
        SystemEvent,
        StockDirectory,
        StockTradingAction,
        RegSHORestriction,
        MarketParticipantPosition,
        MWCBDeclineLevel,
        MWCBStatus,
        IPOQuotingPeriodUpdate,
        LULDAuctionCollar,
        AddOrder,
        AddOrderMPID,
        OrderExecuted,
        OrderExecutedWithPrice,
        OrderCancel,
        OrderDelete,
        OrderReplace,
        TradeNonCross,
        CrossTrade,
        BrokenTrade,
        NOII,
        RPII,
        DLCR
    >;

    /// Parse result with error information
    struct ParseResult {
        std::optional<ITCHMessage> message;
        ErrorCode error_code{ ErrorCode::SUCCESS };
        std::string error_detail;

        bool is_success() const { return message.has_value(); }

        static ParseResult ok(ITCHMessage msg) {
            return { std::move(msg), ErrorCode::SUCCESS, "" };
        }

        static ParseResult error(ErrorCode code, std::string detail) {
            return { std::nullopt, code, std::move(detail) };
        }
    };

    /// Parse ITCH message from buffer
    inline ParseResult parse_message(const uint8_t* buffer, size_t length) {
        // Validate minimum size
        if (length < 1) {
            return ParseResult::error(ErrorCode::PARSE_INVALID_SIZE,
                "Message too short (< 1 byte)");
        }

        // Extract message type
        MessageType type = static_cast<MessageType>(buffer[0]);

        // Dispatch to appropriate parser
        switch (type) {
        case MessageType::SYSTEM_EVENT:
            if (auto msg = SystemEvent::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::STOCK_DIRECTORY:
            if (auto msg = StockDirectory::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::STOCK_TRADING_ACTION:
            if (auto msg = StockTradingAction::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::REG_SHO_RESTRICTION:
            if (auto msg = RegSHORestriction::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::MARKET_PARTICIPANT_POSITION:
            if (auto msg = MarketParticipantPosition::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::MWCB_DECLINE_LEVEL:
            if (auto msg = MWCBDeclineLevel::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::MWCB_STATUS:
            if (auto msg = MWCBStatus::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::IPO_QUOTING_PERIOD_UPDATE:
            if (auto msg = IPOQuotingPeriodUpdate::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::LULD_AUCTION_COLLAR:
            if (auto msg = LULDAuctionCollar::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::ADD_ORDER:
            if (auto msg = AddOrder::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::ADD_ORDER_MPID:
            if (auto msg = AddOrderMPID::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::ORDER_EXECUTED:
            if (auto msg = OrderExecuted::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::ORDER_EXECUTED_WITH_PRICE:
            if (auto msg = OrderExecutedWithPrice::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::ORDER_CANCEL:
            if (auto msg = OrderCancel::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::ORDER_DELETE:
            if (auto msg = OrderDelete::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::ORDER_REPLACE:
            if (auto msg = OrderReplace::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::TRADE_NON_CROSS:
            if (auto msg = TradeNonCross::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::TRADE_CROSS:
            if (auto msg = CrossTrade::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::BROKEN_TRADE:
            if (auto msg = BrokenTrade::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::NOII:
            if (auto msg = NOII::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::RPII:
            if (auto msg = RPII::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        case MessageType::DLCR:
            if (auto msg = DLCR::parse(buffer, length)) return ParseResult::ok(ITCHMessage{*msg});
            break;
        default:
            return ParseResult::error(ErrorCode::PARSE_INVALID_TYPE,
                "Unknown message type: " + std::to_string(buffer[0]));
        }
        
        // If we reach here, the message type was recognized but parsing failed
        return ParseResult::error(ErrorCode::PARSE_INVALID_SIZE,
            "Message type " + std::to_string(buffer[0]) + " failed to parse (invalid size or format)");
    }

    // ============================================================================
    // MESSAGE VISITOR HELPERS
    // ============================================================================

    /// Extract timestamp from any ITCH message
    inline uint64_t get_timestamp(const ITCHMessage& msg) {
        return std::visit([](const auto& m) -> uint64_t {
            static_assert(requires { m.timestamp; }, "All message types must have a 'timestamp' member.");
            return m.timestamp;
            }, msg);
    }

    /// Extract stock locate from any ITCH message
    inline uint16_t get_stock_locate(const ITCHMessage& msg) {
        return std::visit([](const auto& m) -> uint16_t {
            static_assert(requires { m.stock_locate; }, "All message types must have a 'stock_locate' member.");
            return m.stock_locate;
            }, msg);
    }

    /// Get message type name as string
    inline const char* get_message_type_name(const ITCHMessage& msg) {
        return std::visit([](const auto& m) -> const char* {
            return message_type_to_string(std::remove_cvref_t<decltype(m)>::TYPE);
            }, msg);
    }

    /// Check if message is order book related
    inline bool is_order_book_message(const ITCHMessage& msg) {
        return std::visit([](const auto& m) -> bool {
            using T = std::remove_cvref_t<decltype(m)>;
            return std::is_same_v<T, AddOrder> ||
                std::is_same_v<T, AddOrderMPID> ||
                std::is_same_v<T, OrderExecuted> ||
                std::is_same_v<T, OrderExecutedWithPrice> ||
                std::is_same_v<T, OrderCancel> ||
                std::is_same_v<T, OrderDelete> ||
                std::is_same_v<T, OrderReplace>;
            }, msg);
    }

    /// Check if message is a trade
    inline bool is_trade_message(const ITCHMessage& msg) {
        return std::visit([](const auto& m) -> bool {
            using T = std::remove_cvref_t<decltype(m)>;
            return std::is_same_v<T, TradeNonCross> ||
                std::is_same_v<T, CrossTrade> ||
                std::is_same_v<T, BrokenTrade>;
            }, msg);
    }

    /// Check if message is system event
    inline bool is_system_message(const ITCHMessage& msg) {
        return std::visit([](const auto& m) -> bool {
            using T = std::remove_cvref_t<decltype(m)>;
            return std::is_same_v<T, SystemEvent> ||
                std::is_same_v<T, StockDirectory> ||
                std::is_same_v<T, StockTradingAction> ||
                std::is_same_v<T, RegSHORestriction> ||
                std::is_same_v<T, MarketParticipantPosition> ||
                std::is_same_v<T, MWCBDeclineLevel> ||
                std::is_same_v<T, MWCBStatus> ||
                std::is_same_v<T, IPOQuotingPeriodUpdate> ||
                std::is_same_v<T, LULDAuctionCollar> ||
                std::is_same_v<T, NOII> ||
                std::is_same_v<T, RPII> ||
                std::is_same_v<T, DLCR>;
            }, msg);
    }

    // ============================================================================
    // STATISTICS & METRICS
    // ============================================================================

    /// Message statistics for monitoring
    struct MessageStats {
        uint64_t total_messages{ 0 };
        uint64_t system_events{ 0 };
        uint64_t add_orders{ 0 };
        uint64_t executions{ 0 };
        uint64_t cancels{ 0 };
        uint64_t deletes{ 0 };
        uint64_t replaces{ 0 };
        uint64_t trades{ 0 };
        uint64_t parse_errors{ 0 };

        void record_message(const ITCHMessage& msg) {
            total_messages++;

            std::visit([this](const auto& m) {
                using T = std::remove_cvref_t<decltype(m)>;
                if constexpr (std::is_same_v<T, SystemEvent>) {
                    system_events++;
                }
                else if constexpr (std::is_same_v<T, AddOrder> ||
                    std::is_same_v<T, AddOrderMPID>) {
                    add_orders++;
                }
                else if constexpr (std::is_same_v<T, OrderExecuted> ||
                    std::is_same_v<T, OrderExecutedWithPrice>) {
                    executions++;
                }
                else if constexpr (std::is_same_v<T, OrderCancel>) {
                    cancels++;
                }
                else if constexpr (std::is_same_v<T, OrderDelete>) {
                    deletes++;
                }
                else if constexpr (std::is_same_v<T, OrderReplace>) {
                    replaces++;
                }
                else if constexpr (std::is_same_v<T, TradeNonCross>) {
                    trades++;
                }
                }, msg);
        }

        void record_error() {
            parse_errors++;
        }

        void print_summary() const {
            printf("=== ITCH Message Statistics ===\n");
            printf("Total messages:   %llu\n", total_messages);
            printf("System events:    %llu\n", system_events);
            printf("Add orders:       %llu\n", add_orders);
            printf("Executions:       %llu\n", executions);
            printf("Cancels:          %llu\n", cancels);
            printf("Deletes:          %llu\n", deletes);
            printf("Replaces:         %llu\n", replaces);
            printf("Trades:           %llu\n", trades);
            printf("Parse errors:     %llu\n", parse_errors);
            printf("==============================\n");
        }
    };

} // namespace hft::itch