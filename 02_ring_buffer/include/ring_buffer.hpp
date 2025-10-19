#pragma once

#include <atomic>
#include <array>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <limits>

namespace hft::core {
    /**
     * @brief Lock-free Ring Buffer for High-Frequency Trading
     *
     * @tparam T Type of elements stored in the buffer
     * @tparam BufferSize Compile-time fixed size of the buffer
     */
    template<typename T, size_t BufferSize>
    class RingBuffer {
    public:
        // Type traits and static assertions
        static_assert(std::is_move_constructible_v<T>,
            "RingBuffer element type must be move constructible");
        static_assert(BufferSize > 0,
            "RingBuffer size must be greater than zero");
        static_assert(BufferSize < std::numeric_limits<size_t>::max() / 2,
            "Buffer size too large");

        // Constructors and assignment operators
        RingBuffer() noexcept = default;
        ~RingBuffer() noexcept = default;

        // Prevent copying, allow moving
        RingBuffer(const RingBuffer&) = delete;
        RingBuffer& operator=(const RingBuffer&) = delete;
        RingBuffer(RingBuffer&&) noexcept = default;
        RingBuffer& operator=(RingBuffer&&) noexcept = default;

        /**
         * @brief Attempt to push an element into the buffer
         * @param item Element to be pushed
         * @return bool True if push was successful, false if buffer is full
         */
        [[nodiscard]] bool try_push(T item) noexcept {
            const auto current_head = head.load(std::memory_order_relaxed);
            const auto next_head = next_index(current_head);

            // Check if buffer is full
            if (next_head == tail.load(std::memory_order_relaxed)) {
                return false;
            }

            // Move item into buffer
            buffer[current_head] = std::move(item);

            // Update head with release semantics to ensure visibility
            head.store(next_head, std::memory_order_release);

            return true;
        }

        /**
         * @brief Attempt to pop an element from the buffer
         * @return std::optional<T> Popped element or std::nullopt if buffer is empty
         */
        [[nodiscard]] std::optional<T> try_pop() noexcept {
            const auto current_tail = tail.load(std::memory_order_relaxed);

            // Check if buffer is empty
            if (current_tail == head.load(std::memory_order_relaxed)) {
                return std::nullopt;
            }

            // Extract item
            T item = std::move(buffer[current_tail]);

            // Update tail with release semantics
            tail.store(next_index(current_tail), std::memory_order_release);

            return item;
        }

        /**
         * @brief Check if the buffer is empty
         * @return bool True if buffer is empty
         */
        [[nodiscard]] bool empty() const noexcept {
            return head.load(std::memory_order_relaxed) ==
                tail.load(std::memory_order_relaxed);
        }

        /**
         * @brief Check if the buffer is full
         * @return bool True if buffer is full
         */
        [[nodiscard]] bool full() const noexcept {
            return next_index(head.load(std::memory_order_relaxed)) ==
                tail.load(std::memory_order_relaxed);
        }

        /**
         * @brief Get the current number of elements in the buffer
         * @return size_t Number of elements
         */
        [[nodiscard]] size_t size() const noexcept {
            auto h = head.load(std::memory_order_relaxed);
            auto t = tail.load(std::memory_order_relaxed);
            return (h >= t) ? (h - t) : (BufferSize - t + h);
        }

    private:
        // Aligned to prevent false sharing
        alignas(64) std::array<T, BufferSize> buffer{};

        // Atomic indices with relaxed memory ordering
        std::atomic<size_t> head{ 0 };
        std::atomic<size_t> tail{ 0 };

        // Helper to increment and wrap around index
        [[nodiscard]] constexpr size_t next_index(size_t current) const noexcept {
            return (current + 1) % BufferSize;
        }
    };
} // namespace hft::core