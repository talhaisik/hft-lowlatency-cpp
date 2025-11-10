#include <iostream>
#include <type_traits>
#include <string>
#include <vector>
#include <array>
#include <concepts>
#include <limits>
#include <cstdint>
#include <cassert>

// ============================================================================
// SECTION 1: Function Templates - Type Deduction
// ============================================================================

namespace section1 {

    // Basic function template
    template<typename T>
    T max_value(T a, T b) {
        return (a > b) ? a : b;
    }

    // Template with multiple type parameters
    template<typename T, typename U>
    auto add(T a, U b) -> decltype(a + b) {
        return a + b;
    }

    // Template with explicit return type (C++14 auto return type deduction)
    template<typename T>
    auto square(T value) {
        return value * value;
    }

    // Non-type template parameters (compile-time constants)
    template<typename T, size_t N>
    constexpr size_t array_size(const T(&)[N]) {
        return N;
    }

    // Template with default type parameter (guards against divide-by-zero)
    template<typename T = double>
    T compute_percentage(T value, T total) {
        return total == T{} ? T{} : (value / total) * static_cast<T>(100.0);
    }

    void demo() {
        std::cout << "=== SECTION 1: Function Templates ===" << std::endl;
        
        // Type deduction
        std::cout << "max_value(10, 20) = " << max_value(10, 20) << std::endl;
        std::cout << "max_value(3.14, 2.71) = " << max_value(3.14, 2.71) << std::endl;
        
        // Multiple types with auto return
        std::cout << "add(5, 3.14) = " << add(5, 3.14) << std::endl;
        
        // Auto return type deduction
        std::cout << "square(7) = " << square(7) << std::endl;
        
        // Non-type template parameter
        int arr[] = {1, 2, 3, 4, 5};
        std::cout << "Array size: " << array_size(arr) << std::endl;
        
        // Default template parameter
        std::cout << "Percentage: " << compute_percentage(75.0, 100.0) << "%" << std::endl;
        
        std::cout << std::endl;
    }
}

// ============================================================================
// SECTION 2: Class Templates
// ============================================================================

namespace section2 {

    // Basic class template
    template<typename T>
    class Stack {
    private:
        std::vector<T> data_;
    
    public:
        void push(const T& item) { data_.push_back(item); }
        void push(T&& item) { data_.push_back(std::move(item)); }
        
        T pop() {
            assert(!empty() && "Stack::pop() called on empty stack");
            T item = std::move(data_.back());
            data_.pop_back();
            return item;
        }
        
        [[nodiscard]] bool empty() const { return data_.empty(); }
        [[nodiscard]] size_t size() const { return data_.size(); }
    };

    // Class template with multiple parameters
    template<typename Key, typename Value>
    class KeyValuePair {
    private:
        Key key_;
        Value value_;
    
    public:
        KeyValuePair(Key k, Value v) : key_(std::move(k)), value_(std::move(v)) {}
        
        [[nodiscard]] const Key& key() const { return key_; }
        [[nodiscard]] const Value& value() const { return value_; }
        
        void set_value(Value v) { value_ = std::move(v); }
    };

    // Class template with non-type parameter (fixed-size stack)
    // Note: Uses bool return + out-param style (exception-free / embedded-friendly)
    template<typename T, size_t Capacity>
    class FixedStack {
    private:
        std::array<T, Capacity> data_;  // Zero heap allocation!
        size_t size_ = 0;
    
    public:
        [[nodiscard]] bool push(const T& item) {
            if (size_ >= Capacity) return false;
            data_[size_++] = item;
            return true;
        }
        
        [[nodiscard]] bool push(T&& item) {
            if (size_ >= Capacity) return false;
            data_[size_++] = std::move(item);
            return true;
        }
        
        [[nodiscard]] bool pop(T& item) {
            if (size_ == 0) return false;
            item = std::move(data_[--size_]);
            return true;
        }
        
        [[nodiscard]] bool empty() const { return size_ == 0; }
        [[nodiscard]] size_t size() const { return size_; }
        [[nodiscard]] constexpr size_t capacity() const { return Capacity; }
    };

    void demo() {
        std::cout << "=== SECTION 2: Class Templates ===" << std::endl;
        
        // Basic class template
        Stack<int> int_stack;
        int_stack.push(10);
        int_stack.push(20);
        int_stack.push(30);
        std::cout << "Stack size: " << int_stack.size() << std::endl;
        std::cout << "Popped: " << int_stack.pop() << std::endl;
        
        // Multiple type parameters
        KeyValuePair<std::string, int> kvp("price", 100);
        std::cout << "Key: " << kvp.key() << ", Value: " << kvp.value() << std::endl;
        
        // Fixed-size stack (zero heap allocation!)
        FixedStack<int, 8> fixed_stack;
        for (int i = 0; i < 5; ++i) {
            fixed_stack.push(i * 10);
        }
        std::cout << "Fixed stack capacity: " << fixed_stack.capacity() << std::endl;
        std::cout << "Fixed stack size: " << fixed_stack.size() << std::endl;
        
        std::cout << std::endl;
    }
}

// ============================================================================
// SECTION 3: Template Specialization
// ============================================================================

namespace section3 {

    // Primary template
    template<typename T>
    class Serializer {
    public:
        static std::string serialize(const T& value) {
            return "Generic serialization not implemented";
        }
    };

    // Full specialization for int
    template<>
    class Serializer<int> {
    public:
        static std::string serialize(const int& value) {
            return "INT:" + std::to_string(value);
        }
    };

    // Full specialization for double
    template<>
    class Serializer<double> {
    public:
        static std::string serialize(const double& value) {
            return "DOUBLE:" + std::to_string(value);
        }
    };

    // Full specialization for std::string
    template<>
    class Serializer<std::string> {
    public:
        static std::string serialize(const std::string& value) {
            return "STRING:\"" + value + "\"";
        }
    };

    // Partial specialization for pointers
    template<typename T>
    class Serializer<T*> {
    public:
        static std::string serialize(T* const& ptr) {
            if (ptr == nullptr) {
                return "NULLPTR";
            }
            return "PTR[" + Serializer<T>::serialize(*ptr) + "]";
        }
    };

    // Partial specialization for std::vector
    template<typename T>
    class Serializer<std::vector<T>> {
    public:
        static std::string serialize(const std::vector<T>& vec) {
            std::string result = "VECTOR[";
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i > 0) result += ", ";
                result += Serializer<T>::serialize(vec[i]);
            }
            result += "]";
            return result;
        }
    };

    // Function template specialization example
    template<typename T>
    T fast_abs(T value) {
        return value < 0 ? -value : value;
    }

    // Specialization for unsigned types (no-op)
    template<>
    unsigned int fast_abs<unsigned int>(unsigned int value) {
        return value; // Already positive
    }

    void demo() {
        std::cout << "=== SECTION 3: Template Specialization ===" << std::endl;
        
        // Test serializers
        std::cout << Serializer<int>::serialize(42) << std::endl;
        std::cout << Serializer<double>::serialize(3.14159) << std::endl;
        std::cout << Serializer<std::string>::serialize("Hello") << std::endl;
        
        int value = 100;
        int* ptr = &value;
        std::cout << Serializer<int*>::serialize(ptr) << std::endl;
        
        std::vector<int> vec = {1, 2, 3, 4, 5};
        std::cout << Serializer<std::vector<int>>::serialize(vec) << std::endl;
        
        // Function template specialization
        std::cout << "fast_abs(-42) = " << fast_abs(-42) << std::endl;
        std::cout << "fast_abs(42u) = " << fast_abs(42u) << std::endl;
        
        std::cout << std::endl;
    }
}

// ============================================================================
// SECTION 4: Variadic Templates
// ============================================================================

namespace section4 {

    // Basic variadic template - recursive case
    template<typename T>
    void print(const T& value) {
        std::cout << value << std::endl;
    }

    template<typename T, typename... Args>
    void print(const T& first, const Args&... rest) {
        std::cout << first << " ";
        print(rest...);
    }

    // Fold expressions (C++17) - much cleaner!
    template<typename... Args>
    void print_fold(const Args&... args) {
        (std::cout << ... << args) << std::endl;
    }

    // Sum with fold expression
    template<typename... Args>
    auto sum(Args... args) {
        return (... + args); // Left fold: ((arg1 + arg2) + arg3) + ...
    }

    // Count number of arguments
    template<typename... Args>
    constexpr size_t count_args(Args... args) {
        return sizeof...(args);
    }

    // Check if all arguments are the same type
    template<typename T, typename... Args>
    constexpr bool all_same_type() {
        return (std::is_same_v<T, Args> && ...);
    }

    // Practical example: Low-latency logging with variadic templates
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

    template<typename... Args>
    void log_message(LogLevel level, const Args&... args) {
        // Note: Array order must match enum order
        const char* level_str[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        std::cout << level_str[static_cast<int>(level)] << " ";
        print(args...);
    }

    // Variadic class template: Tuple-like structure
    template<typename... Types>
    class TypeList {
    public:
        static constexpr size_t size = sizeof...(Types);
    };

    void demo() {
        std::cout << "=== SECTION 4: Variadic Templates ===" << std::endl;
        
        // Recursive print
        std::cout << "Recursive print: ";
        print(1, 2.5, "hello", 'x');
        
        // Fold expression print
        std::cout << "Fold print: ";
        print_fold(10, 20, 30, 40);
        
        // Sum with fold
        std::cout << "Sum: " << sum(1, 2, 3, 4, 5) << " = 15" << std::endl;
        
        // Count arguments
        std::cout << "Arg count: " << count_args(1, 2, 3, 4, 5, 6) << std::endl;
        
        // Type checking
        std::cout << "All int? " << std::boolalpha 
                  << all_same_type<int, int, int, int>() << std::endl;
        std::cout << "Mixed types? " << all_same_type<int, int, double>() << std::endl;
        
        // Logging example
        log_message(LogLevel::INFO, "Order", 12345, "executed at price", 100.50);
        log_message(LogLevel::ERROR, "Connection lost to exchange");
        
        // TypeList
        using MyTypes = TypeList<int, double, std::string, char>;
        std::cout << "TypeList size: " << MyTypes::size << std::endl;
        
        std::cout << std::endl;
    }
}

// ============================================================================
// SECTION 5: SFINAE (Substitution Failure Is Not An Error)
// ============================================================================

namespace section5 {

    // Modern approach: C++20 Concepts (cleaner than SFINAE!)
    template<std::integral T>
    T safe_divide_concept(T a, T b) {
        if (b == 0) return 0;
        return a / b;
    }

    template<std::floating_point T>
    T safe_divide_concept(T a, T b) {
        if (b == 0.0) return std::numeric_limits<T>::quiet_NaN();
        return a / b;
    }

    // SFINAE with std::enable_if - enable for integral types (older C++11 style)
    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, T>::type
    safe_divide(T a, T b) {
        if (b == 0) return 0;
        return a / b;
    }

    // SFINAE - enable for floating-point types
    template<typename T>
    typename std::enable_if<std::is_floating_point<T>::value, T>::type
    safe_divide(T a, T b) {
        if (b == 0.0) return std::numeric_limits<T>::quiet_NaN();
        return a / b;
    }

    // Modern SFINAE with std::enable_if_t (C++14)
    template<typename T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
    T clamp(T value, T min_val, T max_val) {
        if (value < min_val) return min_val;
        if (value > max_val) return max_val;
        return value;
    }

    // SFINAE for detecting member functions
    template<typename T>
    class has_size_method {
    private:
        template<typename U>
        static auto test(int) -> decltype(std::declval<U>().size(), std::true_type{});
        
        template<typename>
        static std::false_type test(...);
    
    public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };

    // Use the trait
    template<typename T>
    std::enable_if_t<has_size_method<T>::value, size_t>
    get_size(const T& container) {
        return container.size();
    }

    template<typename T>
    std::enable_if_t<!has_size_method<T>::value, size_t>
    get_size(const T&) {
        return 0; // Fallback: return 0 for types without .size()
    }

    // Tag dispatch - alternative to SFINAE
    template<typename T>
    T process_impl(T value, std::true_type /* is_pointer */) {
        std::cout << "Processing pointer type" << std::endl;
        return value;
    }

    template<typename T>
    T process_impl(T value, std::false_type /* is_pointer */) {
        std::cout << "Processing non-pointer type" << std::endl;
        return value;
    }

    template<typename T>
    T process(T value) {
        return process_impl(value, std::is_pointer<T>{});
    }

    void demo() {
        std::cout << "=== SECTION 5: SFINAE & Concepts ===" << std::endl;
        
        // C++20 Concepts (modern, clean!)
        std::cout << "Concepts (C++20):" << std::endl;
        std::cout << "  safe_divide_concept(10, 0) [int] = " << safe_divide_concept(10, 0) << std::endl;
        std::cout << "  safe_divide_concept(10.0, 2.0) [double] = " << safe_divide_concept(10.0, 2.0) << std::endl;
        std::cout << std::endl;
        
        // SFINAE (older style, still widely used)
        std::cout << "SFINAE (C++11):" << std::endl;
        std::cout << "  safe_divide(10, 0) [int] = " << safe_divide(10, 0) << std::endl;
        std::cout << "  safe_divide(10.0, 0.0) [double] = " << safe_divide(10.0, 0.0) << std::endl;
        std::cout << "  safe_divide(10.0, 2.0) [double] = " << safe_divide(10.0, 2.0) << std::endl;
        
        // clamp
        std::cout << "clamp(15, 0, 10) = " << clamp(15, 0, 10) << std::endl;
        std::cout << "clamp(5, 0, 10) = " << clamp(5, 0, 10) << std::endl;
        
        // Member detection
        std::vector<int> vec = {1, 2, 3};
        int value = 42;
        std::cout << "Vector size: " << get_size(vec) << std::endl;
        std::cout << "Int 'size': " << get_size(value) << std::endl;
        
        // Tag dispatch
        int* ptr = nullptr;
        process(42);
        process(ptr);
        
        std::cout << std::endl;
    }
}

// ============================================================================
// SECTION 6: Type Traits - Building Blocks of Metaprogramming
// ============================================================================

namespace section6 {

    // Custom type trait
    template<typename T>
    struct is_price_type : std::false_type {};

    template<>
    struct is_price_type<int64_t> : std::true_type {};

    template<>
    struct is_price_type<double> : std::true_type {};

    // Helper variable template (C++17)
    template<typename T>
    inline constexpr bool is_price_type_v = is_price_type<T>::value;

    // Conditional type selection (shown for illustration; use std::conditional in real code)
    template<bool Condition, typename TrueType, typename FalseType>
    struct conditional_type {
        using type = TrueType;
    };

    template<typename TrueType, typename FalseType>
    struct conditional_type<false, TrueType, FalseType> {
        using type = FalseType;
    };

    // Custom type trait: is_container
    // Note: This will match std::string (which is container-like). Container detection is fuzzy!
    template<typename T, typename = void>
    struct is_container : std::false_type {};

    template<typename T>
    struct is_container<T, std::void_t<
        typename T::value_type,
        typename T::iterator,
        decltype(std::declval<T>().begin()),
        decltype(std::declval<T>().end())
    >> : std::true_type {};

    template<typename T>
    inline constexpr bool is_container_v = is_container<T>::value;

    // Compile-time type inspection
    template<typename T>
    void inspect_type() {
        std::cout << "Type inspection:" << std::endl;
        std::cout << "  is_integral: " << std::is_integral_v<T> << std::endl;
        std::cout << "  is_floating_point: " << std::is_floating_point_v<T> << std::endl;
        std::cout << "  is_pointer: " << std::is_pointer_v<T> << std::endl;
        std::cout << "  is_reference: " << std::is_reference_v<T> << std::endl;
        std::cout << "  is_const: " << std::is_const_v<T> << std::endl;
        std::cout << "  is_trivially_copyable: " << std::is_trivially_copyable_v<T> << std::endl;
        std::cout << "  sizeof: " << sizeof(T) << " bytes" << std::endl;
        std::cout << "  is_container: " << is_container_v<T> << std::endl;
    }

    void demo() {
        std::cout << "=== SECTION 6: Type Traits ===" << std::endl;
        
        // Custom price type trait
        std::cout << std::boolalpha;
        std::cout << "is_price_type<int64_t>: " << is_price_type_v<int64_t> << std::endl;
        std::cout << "is_price_type<double>: " << is_price_type_v<double> << std::endl;
        std::cout << "is_price_type<float>: " << is_price_type_v<float> << std::endl;
        std::cout << std::endl;
        
        // Conditional type
        using fast_int = typename conditional_type<sizeof(int) == 4, int32_t, int64_t>::type;
        std::cout << "fast_int size: " << sizeof(fast_int) << " bytes" << std::endl;
        std::cout << std::endl;
        
        // Container detection
        std::cout << "is_container<std::vector<int>>: " << is_container_v<std::vector<int>> << std::endl;
        std::cout << "is_container<std::string>: " << is_container_v<std::string> << std::endl;
        std::cout << "is_container<int>: " << is_container_v<int> << std::endl;
        std::cout << std::endl;
        
        // Type inspection
        inspect_type<int>();
        std::cout << std::endl;
        inspect_type<std::vector<double>>();
        
        std::cout << std::endl;
    }
}

// ============================================================================
// SECTION 7: Practical HFT Example - Generic Message Parser
// ============================================================================

namespace section7 {

    // Message types
    struct OrderMessage {
        uint64_t order_id;
        double price;
        uint32_t quantity;
        char side; // 'B' or 'S'
    };

    struct TradeMessage {
        uint64_t trade_id;
        double price;
        uint32_t quantity;
    };

    struct QuoteMessage {
        double bid_price;
        double ask_price;
        uint32_t bid_size;
        uint32_t ask_size;
    };

    // Generic message handler using templates
    template<typename MessageType>
    class MessageHandler {
    public:
        virtual ~MessageHandler() = default;
        virtual void handle(const MessageType& msg) = 0;
    };

    // Specialized handlers
    class OrderHandler : public MessageHandler<OrderMessage> {
    public:
        void handle(const OrderMessage& msg) override {
            std::cout << "Order: ID=" << msg.order_id 
                      << " Price=" << msg.price 
                      << " Qty=" << msg.quantity 
                      << " Side=" << msg.side << std::endl;
        }
    };

    class TradeHandler : public MessageHandler<TradeMessage> {
    public:
        void handle(const TradeMessage& msg) override {
            std::cout << "Trade: ID=" << msg.trade_id 
                      << " Price=" << msg.price 
                      << " Qty=" << msg.quantity << std::endl;
        }
    };

    // Generic processor with template template parameter
    template<template<typename> class Handler, typename MessageType>
    class MessageProcessor {
    private:
        Handler<MessageType> handler_;
    
    public:
        void process(const MessageType& msg) {
            handler_.handle(msg);
        }
    };

    // Type-safe message buffer with compile-time size
    template<typename MessageType, size_t Capacity>
    class MessageBuffer {
    private:
        std::array<MessageType, Capacity> buffer_;
        size_t write_pos_ = 0;
        size_t read_pos_ = 0;
        size_t count_ = 0;
    
    public:
        [[nodiscard]] bool push(const MessageType& msg) {
            if (count_ >= Capacity) return false;
            buffer_[write_pos_] = msg;
            write_pos_ = (write_pos_ + 1) % Capacity;
            ++count_;
            return true;
        }
        
        [[nodiscard]] bool pop(MessageType& msg) {
            if (count_ == 0) return false;
            msg = buffer_[read_pos_];
            read_pos_ = (read_pos_ + 1) % Capacity;
            --count_;
            return true;
        }
        
        [[nodiscard]] bool empty() const { return count_ == 0; }
        [[nodiscard]] bool full() const { return count_ >= Capacity; }
        [[nodiscard]] size_t size() const { return count_; }
        [[nodiscard]] constexpr size_t capacity() const { return Capacity; }
    };

    void demo() {
        std::cout << "=== SECTION 7: Practical HFT Example ===" << std::endl;
        
        // Create messages
        OrderMessage order{12345, 100.50, 1000, 'B'};
        TradeMessage trade{67890, 100.52, 500};
        
        // Handle messages
        OrderHandler order_handler;
        TradeHandler trade_handler;
        
        order_handler.handle(order);
        trade_handler.handle(trade);
        
        // Message buffer with zero heap allocation
        MessageBuffer<OrderMessage, 1024> order_buffer;
        
        // Push some orders
        for (uint64_t i = 0; i < 5; ++i) {
            OrderMessage msg{i, 100.0 + i * 0.1, 100, 'B'};
            order_buffer.push(msg);
        }
        
        std::cout << "\nBuffer size: " << order_buffer.size() 
                  << "/" << order_buffer.capacity() << std::endl;
        
        // Pop and process
        std::cout << "\nProcessing orders from buffer:" << std::endl;
        OrderMessage msg;
        while (order_buffer.pop(msg)) {
            order_handler.handle(msg);
        }
        
        std::cout << std::endl;
    }
}

// ============================================================================
// SECTION 8: Key Takeaways and Best Practices
// ============================================================================

namespace section8 {

    void summary() {
        std::cout << "=== SECTION 8: Templates - Key Takeaways ===" << std::endl;
        std::cout << std::endl;
        
        std::cout << "What You've Learned:" << std::endl;
        std::cout << "  [1] Function Templates    - Generic functions with type deduction" << std::endl;
        std::cout << "  [2] Class Templates       - Generic containers and data structures" << std::endl;
        std::cout << "  [3] Specialization        - Custom behavior for specific types" << std::endl;
        std::cout << "  [4] Variadic Templates    - Functions accepting any number of arguments" << std::endl;
        std::cout << "  [5] SFINAE                - Compile-time function overload selection" << std::endl;
        std::cout << "  [6] Type Traits           - Compile-time type queries" << std::endl;
        std::cout << "  [7] Practical Example     - HFT message system with templates" << std::endl;
        std::cout << std::endl;
        
        std::cout << "When to Use Templates:" << std::endl;
        std::cout << "  Generic algorithms (sort, search, filter)" << std::endl;
        std::cout << "  Type-safe containers (no runtime overhead)" << std::endl;
        std::cout << "  Compile-time polymorphism (zero-cost abstraction)" << std::endl;
        std::cout << "  Performance-critical code (enables inlining)" << std::endl;
        std::cout << "  Fixed-size data structures (compile-time capacity)" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Templates vs Virtual Functions:" << std::endl;
        std::cout << "  Templates:        Compile-time polymorphism, faster, larger binary" << std::endl;
        std::cout << "  Virtual Functions: Runtime polymorphism, flexible, smaller binary" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Best Practices:" << std::endl;
        std::cout << "  1. Use static_assert for compile-time checks" << std::endl;
        std::cout << "  2. Provide clear error messages with concepts (C++20)" << std::endl;
        std::cout << "  3. Use constexpr for compile-time computation" << std::endl;
        std::cout << "  4. Avoid template code bloat (extract non-dependent code)" << std::endl;
        std::cout << "  5. Profile before optimizing (measure, don't guess!)" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Next Topics to Explore:" << std::endl;
        std::cout << "  -> C++20 Concepts (modern alternative to SFINAE)" << std::endl;
        std::cout << "  -> constexpr and compile-time computation" << std::endl;
        std::cout << "  -> Template template parameters" << std::endl;
        std::cout << "  -> Expression templates (advanced)" << std::endl;
        std::cout << std::endl;
    }
}

// ============================================================================
// Main - Run All Demonstrations
// ============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "    MODERN C++ TEMPLATES DEEP DIVE" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    section1::demo();
    section2::demo();
    section3::demo();
    section4::demo();
    section5::demo();
    section6::demo();
    section7::demo();
    section8::summary();

    std::cout << "========================================" << std::endl;
    std::cout << "    ALL DEMONSTRATIONS COMPLETE" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}

