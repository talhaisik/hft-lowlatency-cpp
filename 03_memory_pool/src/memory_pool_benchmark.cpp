#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include "memory_pool.hpp"

using namespace hft::memory;
using namespace std::chrono;

// Test object similar to what you'd have in HFT
struct Order {
    size_t order_id;
    double price;
    size_t quantity;
    uint64_t timestamp;
    char symbol[8];
    
    Order() = default;
    Order(size_t id, double p, size_t q) 
        : order_id(id), price(p), quantity(q), timestamp(0) {
        symbol[0] = '\0';
    }
};

// Benchmark configuration
constexpr size_t POOL_SIZE = 10'000;
constexpr size_t NUM_ITERATIONS = 10'000'000;  // 10M for better measurement
constexpr size_t WARMUP_ITERATIONS = 100'000;
constexpr size_t NUM_RUNS = 5;

// Benchmark 1: Pure allocation/deallocation (new/delete)
double benchmark_new_delete_pure() {
    auto start = high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        Order* order = new Order(i, 100.0 + i, 100);
        delete order;
    }
    
    auto end = high_resolution_clock::now();
    return duration<double>(end - start).count();
}

// Benchmark 2: Pure allocation/deallocation (memory pool)
double benchmark_memory_pool_pure() {
    MemoryPool<Order, POOL_SIZE> pool;
    
    auto start = high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        Order* order = pool.allocate(i, 100.0 + i, 100);
        pool.deallocate(order);
    }
    
    auto end = high_resolution_clock::now();
    return duration<double>(end - start).count();
}

// Benchmark 3: Realistic pattern - allocate N, use them, deallocate N
double benchmark_new_delete_batch() {
    constexpr size_t BATCH_SIZE = 1000;
    constexpr size_t NUM_BATCHES = NUM_ITERATIONS / BATCH_SIZE;
    
    auto start = high_resolution_clock::now();
    
    for (size_t batch = 0; batch < NUM_BATCHES; ++batch) {
        // Allocate batch
        Order* orders[BATCH_SIZE];
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            orders[i] = new Order(batch * BATCH_SIZE + i, 100.0, 100);
        }
        
        // Use orders (simulate processing)
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            orders[i]->price += 0.01;  // Touch the memory
        }
        
        // Deallocate batch
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            delete orders[i];
        }
    }
    
    auto end = high_resolution_clock::now();
    return duration<double>(end - start).count();
}

// Benchmark 4: Realistic pattern - memory pool
double benchmark_memory_pool_batch() {
    constexpr size_t BATCH_SIZE = 1000;
    constexpr size_t NUM_BATCHES = NUM_ITERATIONS / BATCH_SIZE;
    
    MemoryPool<Order, POOL_SIZE> pool;
    
    auto start = high_resolution_clock::now();
    
    for (size_t batch = 0; batch < NUM_BATCHES; ++batch) {
        // Allocate batch
        Order* orders[BATCH_SIZE];
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            orders[i] = pool.allocate(batch * BATCH_SIZE + i, 100.0, 100);
        }
        
        // Use orders (simulate processing)
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            orders[i]->price += 0.01;  // Touch the memory
        }
        
        // Deallocate batch
        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            pool.deallocate(orders[i]);
        }
    }
    
    auto end = high_resolution_clock::now();
    return duration<double>(end - start).count();
}

// Benchmark: Single allocation latency distribution
void benchmark_latency_distribution() {
    std::cout << "\n=== Allocation Latency Distribution ===\n";
    
    constexpr size_t SAMPLES = 100'000;
    std::vector<double> new_delete_times;
    std::vector<double> pool_times;
    
    new_delete_times.reserve(SAMPLES);
    pool_times.reserve(SAMPLES);
    
    // Benchmark new/delete
    for (size_t i = 0; i < SAMPLES; ++i) {
        auto start = high_resolution_clock::now();
        Order* order = new Order(i, 100.0, 100);
        auto mid = high_resolution_clock::now();
        delete order;
        auto end = high_resolution_clock::now();
        
        double alloc_time = duration<double, std::nano>(mid - start).count();
        double dealloc_time = duration<double, std::nano>(end - mid).count();
        new_delete_times.push_back(alloc_time + dealloc_time);
    }
    
    // Benchmark pool
    MemoryPool<Order, POOL_SIZE> pool;
    for (size_t i = 0; i < SAMPLES; ++i) {
        auto start = high_resolution_clock::now();
        Order* order = pool.allocate(i, 100.0, 100);
        auto mid = high_resolution_clock::now();
        pool.deallocate(order);
        auto end = high_resolution_clock::now();
        
        double alloc_time = duration<double, std::nano>(mid - start).count();
        double dealloc_time = duration<double, std::nano>(end - mid).count();
        pool_times.push_back(alloc_time + dealloc_time);
    }
    
    // Calculate statistics
    auto calc_stats = [](std::vector<double>& times) {
        std::sort(times.begin(), times.end());
        double avg = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        double p50 = times[times.size() / 2];
        double p95 = times[times.size() * 95 / 100];
        double p99 = times[times.size() * 99 / 100];
        double p999 = times[times.size() * 999 / 1000];
        double min = times.front();
        double max = times.back();
        
        return std::make_tuple(avg, p50, p95, p99, p999, min, max);
    };
    
    auto [nd_avg, nd_p50, nd_p95, nd_p99, nd_p999, nd_min, nd_max] = calc_stats(new_delete_times);
    auto [pool_avg, pool_p50, pool_p95, pool_p99, pool_p999, pool_min, pool_max] = calc_stats(pool_times);
    
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "\nnew/delete latency (nanoseconds):\n";
    std::cout << "  Average: " << nd_avg << " ns\n";
    std::cout << "  Median:  " << nd_p50 << " ns\n";
    std::cout << "  P95:     " << nd_p95 << " ns\n";
    std::cout << "  P99:     " << nd_p99 << " ns\n";
    std::cout << "  P99.9:   " << nd_p999 << " ns\n";
    std::cout << "  Min:     " << nd_min << " ns\n";
    std::cout << "  Max:     " << nd_max << " ns\n";
    
    std::cout << "\nMemory Pool latency (nanoseconds):\n";
    std::cout << "  Average: " << pool_avg << " ns\n";
    std::cout << "  Median:  " << pool_p50 << " ns\n";
    std::cout << "  P95:     " << pool_p95 << " ns\n";
    std::cout << "  P99:     " << pool_p99 << " ns\n";
    std::cout << "  P99.9:   " << pool_p999 << " ns\n";
    std::cout << "  Min:     " << pool_min << " ns\n";
    std::cout << "  Max:     " << pool_max << " ns\n";
    
    std::cout << "\n=== Speedup ===\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Average: " << (nd_avg / pool_avg) << "x faster\n";
    std::cout << "  Median:  " << (nd_p50 / pool_p50) << "x faster\n";
    std::cout << "  P95:     " << (nd_p95 / pool_p95) << "x faster\n";
    std::cout << "  P99:     " << (nd_p99 / pool_p99) << "x faster\n";
    std::cout << "  P99.9:   " << (nd_p999 / pool_p999) << "x faster\n";
}

// Main benchmark runner
void run_benchmark() {
    std::cout << "=== Memory Pool Benchmark (Fair Comparison) ===\n";
    std::cout << "Pool Size: " << POOL_SIZE << "\n";
    std::cout << "Iterations: " << NUM_ITERATIONS << "\n";
    std::cout << "Runs per test: " << NUM_RUNS << "\n\n";
    
    // Warmup
    std::cout << "Warming up...\n";
    MemoryPool<Order, POOL_SIZE> warmup_pool;
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        Order* o = warmup_pool.allocate(i, 100.0, 100);
        warmup_pool.deallocate(o);
    }
    
    std::cout << "\n=== Test 1: Pure Alloc/Dealloc (Immediate) ===\n";
    
    // Benchmark new/delete pure
    std::vector<double> new_delete_pure_times;
    std::cout << "\nBenchmarking new/delete (pure)...\n";
    for (size_t run = 0; run < NUM_RUNS; ++run) {
        double time = benchmark_new_delete_pure();
        new_delete_pure_times.push_back(time);
        std::cout << "  Run " << (run + 1) << ": " << time << "s\n";
    }
    
    // Benchmark memory pool pure
    std::vector<double> pool_pure_times;
    std::cout << "\nBenchmarking memory pool (pure)...\n";
    for (size_t run = 0; run < NUM_RUNS; ++run) {
        double time = benchmark_memory_pool_pure();
        pool_pure_times.push_back(time);
        std::cout << "  Run " << (run + 1) << ": " << time << "s\n";
    }
    
    // Calculate statistics
    auto avg_nd_pure = std::accumulate(new_delete_pure_times.begin(), 
        new_delete_pure_times.end(), 0.0) / NUM_RUNS;
    auto avg_pool_pure = std::accumulate(pool_pure_times.begin(), 
        pool_pure_times.end(), 0.0) / NUM_RUNS;
    
    std::cout << "\n--- Pure Alloc/Dealloc Results ---\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "new/delete:   " << avg_nd_pure << "s (" 
              << (NUM_ITERATIONS / avg_nd_pure / 1e6) << " Mops/s)\n";
    std::cout << "Memory Pool:  " << avg_pool_pure << "s (" 
              << (NUM_ITERATIONS / avg_pool_pure / 1e6) << " Mops/s)\n";
    std::cout << "Speedup:      " << (avg_nd_pure / avg_pool_pure) << "x FASTER\n";
    
    std::cout << "\n=== Test 2: Batch Alloc/Use/Dealloc (Realistic) ===\n";
    
    // Benchmark new/delete batch
    std::vector<double> new_delete_batch_times;
    std::cout << "\nBenchmarking new/delete (batch)...\n";
    for (size_t run = 0; run < NUM_RUNS; ++run) {
        double time = benchmark_new_delete_batch();
        new_delete_batch_times.push_back(time);
        std::cout << "  Run " << (run + 1) << ": " << time << "s\n";
    }
    
    // Benchmark memory pool batch
    std::vector<double> pool_batch_times;
    std::cout << "\nBenchmarking memory pool (batch)...\n";
    for (size_t run = 0; run < NUM_RUNS; ++run) {
        double time = benchmark_memory_pool_batch();
        pool_batch_times.push_back(time);
        std::cout << "  Run " << (run + 1) << ": " << time << "s\n";
    }
    
    auto avg_nd_batch = std::accumulate(new_delete_batch_times.begin(), 
        new_delete_batch_times.end(), 0.0) / NUM_RUNS;
    auto avg_pool_batch = std::accumulate(pool_batch_times.begin(), 
        pool_batch_times.end(), 0.0) / NUM_RUNS;
    
    std::cout << "\n--- Batch Alloc/Use/Dealloc Results ---\n";
    std::cout << "new/delete:   " << avg_nd_batch << "s (" 
              << (NUM_ITERATIONS / avg_nd_batch / 1e6) << " Mops/s)\n";
    std::cout << "Memory Pool:  " << avg_pool_batch << "s (" 
              << (NUM_ITERATIONS / avg_pool_batch / 1e6) << " Mops/s)\n";
    std::cout << "Speedup:      " << (avg_nd_batch / avg_pool_batch) << "x FASTER\n";
    
    // Latency distribution
    benchmark_latency_distribution();
    
    std::cout << "\n=== Summary ===\n";
    std::cout << "Pure allocation speedup:   " << (avg_nd_pure / avg_pool_pure) << "x\n";
    std::cout << "Batch pattern speedup:     " << (avg_nd_batch / avg_pool_batch) << "x\n";
    std::cout << "\nKey insight: Memory pools eliminate heap overhead completely!\n";
}

int main() {
    try {
        run_benchmark();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
}