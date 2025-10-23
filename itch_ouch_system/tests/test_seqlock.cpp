#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>
#include "book/seqlock.hpp"

// A sample data structure to be protected by the SeqLock.
// It's important that this is trivially copyable.
struct TopOfBook {
    uint64_t bid_price;
    uint64_t ask_price;
    uint64_t bid_qty;
    uint64_t ask_qty;

    bool operator==(const TopOfBook& other) const {
        return bid_price == other.bid_price && ask_price == other.ask_price &&
               bid_qty == other.bid_qty && ask_qty == other.ask_qty;
    }
};

void test_seqlock_basic_read_write() {
    std::cout << "\n=== Test: SeqLock Basic Read/Write ===\n";

    hft::SeqLock<TopOfBook> tob_seqlock;
    
    // Initial read
    TopOfBook initial_data = tob_seqlock.read();
    assert(initial_data.bid_price == 0); // Should be default initialized
    std::cout << "[OK] Initial read is default-initialized\n";

    // First write
    TopOfBook data1 = {10000, 10001, 500, 500};
    tob_seqlock.write(data1);
    
    // Read after first write
    TopOfBook read_data1 = tob_seqlock.read();
    assert(read_data1 == data1);
    std::cout << "[OK] Read after first write is correct\n";

    // Second write
    TopOfBook data2 = {10002, 10003, 200, 300};
    tob_seqlock.write(data2);

    // Read after second write
    TopOfBook read_data2 = tob_seqlock.read();
    assert(read_data2 == data2);
    assert(!(read_data2 == data1));
    std::cout << "[OK] Read after second write is correct\n";
}

void test_seqlock_concurrent_read_write() {
    std::cout << "\n=== Test: SeqLock Concurrent Read/Write ===\n";
    
    hft::SeqLock<TopOfBook> tob_seqlock;
    std::atomic<bool> running = true;
    std::atomic<uint64_t> total_reads = 0;

    // --- Writer Thread ---
    std::thread writer_thread([&]() {
        uint64_t i = 1;
        while (running) {
            tob_seqlock.write({i, i + 1, i * 10, (i + 1) * 10});
            i++;
        }
    });

    // --- Reader Threads ---
    std::vector<std::thread> reader_threads;
    const int num_readers = std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1;
    std::cout << "Spawning " << num_readers << " reader threads...\n";

    for (int i = 0; i < num_readers; ++i) {
        reader_threads.emplace_back([&]() {
            uint64_t reads = 0;
            uint64_t last_price = 0;
            while (running) {
                TopOfBook tob = tob_seqlock.read();
                reads++;

                // Core consistency check: a torn read would likely fail this.
                assert(tob.bid_price + 1 == tob.ask_price);
                assert(tob.bid_qty == tob.bid_price * 10);
                assert(tob.ask_qty == tob.ask_price * 10);

                // Monotonicity check: we should not see time go backwards.
                // A torn read could potentially yield a "stale" value after a newer one.
                assert(tob.bid_price >= last_price);
                last_price = tob.bid_price;
            }
            total_reads += reads;
        });
    }

    // Let the test run for a short period
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    writer_thread.join();
    for (auto& t : reader_threads) {
        t.join();
    }

    std::cout << "[OK] All readers completed with no consistency errors.\n";
    std::cout << "Total reads performed: " << total_reads << "\n";
}


int main() {
    test_seqlock_basic_read_write();
    test_seqlock_concurrent_read_write();

    std::cout << "\nAll SeqLock tests passed!\n";
    return 0;
}
