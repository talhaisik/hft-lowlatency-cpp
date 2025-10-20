#pragma once

#include <atomic>
#include <utility>
#include <type_traits>

namespace hft::smart {

    /**
     * @brief Control block for reference counting
     *
     * Shared by all shared_ptr instances pointing to the same object.
     * Contains:
     * - Reference count (how many shared_ptrs own the object)
     * - Weak count (how many weak_ptrs reference it)
     * - The actual object pointer
     *
     * Thread-safe: Uses atomic operations for reference counting
     */
    template<typename T>
    struct ControlBlock {
        T* ptr;                          // Pointer to managed object
        std::atomic<size_t> ref_count;   // Number of shared_ptr owners
        std::atomic<size_t> weak_count;  // Number of weak_ptr references

        ControlBlock(T* p)
            : ptr(p)
            , ref_count(1)    // Start with 1 owner
            , weak_count(0)   // No weak_ptrs yet
        {
        }

        // Add a shared owner
        void add_ref() noexcept {
            ref_count.fetch_add(1, std::memory_order_relaxed);
        }

        // Remove a shared owner, return true if last owner
        bool release() noexcept {
            // Decrement and check if we were the last owner
            if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // We were the last owner - delete the object
                delete ptr;
                ptr = nullptr;

                // If no weak_ptrs exist, delete the control block too
                if (weak_count.load(std::memory_order_acquire) == 0) {
                    return true;  // Caller should delete control block
                }
            }
            return false;
        }

        // Add a weak reference
        void add_weak() noexcept {
            weak_count.fetch_add(1, std::memory_order_relaxed);
        }

        // Remove a weak reference, return true if last weak ref
        bool release_weak() noexcept {
            if (weak_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                // Last weak reference and object already deleted
                if (ref_count.load(std::memory_order_acquire) == 0) {
                    return true;  // Caller should delete control block
                }
            }
            return false;
        }

        size_t use_count() const noexcept {
            return ref_count.load(std::memory_order_relaxed);
        }
    };

    // Forward declaration for weak_ptr
    template<typename T>
    class WeakPtr;

    /**
     * @brief Shared ownership smart pointer
     *
     * Multiple shared_ptr instances can own the same object.
     * The object is deleted when the last shared_ptr is destroyed.
     *
     * Uses reference counting:
     * - Each copy increments the count
     * - Each destruction decrements the count
     * - When count reaches 0, object is deleted
     *
     * Thread-safe reference counting (atomic operations)
     *
     * @tparam T Type of managed object
     */
    template<typename T>
    class SharedPtr {
    public:
        // Constructors

        /**
         * @brief Default constructor - empty pointer
         */
        constexpr SharedPtr() noexcept : control_(nullptr) {}

        /**
         * @brief nullptr constructor
         */
        constexpr SharedPtr(std::nullptr_t) noexcept : control_(nullptr) {}

        /**
         * @brief Take ownership of raw pointer
         *
         * Creates new control block with ref_count = 1
         *
         * @param ptr Pointer to manage (can be nullptr)
         */
        explicit SharedPtr(T* ptr) {
            if (ptr) {
                control_ = new ControlBlock<T>(ptr);
            }
            else {
                control_ = nullptr;
            }
        }

        /**
         * @brief Copy constructor - share ownership
         *
         * Increments reference count
         */
        SharedPtr(const SharedPtr& other) noexcept : control_(other.control_) {
            if (control_) {
                control_->add_ref();
            }
        }

        /**
         * @brief Copy constructor from related type (for polymorphism)
         *
         * Allows SharedPtr<Derived> to convert to SharedPtr<Base>
         */
        template<typename U>
        SharedPtr(const SharedPtr<U>& other) noexcept
            : control_(reinterpret_cast<ControlBlock<T>*>(other.control_)) {
            if (control_) {
                control_->add_ref();
            }
        }

        /**
         * @brief Move constructor - transfer ownership
         *
         * No reference count change (just pointer transfer)
         */
        SharedPtr(SharedPtr&& other) noexcept : control_(other.control_) {
            other.control_ = nullptr;
        }

        /**
         * @brief Move constructor from related type (for polymorphism)
         */
        template<typename U>
        SharedPtr(SharedPtr<U>&& other) noexcept
            : control_(reinterpret_cast<ControlBlock<T>*>(other.control_)) {
            other.control_ = nullptr;
        }

        /**
         * @brief Destructor - release ownership
         *
         * Decrements reference count.
         * If count reaches 0, deletes managed object.
         */
        ~SharedPtr() noexcept {
            if (control_) {
                if (control_->release()) {
                    delete control_;
                }
            }
        }

        // Assignment Operators

        /**
         * @brief Copy assignment - share ownership
         */
        SharedPtr& operator=(const SharedPtr& other) noexcept {
            if (this != &other) {
                // Release our current object
                if (control_) {
                    if (control_->release()) {
                        delete control_;
                    }
                }

                // Share other's object
                control_ = other.control_;
                if (control_) {
                    control_->add_ref();
                }
            }
            return *this;
        }

        /**
         * @brief Move assignment - transfer ownership
         */
        SharedPtr& operator=(SharedPtr&& other) noexcept {
            if (this != &other) {
                // Release our current object
                if (control_) {
                    if (control_->release()) {
                        delete control_;
                    }
                }

                // Take other's object
                control_ = other.control_;
                other.control_ = nullptr;
            }
            return *this;
        }

        /**
         * @brief Assign nullptr - release ownership
         */
        SharedPtr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        // Observers

        /**
         * @brief Get raw pointer
         */
        T* get() const noexcept {
            return control_ ? control_->ptr : nullptr;
        }

        /**
         * @brief Dereference operator
         */
        T& operator*() const noexcept {
            return *get();
        }

        /**
         * @brief Member access operator
         */
        T* operator->() const noexcept {
            return get();
        }

        /**
         * @brief Check if not null
         */
        explicit operator bool() const noexcept {
            return get() != nullptr;
        }

        /**
         * @brief Get reference count
         *
         * @return Number of shared_ptr instances owning the object
         */
        size_t use_count() const noexcept {
            return control_ ? control_->use_count() : 0;
        }

        /**
         * @brief Check if this is the only owner
         */
        bool unique() const noexcept {
            return use_count() == 1;
        }

        // Modifiers

        /**
         * @brief Replace managed pointer
         *
         * Releases current object (if any) and takes ownership of ptr.
         *
         * @param ptr New pointer to manage
         */
        void reset(T* ptr = nullptr) {
            SharedPtr temp(ptr);
            swap(temp);
        }

        /**
         * @brief Swap with another SharedPtr
         */
        void swap(SharedPtr& other) noexcept {
            std::swap(control_, other.control_);
        }

    private:
        ControlBlock<T>* control_;  // Pointer to control block

        // Friend declaration for weak_ptr
        template<typename U>
        friend class WeakPtr;

        // Friend declaration for other SharedPtr instantiations (for polymorphism)
        template<typename U>
        friend class SharedPtr;

        // Private constructor for weak_ptr promotion
        explicit SharedPtr(ControlBlock<T>* ctrl) : control_(ctrl) {
            if (control_) {
                control_->add_ref();
            }
        }
    };

    // Helper Functions

    /**
     * @brief Create SharedPtr (like std::make_shared)
     *
     * More efficient than SharedPtr<T>(new T(...)) because:
     * - Single allocation (object + control block together)
     * - Exception-safe
     * - Better cache locality
     *
     * Note: Our simplified version still uses two allocations
     *       Real std::make_shared uses one allocation
     *
     * @tparam T Type to construct
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return SharedPtr managing new T
     */
    template<typename T, typename... Args>
    SharedPtr<T> make_shared(Args&&... args) {
        return SharedPtr<T>(new T(std::forward<Args>(args)...));
    }

    /**
     * @brief Comparison operators
     */
    template<typename T>
    bool operator==(const SharedPtr<T>& lhs, std::nullptr_t) noexcept {
        return lhs.get() == nullptr;
    }

    template<typename T>
    bool operator==(std::nullptr_t, const SharedPtr<T>& rhs) noexcept {
        return rhs.get() == nullptr;
    }

    template<typename T>
    bool operator!=(const SharedPtr<T>& lhs, std::nullptr_t) noexcept {
        return lhs.get() != nullptr;
    }

    template<typename T>
    bool operator!=(std::nullptr_t, const SharedPtr<T>& rhs) noexcept {
        return rhs.get() != nullptr;
    }

    template<typename T>
    bool operator==(const SharedPtr<T>& lhs, const SharedPtr<T>& rhs) noexcept {
        return lhs.get() == rhs.get();
    }

    template<typename T>
    bool operator!=(const SharedPtr<T>& lhs, const SharedPtr<T>& rhs) noexcept {
        return lhs.get() != rhs.get();
    }

} // namespace hft::smart
