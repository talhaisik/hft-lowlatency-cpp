#pragma once

#include "shared_ptr.hpp"

namespace hft::smart {

    /**
     * @brief Weak reference to shared_ptr managed object
     *
     * weak_ptr observes an object without owning it.
     * Does not prevent object deletion (doesn't increment ref_count).
     * Can be converted to shared_ptr to access object (if still alive).
     *
     * Primary use case: Breaking circular references
     *
     * @tparam T Type of observed object
     */
    template<typename T>
    class WeakPtr {
    public:
        // Constructors

        /**
         * @brief Default constructor - empty weak_ptr
         */
        constexpr WeakPtr() noexcept : control_(nullptr) {}

        /**
         * @brief Construct from shared_ptr
         *
         * Observes the object without owning it.
         * Increments weak_count but NOT ref_count.
         *
         * @param shared The shared_ptr to observe
         */
        WeakPtr(const SharedPtr<T>& shared) noexcept : control_(shared.control_) {
            if (control_) {
                control_->add_weak();
            }
        }

        /**
         * @brief Copy constructor
         */
        WeakPtr(const WeakPtr& other) noexcept : control_(other.control_) {
            if (control_) {
                control_->add_weak();
            }
        }

        /**
         * @brief Copy constructor from related type
         */
        template<typename U>
        WeakPtr(const WeakPtr<U>& other) noexcept
            : control_(reinterpret_cast<ControlBlock<T>*>(other.control_)) {
            if (control_) {
                control_->add_weak();
            }
        }

        /**
         * @brief Move constructor
         */
        WeakPtr(WeakPtr&& other) noexcept : control_(other.control_) {
            other.control_ = nullptr;
        }

        /**
         * @brief Move constructor from related type
         */
        template<typename U>
        WeakPtr(WeakPtr<U>&& other) noexcept
            : control_(reinterpret_cast<ControlBlock<T>*>(other.control_)) {
            other.control_ = nullptr;
        }

        /**
         * @brief Destructor
         *
         * Decrements weak_count.
         * If this is last weak_ptr AND object already deleted, deletes control block.
         */
        ~WeakPtr() noexcept {
            if (control_) {
                if (control_->release_weak()) {
                    delete control_;
                }
            }
        }

        // Assignment

        /**
         * @brief Copy assignment
         */
        WeakPtr& operator=(const WeakPtr& other) noexcept {
            if (this != &other) {
                if (control_) {
                    if (control_->release_weak()) {
                        delete control_;
                    }
                }

                control_ = other.control_;
                if (control_) {
                    control_->add_weak();
                }
            }
            return *this;
        }

        /**
         * @brief Move assignment
         */
        WeakPtr& operator=(WeakPtr&& other) noexcept {
            if (this != &other) {
                if (control_) {
                    if (control_->release_weak()) {
                        delete control_;
                    }
                }

                control_ = other.control_;
                other.control_ = nullptr;
            }
            return *this;
        }

        /**
         * @brief Assign from shared_ptr
         */
        WeakPtr& operator=(const SharedPtr<T>& shared) noexcept {
            if (control_) {
                if (control_->release_weak()) {
                    delete control_;
                }
            }

            control_ = shared.control_;
            if (control_) {
                control_->add_weak();
            }
            return *this;
        }

        // Observers

        /**
         * @brief Get number of shared_ptr owners
         *
         * @return 0 if object expired, otherwise number of owners
         */
        size_t use_count() const noexcept {
            return control_ ? control_->use_count() : 0;
        }

        /**
         * @brief Check if observed object has been deleted
         *
         * @return true if no shared_ptr owners remain
         */
        bool expired() const noexcept {
            return use_count() == 0;
        }

        /**
         * @brief Promote to shared_ptr
         *
         * Attempts to create a shared_ptr from this weak_ptr.
         * Returns empty shared_ptr if object has been deleted.
         *
         * Thread-safe: Uses atomic operations to check if object still exists
         *
         * @return shared_ptr owning the object, or empty if expired
         */
        SharedPtr<T> lock() const noexcept {
            // Check if object still exists
            if (!control_ || control_->ref_count.load(std::memory_order_acquire) == 0) {
                return SharedPtr<T>();  // Object expired, return empty
            }

            // Try to increment ref_count
            // Need to check again inside because another thread might delete between checks
            size_t old_count = control_->ref_count.load(std::memory_order_relaxed);
            while (old_count != 0) {
                if (control_->ref_count.compare_exchange_weak(
                    old_count, old_count + 1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                    // Successfully incremented! Object is safe
                    return SharedPtr<T>(control_);
                }
                // CAS failed, try again with updated old_count
            }

            // Object expired during our attempt
            return SharedPtr<T>();
        }

        // Modifiers

        /**
         * @brief Stop observing
         */
        void reset() noexcept {
            if (control_) {
                if (control_->release_weak()) {
                    delete control_;
                }
                control_ = nullptr;
            }
        }

        /**
         * @brief Swap with another weak_ptr
         */
        void swap(WeakPtr& other) noexcept {
            std::swap(control_, other.control_);
        }

    private:
        ControlBlock<T>* control_;

        // Friend for other WeakPtr instantiations
        template<typename U>
        friend class WeakPtr;
    };

} // namespace hft::smart
