#pragma once
// include/common/types.hpp
//
// Fundamental types used across the ITCH/OUCH trading system.
// All components depend on these definitions for consistency.

#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>
#include <array>

namespace hft {

// ============================================================================
// BASIC TYPES
// ============================================================================

/// Symbol representation (e.g., "AAPL", "MSFT")
using Symbol = std::string;

/// Price in 1/10,000 dollar increments
/// Example: $150.2534 = 1,502,534
using Price = int64_t;

/// Quantity of shares
using Quantity = uint32_t;

/// Order identifier (exchange-assigned)
using OrderId = uint64_t;

/// Timestamp in nanoseconds (high-resolution)
using Timestamp = std::chrono::nanoseconds;

/// Wall-clock time point
using TimePoint = std::chrono::system_clock::time_point;

// ============================================================================
// PRICE UTILITIES
// ============================================================================

/// Price precision (4 decimal places)
constexpr int PRICE_DECIMALS = 4;
constexpr Price PRICE_MULTIPLIER = 10'000;

/// Convert dollars to internal price representation
constexpr Price to_price(double dollars) {
    return static_cast<Price>(dollars * PRICE_MULTIPLIER);
}

/// Convert internal price to dollars
constexpr double to_dollars(Price price) {
    return static_cast<double>(price) / static_cast<double>(PRICE_MULTIPLIER);
}

/// Minimum price increment (1 tick = $0.0001)
constexpr Price MIN_PRICE_INCREMENT = 1;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/// Order side
enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

inline const char* side_to_string(Side side) {
    return (side == Side::BUY) ? "BUY" : "SELL";
}

inline char side_to_char(Side side) {
    return (side == Side::BUY) ? 'B' : 'S';
}

/// Order type
enum class OrderType : uint8_t {
    MARKET = 0,
    LIMIT = 1,
    STOP = 2,
    STOP_LIMIT = 3
};

/// Order status
enum class OrderStatus : uint8_t {
    PENDING_NEW = 0,
    ACCEPTED = 1,
    PARTIAL_FILL = 2,
    FILLED = 3,
    CANCELED = 4,
    REJECTED = 5,
    EXPIRED = 6
};

inline const char* status_to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING_NEW: return "PENDING_NEW";
        case OrderStatus::ACCEPTED: return "ACCEPTED";
        case OrderStatus::PARTIAL_FILL: return "PARTIAL_FILL";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELED: return "CANCELED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::EXPIRED: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

/// Time in force
enum class TimeInForce : uint8_t {
    DAY = 0,        // Good for day
    GTC = 1,        // Good till cancel
    IOC = 2,        // Immediate or cancel
    FOK = 3         // Fill or kill
};

/// Execution type (ITCH/OUCH)
enum class ExecType : char {
    NEW = '0',
    PARTIAL_FILL = '1',
    FILL = '2',
    CANCELED = '4',
    REPLACED = '5',
    REJECTED = '8',
    EXPIRED = 'C'
};

/// Market status (from ITCH System Event)
enum class MarketStatus : char {
    PRE_OPEN = 'P',
    OPEN = 'O',
    CLOSED = 'C',
    HALTED = 'H'
};

// ============================================================================
// STRUCTURES
// ============================================================================

/// Basic order representation
struct Order {
    OrderId id{0};
    Symbol symbol;
    Side side{Side::BUY};
    Price price{0};
    Quantity quantity{0};
    Quantity filled_quantity{0};
    OrderStatus status{OrderStatus::PENDING_NEW};
    Timestamp timestamp{0};
    
    /// Check if order is active (can be modified/canceled)
    bool is_active() const {
        return status == OrderStatus::ACCEPTED || 
               status == OrderStatus::PARTIAL_FILL;
    }
    
    /// Get remaining quantity
    Quantity remaining() const {
        return quantity - filled_quantity;
    }
    
    /// Check if fully filled
    bool is_filled() const {
        return filled_quantity >= quantity;
    }
};

/// Trade/Execution representation
struct Trade {
    OrderId order_id{0};
    Symbol symbol;
    Price price{0};
    Quantity quantity{0};
    Side side{Side::BUY};
    Timestamp timestamp{0};
};

/// Top of book (best bid and ask)
struct TopOfBook {
    Price bid_price{0};
    Quantity bid_quantity{0};
    Price ask_price{0};
    Quantity ask_quantity{0};
    
    /// Calculate mid-price
    Price mid_price() const {
        if (bid_price == 0 || ask_price == 0) return 0;
        return (bid_price + ask_price) / 2;
    }
    
    /// Calculate spread
    Price spread() const {
        if (bid_price == 0 || ask_price == 0) return 0;
        return ask_price - bid_price;
    }
    
    /// Calculate spread in basis points
    double spread_bps() const {
        Price mid = mid_price();
        if (mid == 0) return 0.0;
        return (static_cast<double>(spread()) / mid) * 10'000.0;
    }
    
    /// Check if book is crossed (error condition)
    bool is_crossed() const {
        return bid_price > 0 && ask_price > 0 && bid_price >= ask_price;
    }
    
    /// Check if book is empty
    bool is_empty() const {
        return bid_quantity == 0 && ask_quantity == 0;
    }
};

/// Price level (for order book depth)
struct PriceLevel {
    Price price{0};
    Quantity quantity{0};
    uint32_t order_count{0};
};

/// Symbol specification (tick tables)
struct SymbolSpec {
    Symbol symbol;
    Price tick_size{1};           // Minimum price increment
    Price min_price{0};            // Minimum valid price
    Price max_price{999'999'999};  // Maximum valid price
    uint32_t lot_size{1};          // Minimum order size
    uint16_t stock_locate{0};      // ITCH stock locate code
    
    /// Validate price against tick size
    bool is_valid_price(Price price) const {
        if (price < min_price || price > max_price) return false;
        return (price % tick_size) == 0;
    }
    
    /// Round price to nearest valid tick
    Price round_to_tick(Price price) const {
        return (price / tick_size) * tick_size;
    }
};

// ============================================================================
// RESULT TYPE (for error handling)
// ============================================================================

/// Result type for operations that can fail
template<typename T>
struct Result {
    T value{};
    bool success{false};
    std::string error_message;
    
    /// Create successful result
    static Result<T> ok(T val) {
        return {std::move(val), true, ""};
    }
    
    /// Create error result
    static Result<T> error(std::string msg) {
        return {T{}, false, std::move(msg)};
    }
    
    /// Check if successful
    explicit operator bool() const { return success; }
    
    /// Get value (throws if error)
    const T& get() const {
        if (!success) {
            throw std::runtime_error("Result::get() called on error: " + error_message);
        }
        return value;
    }
};

// ============================================================================
// ERROR CODES
// ============================================================================

enum class ErrorCode : uint16_t {
    SUCCESS = 0,
    
    // Parse errors
    PARSE_INVALID_SIZE = 100,
    PARSE_INVALID_TYPE = 101,
    PARSE_INVALID_CHECKSUM = 102,
    PARSE_CORRUPT_DATA = 103,
    
    // Order errors
    ORDER_INVALID_SYMBOL = 200,
    ORDER_INVALID_PRICE = 201,
    ORDER_INVALID_QUANTITY = 202,
    ORDER_NOT_FOUND = 203,
    
    // Risk errors
    RISK_INSUFFICIENT_FUNDS = 300,
    RISK_POSITION_LIMIT = 301,
    RISK_RATE_LIMIT = 302,
    RISK_PRICE_COLLAR = 303,
    RISK_MARKET_CLOSED = 304,
    RISK_SSR_VIOLATION = 305,
    
    // Network errors
    NETWORK_DISCONNECTED = 400,
    NETWORK_TIMEOUT = 401,
    NETWORK_SEQUENCE_GAP = 402,
    
    // System errors
    SYSTEM_INTERNAL_ERROR = 500,
    SYSTEM_OUT_OF_MEMORY = 501,
};

inline const char* error_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::PARSE_INVALID_SIZE: return "PARSE_INVALID_SIZE";
        case ErrorCode::PARSE_INVALID_TYPE: return "PARSE_INVALID_TYPE";
        case ErrorCode::ORDER_INVALID_SYMBOL: return "ORDER_INVALID_SYMBOL";
        case ErrorCode::RISK_POSITION_LIMIT: return "RISK_POSITION_LIMIT";
        case ErrorCode::NETWORK_DISCONNECTED: return "NETWORK_DISCONNECTED";
        default: return "UNKNOWN_ERROR";
    }
}

// ============================================================================
// CONSTANTS
// ============================================================================

namespace constants {

/// Maximum values
constexpr Price MAX_PRICE = 999'999'999;      // $99,999.9999
constexpr Quantity MAX_QUANTITY = 999'999'999;

/// Message sizes (for validation)
constexpr size_t OUCH_MAX_MESSAGE_SIZE = 128;
constexpr size_t ITCH_MAX_MESSAGE_SIZE = 128;

/// Session parameters
constexpr uint16_t DEFAULT_HEARTBEAT_INTERVAL_SEC = 1;
constexpr uint16_t DEFAULT_RECONNECT_DELAY_MS = 1000;

/// Performance tuning
constexpr size_t DEFAULT_RING_BUFFER_SIZE = 4096;
constexpr size_t DEFAULT_MESSAGE_POOL_SIZE = 10000;
constexpr size_t CACHE_LINE_SIZE = 64;

} // namespace constants

// ============================================================================
// FORMATTING UTILITIES
// ============================================================================

/// Format price as string with proper decimal places
inline std::string format_price(Price price) {
    double dollars = to_dollars(price);
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "$%.4f", dollars);
    return buffer;
}

/// Format quantity as string with thousand separators
inline std::string format_quantity(Quantity qty) {
    if (qty < 1000) return std::to_string(qty);
    
    std::string result;
    std::string num = std::to_string(qty);
    int count = 0;
    
    for (auto it = num.rbegin(); it != num.rend(); ++it) {
        if (count > 0 && count % 3 == 0) {
            result.insert(0, 1, ',');
        }
        result.insert(0, 1, *it);
        count++;
    }
    
    return result;
}

/// Format timestamp as ISO 8601 string
inline std::string format_timestamp(Timestamp ts) {
    auto tp = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(ts)
    );
    auto time_t = std::chrono::system_clock::to_time_t(tp);

    // Use localtime_s on Windows (thread-safe), localtime_r on POSIX
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    return buffer;
}

// ============================================================================
// STRING UTILITIES
// ============================================================================

/// Trim whitespace from both ends
inline std::string trim(std::string_view str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string_view::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return std::string(str.substr(start, end - start + 1));
}

/// Copy string with padding
inline void copy_padded(char* dest, size_t dest_size, 
                       std::string_view src, char pad = ' ') {
    size_t copy_len = std::min(src.size(), dest_size);
    if (copy_len > 0) {
        std::memcpy(dest, src.data(), copy_len);
    }
    if (copy_len < dest_size) {
        std::memset(dest + copy_len, pad, dest_size - copy_len);
    }
}

// ============================================================================
// ALIGNMENT UTILITIES
// ============================================================================

/// Align pointer to cache line boundary
template<typename T>
inline T* align_to_cache_line(T* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned = (addr + constants::CACHE_LINE_SIZE - 1) 
                       & ~(constants::CACHE_LINE_SIZE - 1);
    return reinterpret_cast<T*>(aligned);
}

/// Check if pointer is cache-line aligned
template<typename T>
inline bool is_cache_aligned(T* ptr) {
    return (reinterpret_cast<uintptr_t>(ptr) % constants::CACHE_LINE_SIZE) == 0;
}

} // namespace hft