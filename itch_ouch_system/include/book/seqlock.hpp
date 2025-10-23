#pragma once

#include <atomic>
#include <concepts>

namespace hft {

    /**
     * @class SeqLock
     * @brief A lock-free, single-writer, multiple-reader synchronization primitive.
     *
     * This implementation allows multiple readers to access data without blocking, and
     * a single writer to update the data without using locks (like mutexes). Readers
     * detect when a write was in progress and simply retry their read.
     *
     * This pattern is highly efficient when:
     * 1. Writes are infrequent compared to reads.
     * 2. The data structure `T` is small and cheap to copy.
     * 3. There is only ONE writer thread.
     *
     * @tparam T The type of the data to be protected. Must be trivially copyable.
     */
    template <std::copyable T>
    class SeqLock {
    public:
        SeqLock() : sequence_(0) {}

        /**
         * @brief Reads the protected data in a lock-free manner.
         *
         * This function will retry the read if it detects that a write
         * occurred during the read operation.
         *
         * @return A consistent copy of the protected data.
         */
        T read() const {
            T data;
            uint32_t seq1, seq2 = 0;
            do {
                seq1 = sequence_.load(std::memory_order_acquire);
                while (seq1 & 1) { // Wait for writer to finish
                    seq1 = sequence_.load(std::memory_order_acquire);
                }

                // Make a copy of the data.
                data = data_;

                // Check if the sequence number has changed. If it has, the data is
                // inconsistent, so we must retry.
                seq2 = sequence_.load(std::memory_order_acquire);

            } while (seq1 != seq2);

            return data;
        }

        /**
         * @brief Writes new data, to be called only by the single writer thread.
         *
         * This function updates the data in two stages, incrementing a sequence
         * counter to signal the start and end of the write, allowing readers
         * to detect torn reads.
         *
         * @param new_data The new data to write.
         */
        void write(const T& new_data) noexcept {
            // Increment sequence to an odd number, signaling a write is in progress.
            sequence_.fetch_add(1, std::memory_order_release);

            // Perform the write.
            data_ = new_data;

            // Increment sequence to an even number, signaling the write is complete.
            // This makes the new data available to readers.
            sequence_.fetch_add(1, std::memory_order_release);
        }

    private:
        // The sequence counter. Even = stable, Odd = write in progress.
        alignas(64) std::atomic<uint64_t> sequence_;
        
        // The data being protected.
        T data_;
    };

} // namespace hft
