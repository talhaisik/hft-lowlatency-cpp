# Smart Pointers: Complete Summary

**Phase 1 Completion: Understanding Modern C++ Memory Management**

---

## 🎯 Module Overview

This module explored the three smart pointers in C++:
1. **unique_ptr** - Exclusive ownership
2. **shared_ptr** - Shared ownership with reference counting
3. **weak_ptr** - Non-owning observation

---

## 📊 Quick Comparison Table

| Feature                  | unique_ptr | shared_ptr      | weak_ptr         |
|--------------------------|------------|-----------------|------------------|
| **Ownership**            | Exclusive  | Shared          | None (observes)  |
| **Copy**                 | No         | Yes             | Yes              |
| **Move**                 | Yes        | Yes             | Yes              |
| **Size**                 | 8 bytes    | 16 bytes        | 8 bytes          |
| **Overhead**             | None       | Control block   | None             |
| **Ref counting**         | No         | Yes (atomic)    | Weak count       |
| **Thread-safe counting** | N/A        | Yes             | Yes              |
| **Can be null**          | Yes        | Yes             | N/A (can expire) |
| **Primary use**          | Default    | Multiple owners | Break cycles     |

---

## 🎓 Key Concepts Mastered

### 1. RAII (Resource Acquisition Is Initialization)

**Core principle:** Tie resource lifetime to object lifetime.

```cpp
{
    auto ptr = std::make_unique<Resource>();
    // Use resource...
} // Automatic cleanup - no manual delete!
```

**Benefits:**
- Exception-safe
- No memory leaks
- Clear ownership
- Predictable cleanup

---

### 2. Move Semantics

**Transfer ownership without copying:**

```cpp
std::unique_ptr<Widget> ptr1 = std::make_unique<Widget>();
std::unique_ptr<Widget> ptr2 = std::move(ptr1);
// ptr1 now nullptr, ptr2 owns the Widget
```

**Why it matters:**
- Exclusive ownership enforced at compile-time
- Zero-cost abstraction
- Clear semantic intent

---

### 3. Reference Counting

**Automatic lifetime management for shared objects:**

```cpp
auto ptr1 = std::make_shared<Widget>();  // ref_count: 1
auto ptr2 = ptr1;                        // ref_count: 2
auto ptr3 = ptr1;                        // ref_count: 3
ptr2.reset();                            // ref_count: 2
// Widget deleted when count reaches 0
```

**Implementation:**
- Control block with atomic counter
- Thread-safe increment/decrement
- Automatic deletion at zero

---

### 4. Weak References

**Observation without ownership:**

```cpp
std::shared_ptr<Widget> shared = std::make_shared<Widget>();
std::weak_ptr<Widget> weak = shared;

// Check if still alive
if (auto locked = weak.lock()) {
    // Use object
} else {
    // Object was deleted
}
```

**Purpose:**
- Break circular references
- Cache that can expire
- Observer pattern

---

## 🔧 Implementation Insights

### unique_ptr Internals

```cpp
template<typename T, typename Deleter>
class unique_ptr {
    T* ptr_;
    Deleter deleter_;
    
    ~unique_ptr() {
        if (ptr_) deleter_(ptr_);
    }
};

// With empty deleter: sizeof(unique_ptr) == sizeof(T*)
// Empty Base Optimization!
```

**Key techniques:**
- Move-only (copy deleted)
- Custom deleters via template parameter
- Zero overhead with default deleter

---

### shared_ptr Internals

```cpp
template<typename T>
struct ControlBlock {
    T* ptr;
    std::atomic<size_t> ref_count;   // Atomic!
    std::atomic<size_t> weak_count;
};

template<typename T>
class shared_ptr {
    ControlBlock<T>* control_;
};
```

**Key techniques:**
- Separate control block allocation
- Atomic operations for thread-safety
- make_shared optimization (single allocation)

---

### weak_ptr Internals

```cpp
template<typename T>
class weak_ptr {
    ControlBlock<T>* control_;
    
    SharedPtr<T> lock() {
        // Atomic CAS to safely promote
        if (control_->ref_count == 0) return {};
        control_->ref_count.fetch_add(1);
        return SharedPtr<T>(control_);
    }
};
```

**Key techniques:**
- Shares control block with shared_ptr
- lock() uses CAS for thread-safe promotion
- Doesn't prevent deletion (doesn't own)

---

## 💡 Design Patterns and Best Practices

### Pattern 1: Factory Functions

```cpp
std::unique_ptr<Shape> create_shape(ShapeType type) {
    switch (type) {
        case Circle: return std::make_unique<Circle>();
        case Square: return std::make_unique<Square>();
    }
}

// Caller gets ownership
auto shape = create_shape(Circle);
```

**Why unique_ptr:**
- Clear ownership transfer
- Move-only prevents accidental copies
- Exception-safe

---

### Pattern 2: Pimpl Idiom

```cpp
// widget.h
class Widget {
    class Impl;
    std::unique_ptr<Impl> pimpl_;
public:
    Widget();
    ~Widget();  // Defined in .cpp after Impl is complete
};

// widget.cpp
class Widget::Impl {
    // Implementation details hidden
};

Widget::Widget() : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;  // unique_ptr handles cleanup
```

**Benefits:**
- Hide implementation
- Reduce compile dependencies
- Automatic cleanup

---

### Pattern 3: Observer Pattern with weak_ptr

```cpp
class Subject {
    std::vector<std::weak_ptr<Observer>> observers_;
    
public:
    void attach(std::shared_ptr<Observer> obs) {
        observers_.push_back(obs);  // weak_ptr doesn't prevent deletion
    }
    
    void notify(const Event& event) {
        // Remove expired observers while notifying
        auto it = observers_.begin();
        while (it != observers_.end()) {
            if (auto obs = it->lock()) {
                obs->on_event(event);
                ++it;
            } else {
                it = observers_.erase(it);  // Expired, remove
            }
        }
    }
};
```

**Benefits:**
- Observers can be destroyed anytime
- No dangling pointers
- Automatic cleanup of dead observers

---

### Pattern 4: Memory Pool Integration

```cpp
// Custom deleter for unique_ptr with memory pool
template<typename T, size_t Size>
struct PoolDeleter {
    MemoryPool<T, Size>* pool;
    
    void operator()(T* ptr) const {
        pool->deallocate(ptr);
    }
};

// Usage
MemoryPool<Order, 1000> pool;
std::unique_ptr<Order, PoolDeleter<Order, 1000>> order(
    pool.allocate(1, 100.0),
    PoolDeleter<Order, 1000>{&pool}
);

// Or with shared_ptr for multiple owners + pool reuse
std::shared_ptr<Order> shared_order(
    pool.allocate(1, 100.0),
    PoolDeleter<Order, 1000>{&pool}
);
```

**Connection to your work:**
- PooledPtr is essentially unique_ptr with pool deleter
- Can combine smart pointers with custom allocation strategies
- Best of both worlds: RAII + performance

---

## 🚨 Common Pitfalls and Solutions

### Pitfall 1: Creating shared_ptr from Raw Pointer Twice

```cpp
// ❌ WRONG
Widget* raw = new Widget();
std::shared_ptr<Widget> p1(raw);
std::shared_ptr<Widget> p2(raw);  // Two control blocks!
// Double delete when both destroyed! 💥

// ✅ CORRECT
auto p1 = std::make_shared<Widget>();
auto p2 = p1;  // Share same control block
```

---

### Pitfall 2: Circular References

```cpp
// ❌ WRONG
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;  // Cycle!
};

auto n1 = std::make_shared<Node>();
auto n2 = std::make_shared<Node>();
n1->next = n2;
n2->prev = n1;  // Memory leak!

// ✅ CORRECT
struct Node {
    std::shared_ptr<Node> next;    // Forward: owns
    std::weak_ptr<Node> prev;      // Back: observes
};
```

---

### Pitfall 3: Returning this as shared_ptr

```cpp
// ❌ WRONG
class Widget {
public:
    std::shared_ptr<Widget> get_self() {
        return std::shared_ptr<Widget>(this);  // New control block!
    }
};

// ✅ CORRECT
class Widget : public std::enable_shared_from_this<Widget> {
public:
    std::shared_ptr<Widget> get_self() {
        return shared_from_this();  // Uses existing control block
    }
};
```

---

### Pitfall 4: Thread Safety Confusion

```cpp
std::shared_ptr<Widget> global;

// ✅ Thread-safe: Reference counting
void thread_a() {
    auto local = global;  // Atomic increment
}

// ❌ NOT thread-safe: Object access
void thread_b() {
    global->modify();  // Race condition!
}

// ✅ CORRECT: External synchronization
std::mutex mtx;
void thread_c() {
    std::lock_guard lock(mtx);
    global->modify();
}
```

---

## ⚡ Performance Comparison

### Memory Overhead

```cpp
Raw pointer:        8 bytes
unique_ptr:         8 bytes      (with default deleter)
shared_ptr:         16 bytes     (pointer + control block pointer)
                  + 24 bytes     (control block)
weak_ptr:           8 bytes

// Example
sizeof(int*):                    8 bytes
sizeof(unique_ptr<int>):         8 bytes  ✅ Zero overhead
sizeof(shared_ptr<int>):        16 bytes
```

---

### CPU Overhead

```cpp
// unique_ptr move: ~3 cycles
auto p2 = std::move(p1);

// shared_ptr copy: ~15-30 cycles
auto p2 = p1;  // Atomic increment + memory barriers

// shared_ptr destructor: ~15-50 cycles
// Atomic decrement + conditional delete
```

**Benchmark results (10M operations):**
```
unique_ptr move:     0.05s
shared_ptr copy:     0.15s  (3x slower)
shared_ptr move:     0.08s  (1.6x slower)
```

---

## 🎯 Decision Guide: Which Smart Pointer?

### Use unique_ptr When:

✅ **Default choice** - Start here  
✅ **Single owner** - Clear ownership  
✅ **Performance critical** - Zero overhead  
✅ **Factory functions** - Returning new objects  
✅ **Pimpl idiom** - Implementation hiding  

**Example:**
```cpp
std::unique_ptr<Resource> acquire_resource() {
    return std::make_unique<Resource>();
}
```

---

### Use shared_ptr When:

✅ **Multiple owners** - Ownership unclear  
✅ **Shared resources** - Caching, pooling  
✅ **Callbacks** - Async operations  
✅ **Graph structures** - With weak_ptr for back-references  

**Example:**
```cpp
class Cache {
    std::map<Key, std::shared_ptr<Value>> cache_;
    
    std::shared_ptr<Value> get(const Key& key) {
        return cache_[key];  // Share ownership
    }
};
```

---

### Use weak_ptr When:

✅ **Break cycles** - Prevent memory leaks  
✅ **Observer pattern** - Non-owning references  
✅ **Caching** - Can expire  
✅ **Optional references** - May become invalid  

**Example:**
```cpp
class Child {
    std::weak_ptr<Parent> parent_;  // Doesn't prevent Parent deletion
    
    void notify_parent() {
        if (auto p = parent_.lock()) {
            p->on_child_event();
        }
    }
};
```

---

## 🔗 Connection to Your Previous Work

### PooledPtr is unique_ptr!

```cpp
// Your PooledPtr
template<typename T, size_t PoolSize>
class PooledPtr {
    T* ptr_;
    MemoryPool<T, PoolSize>* pool_;
    
    ~PooledPtr() {
        pool_->deallocate(ptr_);  // Custom cleanup
    }
};

// It's unique_ptr with custom deleter
template<typename T, typename Deleter>
class unique_ptr {
    T* ptr_;
    Deleter deleter_;
    
    ~unique_ptr() {
        deleter_(ptr_);  // Custom cleanup
    }
};
```

**Same principles:**
- Exclusive ownership
- Move-only semantics
- RAII cleanup
- Custom deletion strategy

---

### Memory Pool + Smart Pointers

```cpp
// Combine your pool with standard smart pointers:

// Option 1: PooledPtr (what you built)
auto order = make_pooled<Order>(pool, args);

// Option 2: unique_ptr with pool deleter
auto order = std::unique_ptr<Order, PoolDeleter>(
    pool.allocate(args),
    PoolDeleter{&pool}
);

// Option 3: shared_ptr with pool deleter (multiple owners + reuse!)
auto order = std::shared_ptr<Order>(
    pool.allocate(args),
    PoolDeleter{&pool}
);

// All valid approaches!
```

---

## 📚 Key Takeaways

### 1. Smart Pointers Enforce Ownership

```
Raw pointer:   Widget* ptr;        // Who owns this? Unclear!
unique_ptr:    unique_ptr<Widget>  // I own this exclusively
shared_ptr:    shared_ptr<Widget>  // We share ownership
weak_ptr:      weak_ptr<Widget>    // I'm just watching
```

**Benefit:** Ownership is explicit and enforced by compiler.

---

### 2. RAII Prevents Leaks

```cpp
void risky_function() {
    auto resource = std::make_unique<Resource>();
    
    // Complex logic that might throw
    process(resource.get());
    
    // Even if exception thrown, resource is cleaned up!
}
```

**No manual cleanup needed, no leaks possible!**

---

### 3. Move Semantics Enable Efficiency

```cpp
// Move is cheap (pointer transfer)
std::unique_ptr<HugeObject> p1 = create_huge_object();
std::unique_ptr<HugeObject> p2 = std::move(p1);  // O(1)

// vs copying the HugeObject itself would be expensive
```

**Zero-cost ownership transfer.**

---

### 4. Reference Counting Manages Shared Lifetime

```cpp
auto shared = std::make_shared<Widget>();
store_in_cache(shared);      // ref_count: 2
register_callback(shared);   // ref_count: 3
shared.reset();              // ref_count: 2
// Widget still alive!
// Deleted when last reference gone
```

**Automatic lifetime management for shared resources.**

---

### 5. weak_ptr Breaks Cycles

```cpp
// Cycle causes leak
struct Node {
    shared_ptr<Node> next;
    shared_ptr<Node> prev;  // ❌ Cycle
};

// Fixed with weak_ptr
struct Node {
    shared_ptr<Node> next;   // Owns
    weak_ptr<Node> prev;     // Observes, breaks cycle ✓
};
```

**Essential for graph structures.**
