# unique_ptr: Deep Dive

**Understanding Exclusive Ownership Smart Pointers**

---

## 🎯 What is unique_ptr?

`unique_ptr` is a smart pointer that **owns** an object exclusively. When the `unique_ptr` is destroyed, it automatically deletes the managed object.

### Key Characteristics:

1. **Exclusive ownership** - Only ONE unique_ptr can own an object at a time
2. **Move-only** - Cannot be copied, only moved
3. **Zero overhead** - Same size as raw pointer (with default deleter)
4. **RAII** - Automatic cleanup when scope ends

---

## 🧠 Mental Model

Think of `unique_ptr` like a **key to a house**:
- Only one person can hold the key (exclusive ownership)
- You can hand the key to someone else (move semantics)
- But you can't duplicate the key (no copying)
- When the last person holding the key leaves, the house is demolished (automatic cleanup)

---

## 💻 Basic Usage

### Creating unique_ptr

```cpp
// Method 1: make_unique (preferred - C++14)
auto widget = std::make_unique<Widget>(1, "Alpha");

// Method 2: Constructor with new
std::unique_ptr<Widget> widget(new Widget(1, "Alpha"));

// Method 3: Default construction (empty)
std::unique_ptr<Widget> widget;  // nullptr
```

### Using unique_ptr

```cpp
auto widget = std::make_unique<Widget>(1, "Alpha");

// Dereference
widget->use();      // Call member function
(*widget).use();    // Also works

// Get raw pointer (doesn't release ownership)
Widget* raw = widget.get();

// Check if not null
if (widget) {
    widget->use();
}
```

---

## 🔄 Move Semantics

### unique_ptr is Move-Only

```cpp
auto widget1 = std::make_unique<Widget>(1, "Alpha");

// ❌ COPY: Not allowed!
auto widget2 = widget1;  // ERROR: Copy constructor deleted

// ✅ MOVE: Transfer ownership
auto widget2 = std::move(widget1);
//             └─ Explicitly move

// After move:
// widget1.get() == nullptr  ← Empty
// widget2.get() == pointer  ← Owns the Widget
```

### Why Move-Only?

**Exclusive ownership means only ONE owner:**

```cpp
// If copying were allowed:
auto widget1 = std::make_unique<Widget>(...);
auto widget2 = widget1;  // Suppose this worked

// Now what? Two unique_ptrs own the same Widget!
// When widget1 is destroyed: delete the Widget
// When widget2 is destroyed: delete the Widget AGAIN!
// DOUBLE DELETE = CRASH! 💥
```

**Solution:** Prevent copying, allow moving (transferring ownership).

---

## 🎓 Understanding Our Implementation

### The Core Structure

```cpp
template<typename T, typename Deleter = DefaultDeleter<T>>
class UniquePtr {
private:
    T* ptr_;           // The managed pointer
    Deleter deleter_;  // How to clean up
    
public:
    ~UniquePtr() {
        if (ptr_) {
            deleter_(ptr_);  // Cleanup!
        }
    }
};
```

### Key Methods Explained

#### 1. Constructor

```cpp
explicit UniquePtr(T* ptr) noexcept : ptr_(ptr), deleter_() {}
```

**Why `explicit`?**
```cpp
// Without explicit:
UniquePtr<int> p = new int(42);  // Implicit conversion - dangerous!

// With explicit:
UniquePtr<int> p(new int(42));   // Must be explicit - clear intent!
```

#### 2. Move Constructor

```cpp
UniquePtr(UniquePtr&& other) noexcept
    : ptr_(other.ptr_), deleter_(std::move(other.deleter_)) {
    other.ptr_ = nullptr;  // Leave other empty!
}
```

**Step by step:**
1. Take other's pointer
2. Take other's deleter
3. Set other's pointer to nullptr (important!)

**Why set to nullptr?**
```cpp
// If we didn't:
{
    UniquePtr<Widget> p1(new Widget());
    UniquePtr<Widget> p2(std::move(p1));
    
    // Both p1 and p2 would have the same pointer!
    // When p2 destructs: delete ptr
    // When p1 destructs: delete ptr AGAIN! 💥
}
```

#### 3. release()

```cpp
T* release() noexcept {
    T* old_ptr = ptr_;
    ptr_ = nullptr;     // Give up ownership
    return old_ptr;     // Caller now responsible
}
```

**Use case:**
```cpp
auto widget = std::make_unique<Widget>();

// Transfer to legacy API that expects raw pointer
Widget* raw = widget.release();
legacy_function_that_takes_ownership(raw);
// We no longer own it, no automatic cleanup
```

#### 4. reset()

```cpp
void reset(T* ptr = nullptr) noexcept {
    T* old_ptr = ptr_;
    ptr_ = ptr;
    if (old_ptr) {
        deleter_(old_ptr);  // Delete old object
    }
}
```

**Use case:**
```cpp
auto widget = std::make_unique<Widget>(1, "Old");

// Replace with new object
widget.reset(new Widget(2, "New"));
// Widget 1 deleted, Widget 2 now managed

// Or just delete
widget.reset();  // Deletes Widget 2, widget is now empty
```

---

## 🎨 Custom Deleters

### Why Custom Deleters?

Sometimes `delete` isn't the right cleanup:

```cpp
// File handles
FILE* file = fopen("data.txt", "r");
// Need fclose(), not delete!

// Memory pools
Order* order = pool.allocate(...);
// Need pool.deallocate(), not delete!

// Arrays
int* arr = new int[100];
// Need delete[], not delete!
```

### Example: FILE Handle

```cpp
// Custom deleter for FILE*
struct FileCloser {
    void operator()(FILE* file) const {
        if (file) {
            fclose(file);
            std::cout << "File closed\n";
        }
    }
};

// Usage
{
    std::unique_ptr<FILE, FileCloser> file(
        fopen("data.txt", "r"),
        FileCloser{}
    );
    
    if (file) {
        // Use file...
    }
    
    // Automatically closed when scope ends
}
```

### Example: Memory Pool Deleter

```cpp
// Custom deleter for our memory pool
template<typename T, size_t PoolSize>
struct PoolDeleter {
    MemoryPool<T, PoolSize>* pool;
    
    void operator()(T* ptr) const {
        pool->deallocate(ptr);
    }
};

// Usage
MemoryPool<Order, 1000> order_pool;

{
    std::unique_ptr<Order, PoolDeleter<Order, 1000>> order(
        order_pool.allocate(1, 100.0),
        PoolDeleter<Order, 1000>{&order_pool}
    );
    
    order->process();
    
    // Automatically returned to pool when scope ends
}
```

**This is essentially what PooledPtr does!**

---

## ⚡ Performance: Zero Overhead

### Size Comparison

```cpp
sizeof(int*)                                    // 8 bytes (on x64)
sizeof(std::unique_ptr<int>)                    // 8 bytes ✓
sizeof(std::unique_ptr<int, CustomDeleter>)     // 8-16 bytes (depends on deleter)
```

**With default deleter:** Same size as raw pointer!

### How? Empty Base Optimization

```cpp
// DefaultDeleter is empty (no member data)
struct DefaultDeleter<T> {
    void operator()(T* ptr) const { delete ptr; }
    // No data members!
};

// Compiler optimizes away empty base classes
// So deleter_ takes 0 bytes!
template<typename T, typename Deleter>
class UniquePtr {
    T* ptr_;           // 8 bytes
    Deleter deleter_;  // 0 bytes if empty! (EBO)
};

sizeof(UniquePtr<int>) == 8  // Same as raw pointer!
```

---

## 🎯 unique_ptr vs Raw Pointers vs PooledPtr

### Comparison Table

|         Feature       |  Raw Pointer | unique_ptr  |   PooledPtr   |
|-----------------------|--------------|-------------|---------------|
| **Automatic cleanup** | No           | Yes         | Yes           |
| **Exception safe**    | No           | Yes         | Yes           |
| **Overhead**          | 0 bytes      | 0 bytes*    | 8-16 bytes    |
| **Copy**              | Yes          | No          | No            |
| **Move**              | Yes          | Yes         | Yes           |
| **Custom cleanup**    | Manual       | Via deleter | Pool-specific |
| **Clear ownership**   | Ambiguous    | Clear       | Clear         |

*With default deleter

---

## 🚨 Common Pitfalls

### 1. Double Delete

```cpp
Widget* raw = new Widget();

std::unique_ptr<Widget> p1(raw);
std::unique_ptr<Widget> p2(raw);  // ❌ BAD! Both own same pointer

// When p1 destructs: delete raw
// When p2 destructs: delete raw AGAIN! 💥
```

**Solution:** Only create one unique_ptr per object.

---

### 2. Releasing Then Forgetting

```cpp
auto widget = std::make_unique<Widget>();
Widget* raw = widget.release();

// ... forgot to delete raw ...

// MEMORY LEAK!
```

**Solution:** Use `release()` only when transferring ownership, not for temporary access. Use `get()` instead.

---

### 3. Mixing with Raw Pointers

```cpp
Widget* raw = new Widget();
process(raw);  // Does this take ownership? Who knows!

// vs

auto widget = std::make_unique<Widget>();
process(widget.get());  // Clear: we still own it
```

**Solution:** Use smart pointers consistently, pass raw pointers only when ownership is clear.

---

## 💡 When to Use unique_ptr

### ✅ Use unique_ptr When:

1. **Single clear owner** - One object owns another
2. **Factory functions** - Returning newly created objects
3. **Resource management** - Files, sockets, etc.
4. **Exception safety** - Ensure cleanup even on exceptions
5. **Pimpl idiom** - Implementation hiding

**Example: Factory**
```cpp
std::unique_ptr<Shape> create_shape(ShapeType type) {
    switch (type) {
        case Circle:    return std::make_unique<Circle>();
        case Square:    return std::make_unique<Square>();
        case Triangle:  return std::make_unique<Triangle>();
    }
}

// Caller gets ownership
auto shape = create_shape(Circle);
```

---

### ❌ Don't Use unique_ptr When:

1. **Shared ownership needed** - Use `shared_ptr`
2. **Performance critical + proven bottleneck** - Profile first!
3. **Simple local variables** - Stack allocation is fine
4. **Already have better abstraction** - e.g., PooledPtr for pools

---

## 🎓 Key Takeaways

1. **unique_ptr = exclusive ownership** - Only one owner at a time
2. **Move-only semantics** - Transfer ownership, don't copy
3. **Zero overhead** - Same size as raw pointer (with default deleter)
4. **RAII** - Automatic cleanup, exception-safe
5. **Custom deleters** - Flexible cleanup strategies
6. **PooledPtr is unique_ptr for pools** - Same concept, different cleanup

---

## 🔗 Connection to Your Code

### PooledPtr is unique_ptr!

```cpp
// Your PooledPtr:
template<typename T, size_t PoolSize>
class PooledPtr {
    T* ptr_;
    MemoryPool<T, PoolSize>* pool_;
    
    ~PooledPtr() {
        if (ptr_) pool_->deallocate(ptr_);
    }
};

// It's unique_ptr with custom deleter:
template<typename T>
class unique_ptr {
    T* ptr_;
    Deleter deleter_;
    
    ~unique_ptr() {
        if (ptr_) deleter_(ptr_);
    }
};
```

**Same pattern:**
- Exclusive ownership ✓
- Move-only ✓
- RAII cleanup ✓
- Custom deletion strategy ✓

**You've already mastered the concept!** 🎉

---

## 📚 Next: shared_ptr

Now that you understand unique_ptr (exclusive ownership), we'll explore shared_ptr (shared ownership with reference counting).

The key difference:
- **unique_ptr:** "I own this, nobody else"
- **shared_ptr:** "We own this together, last one out cleans up"