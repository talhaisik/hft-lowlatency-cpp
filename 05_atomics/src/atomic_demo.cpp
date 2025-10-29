#include "../include/atomic_examples.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <cassert>

using namespace atomics;
using namespace std::chrono_literals;

// ============================================================================
// Demo 1: Relaxed Counter
// ============================================================================

void demo_relaxed_counter() {
    std::cout << "\n=== Demo 1: Relaxed Counter ===\n";

    RelaxedCounter counter;
    constexpr int NUM_THREADS = 4;
    constexpr int INCREMENTS_PER_THREAD = 100'000;

    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < INCREMENTS_PER_THREAD; j++) {
                counter.increment();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Threads: " << NUM_THREADS << "\n";
    std::cout << "Increments per thread: " << INCREMENTS_PER_THREAD << "\n";
    std::cout << "Expected count: " << (NUM_THREADS * INCREMENTS_PER_THREAD) << "\n";
    std::cout << "Actual count: " << counter.get() << "\n";
    std::cout << "Duration: " << duration.count() << " us\n";
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) 
              << (NUM_THREADS * INCREMENTS_PER_THREAD / (duration.count() / 1e6) / 1e6) 
              << " million ops/sec\n";

    assert(counter.get() == NUM_THREADS * INCREMENTS_PER_THREAD);
    std::cout << "[PASS] Count is correct!\n";
}

// ============================================================================
// Demo 2: Flag-Protected Data
// ============================================================================

void demo_flag_protected_data() {
    std::cout << "\n=== Demo 2: Flag-Protected Data (Acquire/Release) ===\n";

    FlagProtectedData fpd;
    constexpr int TEST_VALUE = 42;
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&]() {
        std::this_thread::sleep_for(10ms);  // Simulate work
        fpd.publish(TEST_VALUE);
        producer_done.store(true, std::memory_order_release);
    });

    // Consumer thread
    std::thread consumer([&]() {
        int value = 0;
        
        // Spin until data is ready
        while (!fpd.try_consume(value)) {
            std::this_thread::yield();
        }

        std::cout << "Consumer received: " << value << "\n";
        assert(value == TEST_VALUE);
    });

    producer.join();
    consumer.join();

    std::cout << "[PASS] Data synchronized correctly!\n";
}

// ============================================================================
// Demo 3: Spinlock
// ============================================================================

void demo_spinlock() {
    std::cout << "\n=== Demo 3: Spinlock ===\n";

    Spinlock lock;
    int shared_counter = 0;  // Protected by spinlock
    constexpr int NUM_THREADS = 4;
    constexpr int INCREMENTS = 10'000;

    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < INCREMENTS; j++) {
                lock.lock();
                shared_counter++;  // Critical section
                lock.unlock();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Expected: " << (NUM_THREADS * INCREMENTS) << "\n";
    std::cout << "Actual: " << shared_counter << "\n";
    std::cout << "Duration: " << duration.count() << " us\n";

    assert(shared_counter == NUM_THREADS * INCREMENTS);
    std::cout << "[PASS] Spinlock protected counter correctly!\n";
}

// ============================================================================
// Demo 4: Reference Counting
// ============================================================================

void demo_reference_counting() {
    std::cout << "\n=== Demo 4: Reference Counting ===\n";

    RefCount ref;
    std::cout << "Initial count: " << ref.get_count() << "\n";

    // Simulate multiple owners
    ref.add_ref();  // Thread 1 takes ownership
    ref.add_ref();  // Thread 2 takes ownership
    ref.add_ref();  // Thread 3 takes ownership
    std::cout << "After 3 add_ref(): " << ref.get_count() << "\n";

    // Release references
    bool should_delete = ref.release();
    std::cout << "After release 1: count=" << ref.get_count() 
              << ", should_delete=" << should_delete << "\n";
    
    should_delete = ref.release();
    std::cout << "After release 2: count=" << ref.get_count() 
              << ", should_delete=" << should_delete << "\n";
    
    should_delete = ref.release();
    std::cout << "After release 3: count=" << ref.get_count() 
              << ", should_delete=" << should_delete << "\n";
    
    should_delete = ref.release();
    std::cout << "After release 4 (last): count=" << ref.get_count() 
              << ", should_delete=" << should_delete << "\n";

    assert(should_delete);
    std::cout << "[PASS] Reference counting works correctly!\n";
}

// ============================================================================
// Demo 5: CAS Loop
// ============================================================================

void demo_cas_loop() {
    std::cout << "\n=== Demo 5: Compare-And-Swap Loop ===\n";

    CASCounter counter;
    constexpr int NUM_THREADS = 4;
    constexpr int INCREMENTS = 50'000;

    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < INCREMENTS; j++) {
                counter.increment();  // Uses CAS internally
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Expected: " << (NUM_THREADS * INCREMENTS) << "\n";
    std::cout << "Actual: " << counter.get() << "\n";
    std::cout << "Duration: " << duration.count() << " us\n";

    assert(counter.get() == NUM_THREADS * INCREMENTS);
    std::cout << "[PASS] CAS loop works correctly!\n";

    // Test try_set_if_zero
    counter.try_set_if_zero(100);
    std::cout << "After try_set_if_zero(100): " << counter.get() 
              << " (should stay " << (NUM_THREADS * INCREMENTS) << ")\n";
    
    CASCounter counter2;
    bool success = counter2.try_set_if_zero(100);
    std::cout << "New counter try_set_if_zero(100): " << counter2.get() 
              << " (success=" << success << ")\n";
    assert(success && counter2.get() == 100);
}

// ============================================================================
// Demo 6: Work Queue
// ============================================================================

void demo_work_queue() {
    std::cout << "\n=== Demo 6: Atomic Work Queue (SPSC) ===\n";

    AtomicQueue<int, 1024> queue;
    constexpr int NUM_ITEMS = 10'000;
    std::atomic<int> sum_produced{0};
    std::atomic<int> sum_consumed{0};

    // Producer
    std::thread producer([&]() {
        for (int i = 1; i <= NUM_ITEMS; i++) {
            while (!queue.try_push(i)) {
                std::this_thread::yield();
            }
            sum_produced.fetch_add(i, std::memory_order_relaxed);
        }
    });

    // Consumer
    std::thread consumer([&]() {
        int count = 0;
        while (count < NUM_ITEMS) {
            int value;
            if (queue.try_pop(value)) {
                sum_consumed.fetch_add(value, std::memory_order_relaxed);
                count++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    std::cout << "Items produced: " << NUM_ITEMS << "\n";
    std::cout << "Sum produced: " << sum_produced.load() << "\n";
    std::cout << "Sum consumed: " << sum_consumed.load() << "\n";
    std::cout << "Queue empty: " << (queue.empty() ? "yes" : "no") << "\n";

    assert(sum_produced.load() == sum_consumed.load());
    assert(queue.empty());
    std::cout << "[PASS] Queue works correctly!\n";
}

// ============================================================================
// Demo 7: Sequential Consistency
// ============================================================================

void demo_seq_cst() {
    std::cout << "\n=== Demo 7: Sequential Consistency ===\n";
    std::cout << "Running 100,000 iterations to prove seq_cst prevents r1==0 && r2==0...\n";

    SeqCstExample example;
    int violations = 0;
    constexpr int ITERATIONS = 100'000;

    for (int i = 0; i < ITERATIONS; i++) {
        example.reset();
        
        int r1 = 0, r2 = 0;

        std::thread t1([&]() { example.thread1_writes(r1); });
        std::thread t2([&]() { example.thread2_writes(r2); });

        t1.join();
        t2.join();

        // With seq_cst: This should NEVER happen
        if (r1 == 0 && r2 == 0) {
            violations++;
        }
    }

    std::cout << "Violations (r1==0 && r2==0): " << violations << " out of " << ITERATIONS << "\n";
    std::cout << "[" << (violations == 0 ? "PASS" : "FAIL") 
              << "] Sequential consistency maintained!\n";
}

// ============================================================================
// Demo 8: Relaxed Ordering Can Fail
// ============================================================================

void demo_relaxed_failure() {
    std::cout << "\n=== Demo 8: Relaxed Ordering Can Fail (Data Race) ===\n";
    std::cout << "WARNING: This demonstrates a race condition with relaxed ordering!\n";
    std::cout << "Running 10,000 iterations to detect data races...\n";

    int mismatches = 0;
    constexpr int ITERATIONS = 10'000;

    for (int i = 0; i < ITERATIONS; i++) {
        RelaxedFailure example;
        int test_value = i + 1;
        int read_value = -1;

        std::thread writer([&]() {
            example.write_data(test_value);
        });

        std::thread reader([&]() {
            // Spin until flag is set
            while (!example.read_data(read_value)) {
                std::this_thread::yield();
            }
        });

        writer.join();
        reader.join();

        if (read_value != test_value) {
            mismatches++;
        }
    }

    std::cout << "Data mismatches detected: " << mismatches << " out of " << ITERATIONS << "\n";
    
    if (mismatches > 0) {
        std::cout << "[EXPECTED] Relaxed ordering caused data races!\n";
        std::cout << "This is why we need acquire/release for synchronization!\n";
    } else {
        std::cout << "[NOTE] No mismatches detected in this run (but race still exists)\n";
        std::cout << "The race condition is real but may not always manifest.\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "==============================================\n";
    std::cout << "      Atomic Operations Deep Dive Demo       \n";
    std::cout << "==============================================\n";
    std::cout << "\nHardware concurrency: " << std::thread::hardware_concurrency() << " threads\n";

    try {
        demo_relaxed_counter();
        demo_flag_protected_data();
        demo_spinlock();
        demo_reference_counting();
        demo_cas_loop();
        demo_work_queue();
        demo_seq_cst();
        demo_relaxed_failure();

        std::cout << "\n==============================================\n";
        std::cout << "           All Demos Completed!               \n";
        std::cout << "==============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

