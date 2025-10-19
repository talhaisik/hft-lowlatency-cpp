#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <array>
#include <atomic>
#include <optional>

// Optimized Ring Buffer with power-of-2 masking
namespace hft::core {
    template<typename T, size_t BufferSize>
    class RingBuffer {
    public:
        static_assert((BufferSize& (BufferSize - 1)) == 0,
            "BufferSize must be power of 2 for optimal performance");
        static_assert(std::is_move_constructible_v<T>);

        static constexpr size_t INDEX_MASK = BufferSize - 1;

        RingBuffer() noexcept = default;
        RingBuffer(const RingBuffer&) = delete;
        RingBuffer& operator=(const RingBuffer&) = delete;

        [[nodiscard]] bool try_push(T item) noexcept {
            const auto current_head = head_.load(std::memory_order_relaxed);
            const auto next_head = (current_head + 1) & INDEX_MASK;

            if (next_head == tail_.load(std::memory_order_acquire)) {
                return false;
            }

            buffer_[current_head] = std::move(item);
            head_.store(next_head, std::memory_order_release);
            return true;
        }

        [[nodiscard]] std::optional<T> try_pop() noexcept {
            const auto current_tail = tail_.load(std::memory_order_relaxed);

            if (current_tail == head_.load(std::memory_order_acquire)) {
                return std::nullopt;
            }

            T item = std::move(buffer_[current_tail]);
            tail_.store((current_tail + 1) & INDEX_MASK, std::memory_order_release);
            return item;
        }

        [[nodiscard]] bool empty() const noexcept {
            return head_.load(std::memory_order_relaxed) ==
                tail_.load(std::memory_order_relaxed);
        }

    private:
        alignas(64) std::array<T, BufferSize> buffer_{};
        alignas(64) std::atomic<size_t> head_{ 0 };
        alignas(64) std::atomic<size_t> tail_{ 0 };
    };
}

// Fair benchmark comparing equivalent operations
namespace hft::benchmark {

    constexpr size_t BUFFER_SIZE = 1024;  // Power of 2
    constexpr size_t NUM_ITERATIONS = 10'000'000;
    constexpr size_t WARMUP_ITERATIONS = 100'000;
    constexpr size_t NUM_RUNS = 5;

    // Benchmark: Single-threaded push/pop operations
    double benchmark_ring_buffer() {
        hft::core::RingBuffer<int, BUFFER_SIZE> ring_buffer;

        // Fill buffer halfway to test realistic scenario
        for (size_t i = 0; i < BUFFER_SIZE / 2; ++i) {
            ring_buffer.try_push(static_cast<int>(i));
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            ring_buffer.try_push(static_cast<int>(i));
            ring_buffer.try_pop();
        }

        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start).count();
    }

    // Benchmark: Vector with circular buffer simulation
    double benchmark_vector() {
        std::vector<int> vec;
        vec.reserve(BUFFER_SIZE);

        // Fill vector halfway
        for (size_t i = 0; i < BUFFER_SIZE / 2; ++i) {
            vec.push_back(static_cast<int>(i));
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
            if (vec.size() >= BUFFER_SIZE) {
                vec.erase(vec.begin());  // Simulate pop from front
            }
            vec.push_back(static_cast<int>(i));
        }

        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start).count();
    }

    void run_benchmark() {
        std::cout << "=== HFT Ring Buffer Benchmark ===" << std::endl;
        std::cout << "Buffer Size: " << BUFFER_SIZE << std::endl;
        std::cout << "Iterations: " << NUM_ITERATIONS << std::endl;
        std::cout << "Runs per test: " << NUM_RUNS << std::endl << std::endl;

        // Warmup
        std::cout << "Warming up..." << std::endl;
        for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
            hft::core::RingBuffer<int, BUFFER_SIZE> rb;
            rb.try_push(i);
            rb.try_pop();
        }

        // Run Ring Buffer benchmark
        std::array<double, NUM_RUNS> ring_times;
        std::cout << "\nTesting Ring Buffer..." << std::endl;
        for (size_t run = 0; run < NUM_RUNS; ++run) {
            ring_times[run] = benchmark_ring_buffer();
            std::cout << "  Run " << (run + 1) << ": " << ring_times[run] << "s" << std::endl;
        }

        // Run Vector benchmark
        std::array<double, NUM_RUNS> vec_times;
        std::cout << "\nTesting Vector..." << std::endl;
        for (size_t run = 0; run < NUM_RUNS; ++run) {
            vec_times[run] = benchmark_vector();
            std::cout << "  Run " << (run + 1) << ": " << vec_times[run] << "s" << std::endl;
        }

        // Calculate statistics
        auto avg_ring = std::accumulate(ring_times.begin(), ring_times.end(), 0.0) / NUM_RUNS;
        auto avg_vec = std::accumulate(vec_times.begin(), vec_times.end(), 0.0) / NUM_RUNS;

        auto min_ring = *std::min_element(ring_times.begin(), ring_times.end());
        auto min_vec = *std::min_element(vec_times.begin(), vec_times.end());

        // Results
        std::cout << "\n=== RESULTS ===" << std::endl;
        std::cout << "Ring Buffer:" << std::endl;
        std::cout << "  Average: " << avg_ring << "s" << std::endl;
        std::cout << "  Best:    " << min_ring << "s" << std::endl;
        std::cout << "  Throughput: " << (NUM_ITERATIONS / avg_ring / 1e6) << " Mops/s" << std::endl;

        std::cout << "\nVector:" << std::endl;
        std::cout << "  Average: " << avg_vec << "s" << std::endl;
        std::cout << "  Best:    " << min_vec << "s" << std::endl;
        std::cout << "  Throughput: " << (NUM_ITERATIONS / avg_vec / 1e6) << " Mops/s" << std::endl;

        std::cout << "\nComparison:" << std::endl;
        if (avg_vec < avg_ring) {
            std::cout << "  Vector is " << (avg_ring / avg_vec) << "x FASTER" << std::endl;
        }
        else {
            std::cout << "  Ring Buffer is " << (avg_vec / avg_ring) << "x FASTER" << std::endl;
        }
    }
}

int main() {
    try {
        hft::benchmark::run_benchmark();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}