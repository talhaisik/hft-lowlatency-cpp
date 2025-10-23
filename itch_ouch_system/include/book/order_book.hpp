#pragma once

#include "common/types.hpp"
#include "itch/messages.hpp"
#include "book/seqlock.hpp"
#include <vector>
#include <unordered_map>
#include <iostream>

namespace hft {

    /**
     * @class OrderBook
     * @brief A high-performance, single-threaded order book implementation.
     *
     * This order book is designed for low-latency processing of ITCH 5.0 messages.
     * It uses a "Dense Price Ladder" for O(1) access to price levels and a hash map
     * for O(1) access to individual orders.
     *
     * Key design principles:
     * - Single-writer, multiple-reader model (for future extension).
     * - No dynamic memory allocation in the hot path (message processing).
     * - All core data structures are pre-allocated.
     */
    class OrderBook {
    public:
        OrderBook(uint16_t stock_locate, const std::string& symbol);

        // --- Message Processing ---
        void add_order(const itch::AddOrder& msg);
        void execute_order(const itch::OrderExecuted& msg);
        void execute_order_with_price(const itch::OrderExecutedWithPrice& msg);
        void cancel_order(const itch::OrderCancel& msg);
        void delete_order(const itch::OrderDelete& msg);
        void replace_order(const itch::OrderReplace& msg);

        // --- Public Accessors ---

        // Returns a copy of the top-of-book data using the SeqLock.
        // This is safe to call from any thread.
        TopOfBook get_top_of_book() const;
        
        // Prints the current state of the order book (for debugging).
        void print_book() const;

    private:

        // Represents a single active order in the book.
        struct Order {
            Price price;
            uint32_t shares;
            Side side;
        };
        
        // --- Core Data Structures ---

        // "Dense Price Ladder" for bids and asks.
        // The index of the vector represents the price.
        std::vector<PriceLevel> bids_;
        std::vector<PriceLevel> asks_;

        // Hash map to track individual orders by their reference number.
        std::unordered_map<uint64_t, Order> orders_;

        // --- Top of Book Management ---
        
        // The current best bid and ask prices.
        Price best_bid_price_;
        Price best_ask_price_;

        // SeqLock to protect the TopOfBook data for concurrent reads.
        mutable SeqLock<TopOfBook> top_of_book_lock_;

        // Symbol information
        uint16_t stock_locate_;
        std::string symbol_;

        // --- Private Helper Functions ---
        void update_top_of_book();
    };

} // namespace hft
