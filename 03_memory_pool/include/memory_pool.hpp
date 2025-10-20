#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <new>
#include <stdexcept>

namespace hft::memory {

    /**
     * @brief High-performance fixed-size memory pool for HFT applications
     * 
     * Pre-allocates a fixed number of objects at construction time, eliminating
     * heap allocation overhead during trading operations. Provides O(1) allocation
     * and deallocation using an intrusive free list.
     * 
     * Performance characteristics:
     * - Allocation:   ~5-10 CPU cycles (vs ~500 for new/delete)
     * - Deallocation: ~5-10 CPU cycles
     * - Zero fragmentation
     * - Predictable latency
     * 
     * Thread Safety: NOT thread-safe. Use thread-local pools or external synchronization.
     * 
     * @tparam T Type of objects to pool
     * @tparam PoolSize Maximum number of objects in the pool
     * 
     * Example usage:
     * @code
     * MemoryPool<Order, 10000> order_pool;
     * 
     * // Allocate (construct in-place)
     * Order* order = order_pool.allocate(order_id, price, quantity);
     * 
     * // Use the order...
     * process(order);
     * 
     * // Deallocate (destructor called, returned to pool)
     * order_pool.deallocate(order);
     * @endcode
     */
    template<typename T, size_t PoolSize>
    class MemoryPool {
    public:
        static_assert(PoolSize > 0, "Pool size must be greater than zero");
        static_assert(sizeof(T) >= sizeof(void*), 
            "Type must be at least pointer-sized for free list");
        
        /**
         * @brief Construct the memory pool and pre-allocate all memory
         * 
         * Allocates one large contiguous block for all objects and initializes
         * the free list. All objects are ready for allocation.
         * 
         * Time complexity: O(PoolSize) - only done once at startup
         */
        MemoryPool() {
            // Allocate one large block for all objects
            // Using alignas ensures proper alignment for type T
            storage_ = static_cast<std::byte*>(
                ::operator new(sizeof(Slot) * PoolSize, std::align_val_t{alignof(Slot)})
            );
            
            // Initialize free list: link all slots together
            // Each slot points to the next available slot
            free_list_ = reinterpret_cast<Slot*>(storage_);
            
            Slot* current = free_list_;
            for (size_t i = 0; i < PoolSize - 1; ++i) {
                // Get pointer to next slot in array
                Slot* next = reinterpret_cast<Slot*>(
                    storage_ + (i + 1) * sizeof(Slot)
                );
                current->next = next;
                current = next;
            }
            
            // Last slot points to nullptr (end of free list)
            current->next = nullptr;
            
            available_count_ = PoolSize;
        }
        
        /**
         * @brief Destructor - frees the entire memory block
         * 
         * WARNING: Does NOT call destructors on allocated objects!
         * User must deallocate all objects before pool destruction.
         */
        ~MemoryPool() noexcept {
            // In debug builds, could check if all objects were returned
            // For now, just free the memory
            ::operator delete(storage_, std::align_val_t{alignof(Slot)});
        }
        
        // Prevent copying and moving (pools should be stationary)
        MemoryPool(const MemoryPool&) = delete;
        MemoryPool& operator=(const MemoryPool&) = delete;
        MemoryPool(MemoryPool&&) = delete;
        MemoryPool& operator=(MemoryPool&&) = delete;
        
        /**
         * @brief Allocate and construct an object from the pool
         * 
         * Pops a slot from the free list and constructs a T object in that memory
         * using perfect forwarding of constructor arguments.
         * 
         * @tparam Args Constructor argument types
         * @param args Constructor arguments
         * @return Pointer to constructed object, or nullptr if pool exhausted
         * 
         * Time complexity: O(1)
         * 
         * Example:
         * @code
         * auto* obj = pool.allocate(arg1, arg2, arg3);
         * if (obj) {
         *     // Use object
         * } else {
         *     // Pool exhausted!
         * }
         * @endcode
         */
        template<typename... Args>
        [[nodiscard]] T* allocate(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
            // Check if pool is exhausted
            if (free_list_ == nullptr) {
                return nullptr;  // Could throw if preferred
            }
            
            // Pop first slot from free list
            Slot* slot = free_list_;
            free_list_ = slot->next;
            --available_count_;
            
            // Get pointer to the memory where we'll construct T
            void* memory = slot;
            
            // Construct T in-place using placement new
            // Perfect forwarding ensures efficient argument passing
            T* object = new (memory) T(std::forward<Args>(args)...);
            
            return object;
        }
        
        /**
         * @brief Destruct and return an object to the pool
         * 
         * Calls the destructor on the object and returns its memory to the free list.
         * 
         * @param object Pointer to object previously allocated from this pool
         * 
         * Time complexity: O(1)
         * 
         * WARNING: Passing an object not allocated from this pool is undefined behavior!
         * WARNING: Using the object after deallocation is undefined behavior!
         */
        void deallocate(T* object) noexcept {
            if (object == nullptr) {
                return;  // Allow deallocating nullptr
            }
            
            // Call destructor explicitly
            object->~T();
            
            // Return slot to free list
            Slot* slot = reinterpret_cast<Slot*>(object);
            slot->next = free_list_;
            free_list_ = slot;
            ++available_count_;
        }
        
        /**
         * @brief Get number of available (unallocated) objects
         * @return Number of objects that can be allocated
         */
        [[nodiscard]] size_t available() const noexcept {
            return available_count_;
        }
        
        /**
         * @brief Get total pool capacity
         * @return Maximum number of objects (fixed at compile time)
         */
        [[nodiscard]] constexpr size_t capacity() const noexcept {
            return PoolSize;
        }
        
        /**
         * @brief Check if pool is exhausted
         * @return true if no objects available
         */
        [[nodiscard]] bool empty() const noexcept {
            return free_list_ == nullptr;
        }
        
        /**
         * @brief Check if all objects are available
         * @return true if all objects are in the free list
         */
        [[nodiscard]] bool full() const noexcept {
            return available_count_ == PoolSize;
        }
        
    private:
        /**
         * @brief Union for memory reuse trick
         * 
         * When a slot is FREE: It stores a 'next' pointer (part of free list)
         * When a slot is ALLOCATED: It stores a T object
         * 
         * This intrusive free list approach requires zero extra memory overhead!
         * 
         * Key insight: We can reuse the object's memory to store the free list
         * pointer because the object isn't constructed yet (or was destroyed).
         */
        union Slot {
            T object;      // When allocated: actual object lives here
            Slot* next;    // When free: pointer to next free slot
            
            // Union doesn't construct/destruct members automatically
            // We handle construction/destruction manually with placement new
            Slot() {}  // Trivial constructor
            ~Slot() {} // Trivial destructor
        };
        
        // Ensure proper alignment for both T and pointer
        // This is critical for performance and correctness
        static_assert(alignof(Slot) >= alignof(T), "Alignment mismatch");
        static_assert(alignof(Slot) >= alignof(void*), "Alignment mismatch");
        
        // Pool storage: one contiguous block of memory
        std::byte* storage_{nullptr};
        
        // Free list: singly-linked list of available slots
        // Head of the list (next slot to allocate)
        Slot* free_list_{nullptr};
        
        // Track available slots for monitoring
        size_t available_count_{0};
    };
    
    /**
     * @brief RAII wrapper for pool-allocated objects
     * 
     * Similar to std::unique_ptr but for memory pools.
     * Automatically returns object to pool on destruction.
     * 
     * Example:
     * @code
     * MemoryPool<Order, 1000> pool;
     * 
     * {
     *     auto order = make_pooled<Order>(pool, arg1, arg2);
     *     // Use order...
     * } // Automatically returned to pool here
     * @endcode
     */
    template<typename T, size_t PoolSize>
    class PooledPtr {
    public:
        PooledPtr(T* ptr, MemoryPool<T, PoolSize>& pool) noexcept
            : ptr_(ptr), pool_(&pool) {}
        
        ~PooledPtr() noexcept {
            if (ptr_) {
                pool_->deallocate(ptr_);
            }
        }
        
        // Move semantics
        PooledPtr(PooledPtr&& other) noexcept
            : ptr_(other.ptr_), pool_(other.pool_) {
            other.ptr_ = nullptr;
        }
        
        PooledPtr& operator=(PooledPtr&& other) noexcept {
            if (this != &other) {
                if (ptr_) {
                    pool_->deallocate(ptr_);
                }
                ptr_ = other.ptr_;
                pool_ = other.pool_;
                other.ptr_ = nullptr;
            }
            return *this;
        }
        
        // No copying
        PooledPtr(const PooledPtr&) = delete;
        PooledPtr& operator=(const PooledPtr&) = delete;
        
        // Smart pointer interface
        T* get() const noexcept { return ptr_; }
        T& operator*() const noexcept { return *ptr_; }
        T* operator->() const noexcept { return ptr_; }
        explicit operator bool() const noexcept { return ptr_ != nullptr; }
        
        // Release ownership (won't be returned to pool)
        T* release() noexcept {
            T* temp = ptr_;
            ptr_ = nullptr;
            return temp;
        }
        
    private:
        T* ptr_;
        MemoryPool<T, PoolSize>* pool_;
    };
    
    /**
     * @brief Helper function to create pool-allocated objects with RAII
     * 
     * Similar to std::make_unique but for memory pools.
     * 
     * @tparam T Object type
     * @tparam PoolSize Pool capacity
     * @tparam Args Constructor argument types
     * @param pool Memory pool to allocate from
     * @param args Constructor arguments
     * @return PooledPtr managing the allocated object
     */
    template<typename T, size_t PoolSize, typename... Args>
    [[nodiscard]] PooledPtr<T, PoolSize> make_pooled(
        MemoryPool<T, PoolSize>& pool,
        Args&&... args
    ) {
        T* ptr = pool.allocate(std::forward<Args>(args)...);
        if (!ptr) {
            throw std::bad_alloc();  // Or handle differently
        }
        return PooledPtr<T, PoolSize>(ptr, pool);
    }

} // namespace hft::memory