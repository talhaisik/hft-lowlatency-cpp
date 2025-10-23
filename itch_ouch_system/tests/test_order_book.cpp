#include "book/order_book.hpp"
#include "itch/messages.hpp"
#include <cassert>
#include <iostream>

using namespace hft;
using namespace hft::itch;

// Helper function to create an AddOrder message
AddOrder create_add_order(uint64_t ref, char side, uint32_t shares, const char* symbol_str, uint32_t price) {
    AddOrder msg{};
    msg.order_reference = ref;
    msg.buy_sell_indicator = side;
    msg.shares = shares;
    std::memcpy(msg.symbol.data(), symbol_str, 8);
    msg.price = price;
    return msg;
}

void test_order_book_add_orders() {
    std::cout << "\n=== Test: OrderBook Add Orders ===\n";

    OrderBook book(1, "MSFT    ");

    // --- Test 1: Add a bid ---
    auto bid1 = create_add_order(101, 'B', 100, "MSFT    ", 1500000); // $150.0000
    book.add_order(bid1);
    
    TopOfBook tob1 = book.get_top_of_book();
    assert(tob1.bid_price == 1500000);
    assert(tob1.bid_quantity == 100);
    std::cout << "[OK] Added first bid correctly\n";
    book.print_book();

    // --- Test 2: Add a higher bid ---
    auto bid2 = create_add_order(102, 'B', 200, "MSFT    ", 1500100); // $150.0100
    book.add_order(bid2);

    TopOfBook tob2 = book.get_top_of_book();
    assert(tob2.bid_price == 1500100);
    assert(tob2.bid_quantity == 200);
    std::cout << "[OK] Added higher bid correctly, BBO updated\n";
    book.print_book();
    
    // --- Test 3: Add an ask ---
    auto ask1 = create_add_order(201, 'S', 150, "MSFT    ", 1500500); // $150.0500
    book.add_order(ask1);

    TopOfBook tob3 = book.get_top_of_book();
    assert(tob3.bid_price == 1500100);
    assert(tob3.bid_quantity == 200);
    assert(tob3.ask_price == 1500500);
    assert(tob3.ask_quantity == 150);
    std::cout << "[OK] Added first ask correctly\n";
    book.print_book();
    
    // --- Test 4: Add a lower ask ---
    auto ask2 = create_add_order(202, 'S', 250, "MSFT    ", 1500400); // $150.0400
    book.add_order(ask2);
    
    TopOfBook tob4 = book.get_top_of_book();
    assert(tob4.ask_price == 1500400);
    assert(tob4.ask_quantity == 250);
    std::cout << "[OK] Added lower ask correctly, BBO updated\n";
    book.print_book();
    
    // --- Test 5: Add to existing bid level ---
    auto bid3 = create_add_order(103, 'B', 50, "MSFT    ", 1500100); // $150.0100
    book.add_order(bid3);

    TopOfBook tob5 = book.get_top_of_book();
    assert(tob5.bid_price == 1500100);
    assert(tob5.bid_quantity == 250); // 200 + 50
    std::cout << "[OK] Added to existing best bid level\n";
    book.print_book();
}

void test_order_book_executes_and_deletes() {
    std::cout << "\n=== Test: OrderBook Executes and Deletes ===\n";

    OrderBook book(1, "MSFT    ");

    // Setup the book
    book.add_order(create_add_order(101, 'B', 100, "MSFT    ", 1500000));
    book.add_order(create_add_order(102, 'B', 200, "MSFT    ", 1500100)); // Best bid
    book.add_order(create_add_order(201, 'S', 150, "MSFT    ", 1500500));
    book.add_order(create_add_order(202, 'S', 250, "MSFT    ", 1500400)); // Best ask
    
    std::cout << "Initial book state:\n";
    book.print_book();

    // --- Test 1: Partial execution of best bid ---
    OrderExecuted exec1{};
    exec1.order_reference = 102; // Execute against the best bid (200 @ 150.0100)
    exec1.executed_shares = 50;
    book.execute_order(exec1);

    TopOfBook tob1 = book.get_top_of_book();
    assert(tob1.bid_price == 1500100);
    assert(tob1.bid_quantity == 150); // 200 - 50
    std::cout << "[OK] Partial execution of best bid\n";
    book.print_book();

    // --- Test 2: Delete best ask order ---
    OrderDelete del1{};
    del1.order_reference = 202; // Delete the best ask (250 @ 150.0400)
    book.delete_order(del1);

    TopOfBook tob2 = book.get_top_of_book();
    assert(tob2.ask_price == 1500500); // Best ask should fall back
    assert(tob2.ask_quantity == 150);
    std::cout << "[OK] Deleted best ask, BBO updated\n";
    book.print_book();

    // --- Test 3: Full execution of remaining best bid ---
    OrderExecuted exec2{};
    exec2.order_reference = 102; // Execute the rest of the best bid
    exec2.executed_shares = 150;
    book.execute_order(exec2);
    
    TopOfBook tob3 = book.get_top_of_book();
    assert(tob3.bid_price == 1500000); // Best bid should fall back
    assert(tob3.bid_quantity == 100);
    std::cout << "[OK] Full execution of best bid, BBO updated\n";
    book.print_book();
}

void test_order_book_cancel_replace() {
    std::cout << "\n=== Test: OrderBook Cancel and Replace ===\n";

    OrderBook book(1, "MSFT    ");

    // Setup the book
    book.add_order(create_add_order(101, 'B', 100, "MSFT    ", 1500000));
    book.add_order(create_add_order(102, 'B', 200, "MSFT    ", 1500100)); // Best bid
    book.add_order(create_add_order(201, 'S', 150, "MSFT    ", 1500500));
    book.add_order(create_add_order(202, 'S', 250, "MSFT    ", 1500400)); // Best ask
    
    std::cout << "Initial book state:\n";
    book.print_book();
    
    // --- Test 1: Partial cancel of a non-BBO bid ---
    OrderCancel cancel1{};
    cancel1.order_reference = 101;
    cancel1.cancelled_shares = 30;
    book.cancel_order(cancel1);

    // BBO should be unchanged
    TopOfBook tob1 = book.get_top_of_book();
    assert(tob1.bid_price == 1500100);
    assert(tob1.bid_quantity == 200);
    std::cout << "[OK] Partial cancel of non-BBO order\n";
    book.print_book();
    
    // --- Test 2: Replace best ask with a lower price and new size ---
    OrderReplace replace1{};
    replace1.original_order_reference = 202; // 250 @ 150.0400
    replace1.new_order_reference = 301;
    replace1.shares = 300;
    replace1.price = 1500300; // New best ask: $150.0300
    book.replace_order(replace1);

    TopOfBook tob2 = book.get_top_of_book();
    assert(tob2.ask_price == 1500300);
    assert(tob2.ask_quantity == 300);
    std::cout << "[OK] Replaced best ask with new price/size\n";
    book.print_book();
}

int main() {
    test_order_book_add_orders();
    test_order_book_executes_and_deletes();
    test_order_book_cancel_replace();
    std::cout << "\nAll OrderBook tests passed!\n";
    return 0;
}
