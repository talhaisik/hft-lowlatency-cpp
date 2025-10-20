#pragma once

#include <utility>  // For std::forward, std::move
#include <type_traits>

namespace hft::smart {

    /**
     * @brief Simplified unique_ptr implementation for learning
     * 
     * Demonstrates:
     * - RAII ownership
     * - Move-only semantics
     * - Custom deleters
     * - Empty base optimization
     * 
     * This is a teaching implementation - std::unique_ptr has more features!
     */
    
    // Default Deleter
    
    /**
     * @brief Default deleter - just calls delete
     */
    template<typename T>
    struct DefaultDeleter {
        void operator()(T* ptr) const noexcept {
            delete ptr;
        }
    };
    
    /**
     * @brief Array specialization - calls delete[]
     */
    template<typename T>
    struct DefaultDeleter<T[]> {
        void operator()(T* ptr) const noexcept {
            delete[] ptr;
        }
    };
    
    // UniquePtr Implementation
    
    /**
     * @brief Exclusive ownership smart pointer
     * 
     * Key characteristics:
     * - Owns the object exclusively (no sharing)
     * - Move-only (cannot copy)
     * - Zero overhead with default deleter
     * - Custom deleters supported
     * 
     * @tparam T Type of managed object
     * @tparam Deleter Callable object that cleans up T*
     */
    template<typename T, typename Deleter = DefaultDeleter<T>>
    class UniquePtr {
    public:
        // Constructors
        
        /**
         * @brief Default constructor - empty pointer
         */
        constexpr UniquePtr() noexcept : ptr_(nullptr), deleter_() {}
        
        /**
         * @brief nullptr constructor
         */
        constexpr UniquePtr(std::nullptr_t) noexcept : ptr_(nullptr), deleter_() {}
        
        /**
         * @brief Take ownership of raw pointer
         * 
         * @param ptr Pointer to manage (can be nullptr)
         */
        explicit UniquePtr(T* ptr) noexcept : ptr_(ptr), deleter_() {}
        
        /**
         * @brief Take ownership with custom deleter
         * 
         * @param ptr Pointer to manage
         * @param deleter Custom deleter to use
         */
        UniquePtr(T* ptr, Deleter deleter) noexcept
            : ptr_(ptr), deleter_(std::move(deleter)) {}
        
        /**
         * @brief Move constructor - transfer ownership
         * 
         * After move, other.ptr_ is nullptr
         */
        UniquePtr(UniquePtr&& other) noexcept
            : ptr_(other.ptr_), deleter_(std::move(other.deleter_)) {
            other.ptr_ = nullptr;
        }
        
        /**
         * @brief Destructor - cleanup managed object
         * 
         * Calls deleter on ptr_ if not nullptr
         */
        ~UniquePtr() noexcept {
            if (ptr_) {
                deleter_(ptr_);
            }
        }
        
        // Assignment Operators
        
        /**
         * @brief Move assignment - transfer ownership
         * 
         * Deletes currently managed object, then takes ownership of other's
         */
        UniquePtr& operator=(UniquePtr&& other) noexcept {
            if (this != &other) {
                // Delete our current object
                if (ptr_) {
                    deleter_(ptr_);
                }
                
                // Take ownership of other's object
                ptr_ = other.ptr_;
                deleter_ = std::move(other.deleter_);
                
                // Leave other empty
                other.ptr_ = nullptr;
            }
            return *this;
        }
        
        /**
         * @brief Assign nullptr - delete managed object
         */
        UniquePtr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }
        
        // Delete copy operations - unique ownership!
        UniquePtr(const UniquePtr&) = delete;
        UniquePtr& operator=(const UniquePtr&) = delete;
        
        // Observers
        
        /**
         * @brief Get raw pointer without releasing ownership
         */
        T* get() const noexcept {
            return ptr_;
        }
        
        /**
         * @brief Dereference operator
         */
        T& operator*() const noexcept {
            return *ptr_;
        }
        
        /**
         * @brief Member access operator
         */
        T* operator->() const noexcept {
            return ptr_;
        }
        
        /**
         * @brief Check if pointer is not null
         */
        explicit operator bool() const noexcept {
            return ptr_ != nullptr;
        }
        
        /**
         * @brief Get reference to deleter
         */
        Deleter& get_deleter() noexcept {
            return deleter_;
        }
        
        const Deleter& get_deleter() const noexcept {
            return deleter_;
        }
        
        // Modifiers
        
        /**
         * @brief Release ownership without deleting
         * 
         * Returns the pointer and sets internal pointer to nullptr.
         * Caller is responsible for deleting the returned pointer.
         * 
         * @return The previously managed pointer
         */
        T* release() noexcept {
            T* old_ptr = ptr_;
            ptr_ = nullptr;
            return old_ptr;
        }
        
        /**
         * @brief Replace managed pointer
         * 
         * Deletes currently managed object (if any) and takes ownership of ptr.
         * 
         * @param ptr New pointer to manage (can be nullptr)
         */
        void reset(T* ptr = nullptr) noexcept {
            T* old_ptr = ptr_;
            ptr_ = ptr;
            if (old_ptr) {
                deleter_(old_ptr);
            }
        }
        
        /**
         * @brief Swap with another UniquePtr
         */
        void swap(UniquePtr& other) noexcept {
            std::swap(ptr_, other.ptr_);
            std::swap(deleter_, other.deleter_);
        }
        
    private:
        T* ptr_;           // The managed pointer
        Deleter deleter_;  // The deleter callable
        
        // Note: With empty deleter (like DefaultDeleter), 
        // Empty Base Optimization makes deleter_ take 0 bytes!
        // sizeof(UniquePtr<T>) == sizeof(T*) in that case
    };
    
    // Helper Functions
    
    /**
     * @brief Create UniquePtr (like std::make_unique)
     * 
     * Safer than UniquePtr<T>(new T(...)) because:
     * - Exception-safe (no leak if constructor throws)
     * - More concise
     * - Clear intent
     * 
     * @tparam T Type to construct
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return UniquePtr managing new T
     */
    template<typename T, typename... Args>
    UniquePtr<T> make_unique(Args&&... args) {
        return UniquePtr<T>(new T(std::forward<Args>(args)...));
    }
    
    /**
     * @brief Comparison operators
     */
    template<typename T, typename D>
    bool operator==(const UniquePtr<T, D>& lhs, std::nullptr_t) noexcept {
        return lhs.get() == nullptr;
    }
    
    template<typename T, typename D>
    bool operator==(std::nullptr_t, const UniquePtr<T, D>& rhs) noexcept {
        return rhs.get() == nullptr;
    }
    
    template<typename T, typename D>
    bool operator!=(const UniquePtr<T, D>& lhs, std::nullptr_t) noexcept {
        return lhs.get() != nullptr;
    }
    
    template<typename T, typename D>
    bool operator!=(std::nullptr_t, const UniquePtr<T, D>& rhs) noexcept {
        return rhs.get() != nullptr;
    }
    
} // namespace hft::smart
