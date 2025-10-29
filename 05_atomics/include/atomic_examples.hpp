#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>
#include <array>

namespace atomics {

// ============================================================================
// Example 1: Simple Atomic Counter (Relaxed)
// ============================================================================

class RelaxedCounter {
    std::atomic<uint64_t> count_{0};

public:
    // Relaxed ordering: We only care about the final count, not the order
    void increment() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t get() const {
        return count_.load(std::memory_order_relaxed);
    }

    void reset() {
        count_.store(0, std::memory_order_relaxed);
    }
};

// ============================================================================
// Example 2: Flag-Protected Data (Acquire/Release)
// ============================================================================

class FlagProtectedData {
    int data_{0};
    std::atomic<bool> ready_{false};

public:
    // Producer: Publish data with release semantics
    void publish(int value) {
        data_ = value;  // Non-atomic write
        ready_.store(true, std::memory_order_release);
        // Release ensures data_ write happens-before ready_ write
    }

    // Consumer: Read data with acquire semantics
    bool try_consume(int& out_value) {
        if (ready_.load(std::memory_order_acquire)) {
            // Acquire ensures ready_ read happens-before data_ read
            out_value = data_;
            return true;
        }
        return false;
    }

    void reset() {
        ready_.store(false, std::memory_order_relaxed);
        data_ = 0;
    }
};

// ============================================================================
// Example 3: Spinlock (Acquire/Release)
// ============================================================================

class Spinlock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        // Try to acquire lock with exchange
        while (locked_.exchange(true, std::memory_order_acquire)) {
            // Spin-wait: check with relaxed ordering
            while (locked_.load(std::memory_order_relaxed)) {
                // Hint to CPU that we're spinning (reduces power, helps hyperthreading)
#if defined(_MSC_VER)
                _mm_pause();  // MSVC intrinsic
#elif defined(__GNUC__) || defined(__clang__)
                __builtin_ia32_pause();  // GCC/Clang intrinsic
#endif
            }
        }
        // When we exit: memory is synchronized (acquire semantics)
    }

    void unlock() {
        // Release lock with release semantics
        locked_.store(false, std::memory_order_release);
        // Ensures all writes in critical section happen-before unlock
    }

    bool try_lock() {
        // Try to acquire without spinning
        return !locked_.exchange(true, std::memory_order_acquire);
    }
};

// ============================================================================
// Example 4: Reference Counting (Mixed Orderings)
// ============================================================================

class RefCount {
    std::atomic<int> count_{1};  // Start at 1 for initial owner

public:
    // Increment: relaxed is safe (no synchronization needed)
    void add_ref() {
        count_.fetch_add(1, std::memory_order_relaxed);
    }

    // Decrement: acq_rel for last reference
    bool release() {
        // Need acq_rel to synchronize with other decrements
        if (count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // We were the last reference
            // Acquire semantics ensure we see all writes from other threads
            return true;  // Caller should delete object
        }
        return false;
    }

    int get_count() const {
        // Just reading count, no synchronization needed
        return count_.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// Example 5: Compare-Exchange (CAS) Loop
// ============================================================================

class CASCounter {
    std::atomic<uint64_t> value_{0};

public:
    // Increment using CAS loop (demonstrates the pattern)
    void increment() {
        uint64_t expected = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(
            expected,           // Updated on failure
            expected + 1,       // Desired value
            std::memory_order_relaxed,  // Success ordering
            std::memory_order_relaxed   // Failure ordering
        )) {
            // expected was updated by CAS, loop continues
        }
    }

    // Try to set value if it's currently zero
    bool try_set_if_zero(uint64_t new_value) {
        uint64_t expected = 0;
        return value_.compare_exchange_strong(
            expected,
            new_value,
            std::memory_order_relaxed
        );
    }

    uint64_t get() const {
        return value_.load(std::memory_order_relaxed);
    }
};

// ============================================================================
// Example 6: Work Queue (Producer-Consumer)
// ============================================================================

template<typename T, size_t Capacity>
class AtomicQueue {
    struct alignas(64) AlignedIndex {
        std::atomic<size_t> value{0};
    };

    std::array<T, Capacity> buffer_;
    AlignedIndex head_;  // Consumer reads from here
    AlignedIndex tail_;  // Producer writes here

public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

    // Producer: Push item
    bool try_push(const T& item) {
        const size_t tail = tail_.value.load(std::memory_order_relaxed);
        const size_t head = head_.value.load(std::memory_order_acquire);

        // Check if queue is full
        if (tail - head >= Capacity) {
            return false;
        }

        // Write data
        buffer_[tail & (Capacity - 1)] = item;

        // Publish with release semantics
        tail_.value.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Consumer: Pop item
    bool try_pop(T& out_item) {
        const size_t head = head_.value.load(std::memory_order_relaxed);
        const size_t tail = tail_.value.load(std::memory_order_acquire);

        // Check if queue is empty
        if (head == tail) {
            return false;
        }

        // Read data
        out_item = buffer_[head & (Capacity - 1)];

        // Advance head with release semantics
        head_.value.store(head + 1, std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t head = head_.value.load(std::memory_order_relaxed);
        const size_t tail = tail_.value.load(std::memory_order_relaxed);
        return tail - head;
    }

    bool empty() const {
        return size() == 0;
    }
};

// ============================================================================
// Example 7: Sequential Consistency Demonstration
// ============================================================================

class SeqCstExample {
    std::atomic<int> x_{0};
    std::atomic<int> y_{0};

public:
    // Thread 1
    void thread1_writes(int& r1) {
        x_.store(1, std::memory_order_seq_cst);
        r1 = y_.load(std::memory_order_seq_cst);
    }

    // Thread 2
    void thread2_writes(int& r2) {
        y_.store(1, std::memory_order_seq_cst);
        r2 = x_.load(std::memory_order_seq_cst);
    }

    // With seq_cst: It's IMPOSSIBLE for r1==0 && r2==0
    // One thread MUST see the other's write

    void reset() {
        x_.store(0, std::memory_order_relaxed);
        y_.store(0, std::memory_order_relaxed);
    }
};

// ============================================================================
// Example 8: Relaxed Can Fail Synchronization
// ============================================================================

class RelaxedFailure {
    int data_{0};
    std::atomic<bool> flag_{false};

public:
    // Writer
    void write_data(int value) {
        data_ = value;
        flag_.store(true, std::memory_order_relaxed);  // ❌ Not enough!
    }

    // Reader
    bool read_data(int& out_value) {
        if (flag_.load(std::memory_order_relaxed)) {   // ❌ Not enough!
            out_value = data_;  // May read stale data!
            return true;
        }
        return false;
    }

    void reset() {
        flag_.store(false, std::memory_order_relaxed);
        data_ = 0;
    }
};

// ============================================================================
// Helper: Cache Line Size Constants
// ============================================================================

constexpr size_t CACHE_LINE_SIZE = 64;

// Pad structure to prevent false sharing
template<typename T>
struct alignas(CACHE_LINE_SIZE) CacheAligned {
    T value;

    CacheAligned() : value{} {}
    explicit CacheAligned(const T& v) : value(v) {}

    T& get() { return value; }
    const T& get() const { return value; }
};

} // namespace atomics
