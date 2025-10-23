#include "book/order_book.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace hft {

    // Prices are scaled by 10,000. A max price of $10,000.0000 gives us 100 million levels.
    // This may be too large for a std::vector on the stack. Let's use a more reasonable
    // upper limit for a typical stock, e.g., $2,000.0000.
    constexpr Price MAX_PRICE_LEVELS = 2000 * 10000;

    OrderBook::OrderBook(uint16_t stock_locate, const std::string& symbol)
        : stock_locate_(stock_locate),
          symbol_(symbol),
          bids_(MAX_PRICE_LEVELS, {0, 0, 0}),
          asks_(MAX_PRICE_LEVELS, {0, 0, 0}),
          best_bid_price_(0),
          best_ask_price_(MAX_PRICE_LEVELS - 1) {
        
        // Trim spaces from symbol to match ITCH message format
        size_t end = symbol_.find_last_not_of(' ');
        if (end != std::string::npos) {
            symbol_ = symbol_.substr(0, end + 1);
        }
        
        std::cout << "OrderBook for " << symbol_ << " initialized." << std::endl;
        update_top_of_book();
    }

    void OrderBook::add_order(const itch::AddOrder& msg) {
        // This check is important for a multi-symbol system
        // where a single OrderBookManager routes messages.
        if (msg.get_symbol() != symbol_) {
            return;
        }

        Price price = msg.get_price();
        if (price >= MAX_PRICE_LEVELS) {
            std::cerr << "Error: Price level " << price << " for symbol " << symbol_ 
                      << " is out of the book's configured range." << std::endl;
            return;
        }

        // Store the order details for future modifications (cancel, delete, replace)
        orders_[msg.order_reference] = {price, msg.shares, msg.side()};

        if (msg.side() == Side::BUY) {
            bids_[price].quantity += msg.shares;
            bids_[price].order_count++;
            if (price > best_bid_price_) {
                best_bid_price_ = price;
            }
        } else { // SELL
            asks_[price].quantity += msg.shares;
            asks_[price].order_count++;
            if (price < best_ask_price_) {
                best_ask_price_ = price;
            }
        }
        update_top_of_book();
    }
    
    void OrderBook::execute_order(const itch::OrderExecuted& msg) {
        auto it = orders_.find(msg.order_reference);
        if (it == orders_.end()) {
            return; // Order not found
        }

        Order& order = it->second;
        Price price = order.price;

        if (order.side == Side::BUY) {
            bids_[price].quantity -= msg.executed_shares;
        } else { // SELL
            asks_[price].quantity -= msg.executed_shares;
        }

        order.shares -= msg.executed_shares;
        
        // If order is fully executed, remove it
        if (order.shares == 0) {
            if (order.side == Side::BUY) {
                bids_[price].order_count--;
            } else { // SELL
                asks_[price].order_count--;
            }
            orders_.erase(it);
        }
        
        // Note: For simplicity, we don't update best bid/ask on execution here.
        // A real implementation would need to scan for the next best price if the
        // best price level is depleted. We'll add this logic later.
        update_top_of_book();
    }
    
    void OrderBook::execute_order_with_price(const itch::OrderExecutedWithPrice& msg) {
        // This message implies a trade happened at a specific price, which may or may
        // not be at the current best bid/ask. It's often used for hidden orders or crosses.
        // For our simple dense ladder, we'll handle it similarly to a standard execution,
        // but we'll use the price from the order map, assuming it's the correct one.
        auto it = orders_.find(msg.order_reference);
        if (it == orders_.end()) {
            return; // Order not found
        }

        Order& order = it->second;
        Price price = order.price;

        // Reduce quantity at the price level
        if (order.side == Side::BUY) {
            bids_[price].quantity -= msg.executed_shares;
        } else { // SELL
            asks_[price].quantity -= msg.executed_shares;
        }

        // Reduce shares in the specific order
        order.shares -= msg.executed_shares;
        
        // If order is fully executed, remove it
        if (order.shares == 0) {
            if (order.side == Side::BUY) {
                bids_[price].order_count--;
            } else { // SELL
                asks_[price].order_count--;
            }
            orders_.erase(it);
        }
        
        update_top_of_book();
    }

    void OrderBook::cancel_order(const itch::OrderCancel& msg) {
        auto it = orders_.find(msg.order_reference);
        if (it == orders_.end()) {
            return; // Order not found
        }

        Order& order = it->second;
        Price price = order.price;
        uint32_t cancelled_shares = msg.cancelled_shares;

        // Ensure we don't cancel more shares than the order has
        if (cancelled_shares > order.shares) {
            cancelled_shares = order.shares;
        }

        if (order.side == Side::BUY) {
            bids_[price].quantity -= cancelled_shares;
        } else { // SELL
            asks_[price].quantity -= cancelled_shares;
        }

        order.shares -= cancelled_shares;
        
        if (order.shares == 0) {
            if (order.side == Side::BUY) {
                bids_[price].order_count--;
            } else { // SELL
                asks_[price].order_count--;
            }
            orders_.erase(it);
        }
        
        update_top_of_book();
    }

    void OrderBook::delete_order(const itch::OrderDelete& msg) {
        auto it = orders_.find(msg.order_reference);
        if (it == orders_.end()) {
            return; // Order not found
        }

        const Order& order = it->second;
        Price price = order.price;

        if (order.side == Side::BUY) {
            bids_[price].quantity -= order.shares;
            bids_[price].order_count--;
        } else { // SELL
            asks_[price].quantity -= order.shares;
            asks_[price].order_count--;
        }

        orders_.erase(it);

        // This is a simplification. A real implementation needs to handle the
        // case where the deleted order was at the best bid or ask.
        update_top_of_book();
    }

    void OrderBook::replace_order(const itch::OrderReplace& msg) {
        // A replace is effectively an atomic cancel + add.
        // 1. Find the old order and remove its quantity from the book.
        auto it = orders_.find(msg.original_order_reference);
        if (it == orders_.end()) {
            // If the original order doesn't exist, we can't replace it.
            // A more robust implementation might treat this as a new add,
            // but for now, we'll just ignore it.
            return;
        }

        const Order old_order = it->second;
        Price old_price = old_order.price;

        if (old_order.side == Side::BUY) {
            bids_[old_price].quantity -= old_order.shares;
            bids_[old_price].order_count--;
        } else { // SELL
            asks_[old_price].quantity -= old_order.shares;
            asks_[old_price].order_count--;
        }
        orders_.erase(it);

        // 2. Add the new order.
        Price new_price = msg.get_price();
        if (new_price >= MAX_PRICE_LEVELS) {
            std::cerr << "Error: Replace order price " << new_price << " for symbol " << symbol_
                      << " is out of range." << std::endl;
            // Note: We've already removed the old order. A production system would
            // need a strategy for this scenario (e.g., reject, or add back old order).
            // For now, we proceed, leaving the old order removed.
            update_top_of_book();
            return;
        }

        Side side = old_order.side; // Side is not in replace message, must be inferred
        orders_[msg.new_order_reference] = {new_price, msg.shares, side};

        if (side == Side::BUY) {
            bids_[new_price].quantity += msg.shares;
            bids_[new_price].order_count++;
            if (new_price > best_bid_price_) {
                best_bid_price_ = new_price;
            }
        } else { // SELL
            asks_[new_price].quantity += msg.shares;
            asks_[new_price].order_count++;
            if (new_price < best_ask_price_) {
                best_ask_price_ = new_price;
            }
        }

        update_top_of_book();
    }

    TopOfBook OrderBook::get_top_of_book() const {
        return top_of_book_lock_.read();
    }
    
    void OrderBook::update_top_of_book() {
        // Scan the entire price range to find the best bid (highest price with quantity > 0)
        Price new_best_bid = 0;
        for (Price p = 0; p < MAX_PRICE_LEVELS; ++p) {
            if (bids_[p].quantity > 0) {
                new_best_bid = p; // Keep updating to find the highest
            }
        }
        best_bid_price_ = new_best_bid;
        
        // Scan the entire price range to find the best ask (lowest price with quantity > 0)
        Price new_best_ask = MAX_PRICE_LEVELS - 1;
        for (Price p = 0; p < MAX_PRICE_LEVELS; ++p) {
            if (asks_[p].quantity > 0) {
                new_best_ask = p; // Take the first (lowest) non-zero ask
                break;
            }
        }
        best_ask_price_ = new_best_ask;

        top_of_book_lock_.write({
            best_bid_price_,
            bids_[best_bid_price_].quantity,
            best_ask_price_,
            asks_[best_ask_price_].quantity
        });
    }

    void OrderBook::print_book() const {
        // 1. Get a consistent snapshot of the top of book.
        TopOfBook tob = get_top_of_book();
        
        // 2. Print the summary header.
        std::cout << "--- Order Book for " << symbol_ << " ---\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Best Ask: " << static_cast<double>(tob.ask_price) / 10000.0 
                  << " (Qty: " << tob.ask_quantity << ")\n";
        std::cout << "Best Bid:  " << static_cast<double>(tob.bid_price) / 10000.0
                  << " (Qty: " << tob.bid_quantity << ")\n";
                  
        std::cout << "--- Top 5 Levels ---\n";
        
        // 3. Print Ask levels, iterating from the snapshot's ask price.
        std::cout << "ASKS:\n";
        int count = 0;
        if (tob.ask_quantity > 0) {
            for (Price p = tob.ask_price; p < MAX_PRICE_LEVELS && count < 5; ++p) {
                if (asks_[p].quantity > 0) {
                    std::cout << "  " << std::setw(10) << static_cast<double>(p) / 10000.0
                              << "\t" << std::setw(8) << asks_[p].quantity 
                              << " (" << asks_[p].order_count << ")\n";
                    count++;
                }
            }
        }
        
        // 4. Print Bid levels, iterating from the snapshot's bid price.
        std::cout << "BIDS:\n";
        count = 0;
        if (tob.bid_quantity > 0) {
            for (Price p = tob.bid_price; p > 0 && count < 5; --p) {
                if (bids_[p].quantity > 0) {
                    std::cout << "  " << std::setw(10) << static_cast<double>(p) / 10000.0
                              << "\t" << std::setw(8) << bids_[p].quantity 
                              << " (" << bids_[p].order_count << ")\n";
                    count++;
                }
            }
        }
        std::cout << "---------------------\n";
    }

} // namespace hft
