#include <atomic>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <array>

// Compiler barrier to prevent optimization
#if defined(_MSC_VER)
    #include <intrin.h>
    #define COMPILER_BARRIER() _ReadWriteBarrier()
#elif defined(__GNUC__) || defined(__clang__)
    #define COMPILER_BARRIER() asm volatile("" ::: "memory")
#else
    #define COMPILER_BARRIER() std::atomic_signal_fence(std::memory_order_seq_cst)
#endif

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

struct BenchmarkResult {
    std::string name;
    uint64_t operations;
    double duration_us;
    double ops_per_sec;
    double ns_per_op;
};

template<typename Func>
BenchmarkResult benchmark(const std::string& name, uint64_t operations, Func&& func) {
    // Warmup
    func();

    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double ops_per_sec = operations / (duration_us / 1e6);
    double ns_per_op = (duration_us * 1000.0) / operations;

    return {name, operations, static_cast<double>(duration_us), ops_per_sec, ns_per_op};
}

void print_results(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n" << std::string(90, '=') << "\n";
    std::cout << std::left << std::setw(40) << "Benchmark"
              << std::right << std::setw(15) << "Duration (us)"
              << std::setw(18) << "Ops/sec (M)"
              << std::setw(15) << "ns/op"
              << "\n";
    std::cout << std::string(90, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(40) << r.name
                  << std::right << std::setw(15) << std::fixed << std::setprecision(0) << r.duration_us
                  << std::setw(18) << std::fixed << std::setprecision(2) << (r.ops_per_sec / 1e6)
                  << std::setw(15) << std::fixed << std::setprecision(2) << r.ns_per_op
                  << "\n";
    }
    std::cout << std::string(90, '=') << "\n";
}

// ============================================================================
// Benchmark 1: Load Performance
// ============================================================================

void bench_loads() {
    std::cout << "\n### Benchmark 1: Atomic Loads (Single Thread) ###\n";
    constexpr uint64_t ITERATIONS = 100'000'000;
    std::vector<BenchmarkResult> results;

    // Relaxed
    {
        std::atomic<int> x{42};
        results.push_back(benchmark("load(relaxed)", ITERATIONS, [&]() {
            int sum = 0;
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                sum += x.load(std::memory_order_relaxed);
            }
            COMPILER_BARRIER();  // Prevent optimization
            (void)sum;  // Ensure sum is used
        }));
    }

    // Acquire
    {
        std::atomic<int> x{42};
        results.push_back(benchmark("load(acquire)", ITERATIONS, [&]() {
            int sum = 0;
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                sum += x.load(std::memory_order_acquire);
            }
            COMPILER_BARRIER();  // Prevent optimization
            (void)sum;  // Ensure sum is used
        }));
    }

    // Seq_cst
    {
        std::atomic<int> x{42};
        results.push_back(benchmark("load(seq_cst)", ITERATIONS, [&]() {
            int sum = 0;
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                sum += x.load(std::memory_order_seq_cst);
            }
            COMPILER_BARRIER();  // Prevent optimization
            (void)sum;  // Ensure sum is used
        }));
    }

    print_results(results);
}

// ============================================================================
// Benchmark 2: Store Performance
// ============================================================================

void bench_stores() {
    std::cout << "\n### Benchmark 2: Atomic Stores (Single Thread) ###\n";
    constexpr uint64_t ITERATIONS = 100'000'000;
    std::vector<BenchmarkResult> results;

    // Relaxed
    {
        std::atomic<int> x{0};
        results.push_back(benchmark("store(relaxed)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                x.store(static_cast<int>(i), std::memory_order_relaxed);
            }
        }));
    }

    // Release
    {
        std::atomic<int> x{0};
        results.push_back(benchmark("store(release)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                x.store(static_cast<int>(i), std::memory_order_release);
            }
        }));
    }

    // Seq_cst (expensive!)
    {
        std::atomic<int> x{0};
        results.push_back(benchmark("store(seq_cst)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                x.store(static_cast<int>(i), std::memory_order_seq_cst);
            }
        }));
    }

    print_results(results);
}

// ============================================================================
// Benchmark 3: Fetch-Add Performance
// ============================================================================

void bench_fetch_add() {
    std::cout << "\n### Benchmark 3: Atomic fetch_add (Single Thread) ###\n";
    constexpr uint64_t ITERATIONS = 50'000'000;
    std::vector<BenchmarkResult> results;

    // Relaxed
    {
        std::atomic<uint64_t> x{0};
        results.push_back(benchmark("fetch_add(relaxed)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                x.fetch_add(1, std::memory_order_relaxed);
            }
        }));
    }

    // Acq_rel
    {
        std::atomic<uint64_t> x{0};
        results.push_back(benchmark("fetch_add(acq_rel)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                x.fetch_add(1, std::memory_order_acq_rel);
            }
        }));
    }

    // Seq_cst
    {
        std::atomic<uint64_t> x{0};
        results.push_back(benchmark("fetch_add(seq_cst)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                x.fetch_add(1, std::memory_order_seq_cst);
            }
        }));
    }

    print_results(results);
}

// ============================================================================
// Benchmark 4: Compare-Exchange Performance
// ============================================================================

void bench_compare_exchange() {
    std::cout << "\n### Benchmark 4: Atomic compare_exchange (Single Thread) ###\n";
    constexpr uint64_t ITERATIONS = 10'000'000;
    std::vector<BenchmarkResult> results;

    // CAS weak (successful)
    {
        std::atomic<uint64_t> x{0};
        results.push_back(benchmark("CAS weak (success)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                uint64_t expected = i;
                x.compare_exchange_weak(expected, i + 1, std::memory_order_relaxed);
            }
        }));
    }

    // CAS strong (successful)
    {
        std::atomic<uint64_t> x{0};
        results.push_back(benchmark("CAS strong (success)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                uint64_t expected = i;
                x.compare_exchange_strong(expected, i + 1, std::memory_order_relaxed);
            }
        }));
    }

    // CAS weak (with retry loop - contention simulation)
    {
        std::atomic<uint64_t> x{0};
        results.push_back(benchmark("CAS weak (retry loop)", ITERATIONS, [&]() {
            for (uint64_t i = 0; i < ITERATIONS; i++) {
                uint64_t expected = x.load(std::memory_order_relaxed);
                while (!x.compare_exchange_weak(expected, expected + 1, std::memory_order_relaxed)) {
                    // expected is updated automatically
                }
            }
        }));
    }

    print_results(results);
}

// ============================================================================
// Benchmark 5: Multi-threaded Counter (Contention)
// ============================================================================

void bench_multithreaded_counter() {
    std::cout << "\n### Benchmark 5: Multi-threaded Counter (High Contention) ###\n";
    
    const int NUM_THREADS = std::thread::hardware_concurrency();
    constexpr uint64_t ITERATIONS_PER_THREAD = 1'000'000;
    const uint64_t TOTAL_OPS = NUM_THREADS * ITERATIONS_PER_THREAD;

    std::cout << "Threads: " << NUM_THREADS << "\n";
    std::cout << "Iterations per thread: " << ITERATIONS_PER_THREAD << "\n";

    std::vector<BenchmarkResult> results;

    // Relaxed
    {
        std::atomic<uint64_t> counter{0};
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < NUM_THREADS; t++) {
            threads.emplace_back([&]() {
                for (uint64_t i = 0; i < ITERATIONS_PER_THREAD; i++) {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        results.push_back({
            "MT fetch_add(relaxed)",
            TOTAL_OPS,
            static_cast<double>(duration_us),
            TOTAL_OPS / (duration_us / 1e6),
            (duration_us * 1000.0) / TOTAL_OPS
        });
    }

    // Seq_cst
    {
        std::atomic<uint64_t> counter{0};
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < NUM_THREADS; t++) {
            threads.emplace_back([&]() {
                for (uint64_t i = 0; i < ITERATIONS_PER_THREAD; i++) {
                    counter.fetch_add(1, std::memory_order_seq_cst);
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        results.push_back({
            "MT fetch_add(seq_cst)",
            TOTAL_OPS,
            static_cast<double>(duration_us),
            TOTAL_OPS / (duration_us / 1e6),
            (duration_us * 1000.0) / TOTAL_OPS
        });
    }

    print_results(results);
}

// ============================================================================
// Benchmark 6: False Sharing Impact
// ============================================================================

// Define PaddedCounter outside function for MSVC compatibility
struct alignas(64) PaddedCounter {
    std::atomic<uint64_t> count{0};
    char padding[64 - sizeof(std::atomic<uint64_t>)];
};

void bench_false_sharing() {
    std::cout << "\n### Benchmark 6: False Sharing Impact ###\n";
    
    constexpr int NUM_THREADS = 4;
    constexpr uint64_t ITERATIONS = 10'000'000;
    const uint64_t TOTAL_OPS = NUM_THREADS * ITERATIONS;

    std::vector<BenchmarkResult> results;

    // Without padding (false sharing)
    {
        struct Counters {
            std::atomic<uint64_t> counts[NUM_THREADS];
        } counters{};

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < NUM_THREADS; t++) {
            threads.emplace_back([&, t]() {
                for (uint64_t i = 0; i < ITERATIONS; i++) {
                    counters.counts[t].fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        results.push_back({
            "No padding (false sharing)",
            TOTAL_OPS,
            static_cast<double>(duration_us),
            TOTAL_OPS / (duration_us / 1e6),
            (duration_us * 1000.0) / TOTAL_OPS
        });
    }

    // With padding (no false sharing)
    {
        std::array<PaddedCounter, NUM_THREADS> counters;

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (int t = 0; t < NUM_THREADS; t++) {
            threads.emplace_back([&, t]() {
                for (uint64_t i = 0; i < ITERATIONS; i++) {
                    counters[t].count.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : threads) {
            th.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        results.push_back({
            "With padding (no false sharing)",
            TOTAL_OPS,
            static_cast<double>(duration_us),
            TOTAL_OPS / (duration_us / 1e6),
            (duration_us * 1000.0) / TOTAL_OPS
        });
    }

    print_results(results);

    // Calculate speedup
    double speedup = results[0].duration_us / results[1].duration_us;
    std::cout << "\nSpeedup from eliminating false sharing: " << std::fixed << std::setprecision(2) 
              << speedup << "x\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "==============================================\n";
    std::cout << "    Atomic Operations Performance Benchmark   \n";
    std::cout << "==============================================\n";
    std::cout << "\nHardware: " << std::thread::hardware_concurrency() << " hardware threads\n";
    std::cout << "\nNOTE: Results vary by CPU architecture!\n";
    std::cout << "      x86 has strong memory model (acquire/release nearly free)\n";
    std::cout << "      ARM has weak memory model (acquire/release more expensive)\n";

    try {
        bench_loads();
        bench_stores();
        bench_fetch_add();
        bench_compare_exchange();
        bench_multithreaded_counter();
        bench_false_sharing();

        std::cout << "\n==============================================\n";
        std::cout << "         Benchmarking Complete!               \n";
        std::cout << "==============================================\n";

        std::cout << "\nKey Takeaways:\n";
        std::cout << "1. On x86: relaxed ~ acquire/release (loads/stores)\n";
        std::cout << "2. seq_cst stores are MUCH slower (MFENCE overhead)\n";
        std::cout << "3. CAS operations are expensive (cache line locking)\n";
        std::cout << "4. False sharing can cause massive slowdowns\n";
        std::cout << "5. Multi-threaded contention reduces throughput\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

