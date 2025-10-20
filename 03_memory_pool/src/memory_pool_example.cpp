#include <iostream>
#include <string>
#include <vector>
#include "memory_pool.hpp"

using namespace hft::memory;

// Example 1: Simple struct for trading orders
struct Order {
    size_t order_id;
    double price;
    size_t quantity;
    std::string symbol;

    Order(size_t id, double p, size_t q, std::string s)
        : order_id(id), price(p), quantity(q), symbol(std::move(s)) {
        std::cout << "Order " << order_id << " constructed\n";
    }

    ~Order() {
        std::cout << "Order " << order_id << " destructed\n";
    }

    void print() const {
        std::cout << "Order #" << order_id << ": "
            << symbol << " " << quantity << "@" << price << "\n";
    }
};

// Example 2: Manual allocation/deallocation
void example_manual_usage() {
    std::cout << "\n=== Example 1: Manual Allocation ===\n";

    // Create pool for 10 orders
    MemoryPool<Order, 10> order_pool;

    std::cout << "Pool capacity: " << order_pool.capacity() << "\n";
    std::cout << "Available: " << order_pool.available() << "\n\n";

    // Allocate some orders
    std::vector<Order*> orders;

    orders.push_back(order_pool.allocate(1, 100.50, 100, "AAPL"));
    orders.push_back(order_pool.allocate(2, 200.75, 50, "GOOGL"));
    orders.push_back(order_pool.allocate(3, 50.25, 200, "MSFT"));

    std::cout << "\nAfter allocation:\n";
    std::cout << "Available: " << order_pool.available() << "\n\n";

    // Use orders
    for (auto* order : orders) {
        order->print();
    }

    // Deallocate
    std::cout << "\nDeallocating...\n";
    for (auto* order : orders) {
        order_pool.deallocate(order);
    }

    std::cout << "\nAfter deallocation:\n";
    std::cout << "Available: " << order_pool.available() << "\n";
}

// Example 3: RAII with PooledPtr
void example_raii_usage() {
    std::cout << "\n=== Example 2: RAII with PooledPtr ===\n";

    MemoryPool<Order, 10> order_pool;

    {
        std::cout << "Creating scoped orders...\n";

        auto order1 = make_pooled<Order>(order_pool, 10, 150.25, 75, "TSLA");
        auto order2 = make_pooled<Order>(order_pool, 11, 300.50, 25, "NVDA");

        order1->print();
        order2->print();

        std::cout << "\nAvailable in pool: " << order_pool.available() << "\n";
        std::cout << "Leaving scope...\n";
    } // Orders automatically returned to pool here!

    std::cout << "\nAfter scope exit:\n";
    std::cout << "Available in pool: " << order_pool.available() << "\n";
}

// Example 4: Pool exhaustion handling
void example_exhaustion() {
    std::cout << "\n=== Example 3: Pool Exhaustion ===\n";

    // Small pool for demonstration
    MemoryPool<Order, 3> small_pool;

    std::vector<Order*> orders;

    // Try to allocate 5 orders (pool only has 3 slots)
    for (int i = 0; i < 5; ++i) {
        auto* order = small_pool.allocate(i, 100.0, 10, "TEST");

        if (order) {
            std::cout << "Allocated order " << i << "\n";
            orders.push_back(order);
        }
        else {
            std::cout << "Failed to allocate order " << i
                << " - pool exhausted!\n";
        }
    }

    std::cout << "\nAllocated: " << orders.size() << " orders\n";
    std::cout << "Available: " << small_pool.available() << "\n";

    // Clean up
    for (auto* order : orders) {
        small_pool.deallocate(order);
    }
}

// Example 5: Reuse demonstration
void example_reuse() {
    std::cout << "\n=== Example 4: Memory Reuse ===\n";

    MemoryPool<Order, 5> pool;

    // Allocate and deallocate multiple times
    for (int round = 0; round < 3; ++round) {
        std::cout << "\nRound " << round + 1 << ":\n";

        auto order = make_pooled<Order>(pool, round * 100, 50.0, 10, "REUSE");
        order->print();

        std::cout << "Available: " << pool.available() << "\n";
    }

    std::cout << "\nNote: Same memory reused 3 times!\n";
    std::cout << "Final available: " << pool.available() << "\n";
}

// Example 6: HFT-style usage pattern
void example_hft_pattern() {
    std::cout << "\n=== Example 5: HFT Usage Pattern ===\n";

    constexpr size_t MAX_ORDERS = 1000;
    MemoryPool<Order, MAX_ORDERS> order_pool;

    // Simulate incoming market data
    std::vector<Order*> active_orders;

    std::cout << "Simulating order flow...\n";

    // Incoming orders
    for (int i = 0; i < 10; ++i) {
        auto* order = order_pool.allocate(
            i,
            100.0 + i * 0.5,
            100 - i * 5,
            "SYM" + std::to_string(i)
        );

        if (order) {
            active_orders.push_back(order);
        }
    }

    std::cout << "Active orders: " << active_orders.size() << "\n";
    std::cout << "Pool available: " << order_pool.available()
        << "/" << order_pool.capacity() << "\n";

    // Process some orders (e.g., filled)
    std::cout << "\nProcessing first 5 orders...\n";
    for (int i = 0; i < 5 && !active_orders.empty(); ++i) {
        Order* order = active_orders.back();
        active_orders.pop_back();

        // Process...
        order->print();

        // Return to pool
        order_pool.deallocate(order);
    }

    std::cout << "\nAfter processing:\n";
    std::cout << "Active orders: " << active_orders.size() << "\n";
    std::cout << "Pool available: " << order_pool.available()
        << "/" << order_pool.capacity() << "\n";

    // Clean up remaining
    for (auto* order : active_orders) {
        order_pool.deallocate(order);
    }
}

int main() {
    try {
        example_manual_usage();
        example_raii_usage();
        example_exhaustion();
        example_reuse();
        example_hft_pattern();

        std::cout << "\n=== All Examples Completed Successfully ===\n";
        return 0;

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}