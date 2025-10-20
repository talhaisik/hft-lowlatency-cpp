# shared_ptr: Deep Dive

**Understanding Shared Ownership and Reference Counting**

---

## 🎯 What is shared_ptr?

`shared_ptr` is a smart pointer that allows **multiple owners** of the same object. The object is automatically deleted when the **last** `shared_ptr` owning it is destroyed.

### Key Characteristics:

1. **Shared ownership** - Multiple shared_ptrs can own the same object
2. **Reference counting** - Tracks how many owners exist
3. **Automatic cleanup** - Object deleted when count reaches 0
4. **Thread-safe counting** - Uses atomic operations
5. **Copyable** - Unlike unique_ptr

---

## 🧠 Mental Model

Think of `shared_ptr` like a **shared document**:
- Multiple people can have a copy of the document (shared ownership)
- Everyone has a counter showing how many copies exist (reference count)
- When the last person deletes their copy, the original is shredded (automatic cleanup)

---

## 📊 The Control Block

The magic of `shared_ptr` lies in the **control block**:

```
Memory Layout:
┌──────────────────────┐
│   Control Block      │
│  ┌────────────────┐  │
│  │ ptr (object*)  │──┼──→ [Actual Object]
│  │ ref_count: 3   │  │
│  │ weak_count: 0  │  │
│  └────────────────┘  │
└──────────────────────┘
         ↑
         │
    ┌────┴────┬────────┬────────┐
    │         │        │        │
  ptr1      ptr2     ptr3     (3 shared_ptrs point to same control block)
```

**Control block contains:**
- Pointer to the actual object
- Reference count (how many shared_ptrs)
- Weak count (how many weak_ptrs)
- Deleter (optional)
- Allocator (optional)

---

## 💻 Basic Usage

### Creating shared_ptr

```cpp
// Method 1: make_shared (preferred!)
auto resource = hft::smart::make_shared<Resource>(1, "Alpha");

// Method 2: Constructor with new
hft::smart::SharedPtr<Resource> resource(new Resource(1, "Alpha"));

// Method 3: Copy from existing
auto resource2 = resource;  // Shares ownership!
```

### Using shared_ptr

```cpp
auto resource = hft::smart::make_shared<Resource>(1);

// Use like a pointer
resource->use();
(*resource).use();

// Get raw pointer
Resource* raw = resource.get();

// Check reference count
std::cout << "Owners: " << resource.use_count() << "\n";

// Check if only owner
if (resource.unique()) {
    std::cout << "I'm the only owner\n";
}
```

---

## 🔄 Reference Counting in Action

### Visual Walkthrough

```cpp
// Step 1: Create first shared_ptr
auto ptr1 = hft::smart::make_shared<Widget>(1);
```

```
Control Block:
┌──────────────┐
│ ptr → Widget │
│ ref_count: 1 │ ← One owner
└──────────────┘
       ↑
       │
      ptr1
```

---

```cpp
// Step 2: Copy to create second shared_ptr
auto ptr2 = ptr1;  // Copy!
```

```
Control Block:
┌──────────────┐
│ ptr → Widget │
│ ref_count: 2 │ ← Two owners
└──────────────┘
       ↑
       │
   ┌───┴───┐
  ptr1   ptr2
```

---

```cpp
// Step 3: Create third shared_ptr
auto ptr3 = ptr1;
```

```
Control Block:
┌──────────────┐
│ ptr → Widget │
│ ref_count: 3 │ ← Three owners
└──────────────┘
       ↑
       │
   ┌───┼───┐
  ptr1 ptr2 ptr3
```

---

```cpp
// Step 4: ptr2 goes out of scope or reset
ptr2.reset();
```

```
Control Block:
┌──────────────┐
│ ptr → Widget │
│ ref_count: 2 │ ← Back to two
└──────────────┘
       ↑
       │
   ┌───┴───┐
  ptr1   ptr3
```

---

```cpp
// Step 5: Last owner destroyed
// ptr1 and ptr3 go out of scope
```

```
Control Block:
┌──────────────┐
│ ptr → Widget │
│ ref_count: 0 │ ← Last owner gone!
└──────────────┘
       ↓
    Widget deleted!
    Control block deleted!
```

---

## 🎓 Understanding the Implementation

### Reference Count Operations

#### Add Reference (Copy)

```cpp
SharedPtr(const SharedPtr& other) : control_(other.control_) {
    if (control_) {
        control_->add_ref();  // Atomic increment
    }
}

void add_ref() noexcept {
    ref_count.fetch_add(1, std::memory_order_relaxed);
    //                      └─ Thread-safe increment!
}
```

**What happens:**
1. Copy pointer to control block
2. Atomically increment reference count
3. Both shared_ptrs now own the object

---

#### Release Reference (Destructor)

```cpp
~SharedPtr() {
    if (control_) {
        if (control_->release()) {  // Decrement and check
            delete control_;         // Last owner cleans up
        }
    }
}

bool release() noexcept {
    if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // We were the last owner!
        delete ptr;          // Delete the object
        ptr = nullptr;
        
        if (weak_count == 0) {
            return true;     // Delete control block too
        }
    }
    return false;
}
```

**What happens:**
1. Atomically decrement reference count
2. If count was 1 (we were last): delete object
3. If no weak_ptrs exist: delete control block
4. Otherwise: control block stays (for weak_ptrs)

---

### Why Atomic Operations?

```cpp
// Without atomics (BROKEN):
void add_ref() {
    ++ref_count;  // NOT ATOMIC!
}

// Thread 1:                 Thread 2:
load ref_count (5)          load ref_count (5)
add 1 (6)                   add 1 (6)
store ref_count (6)         store ref_count (6)
                            →
                            RACE! Should be 7, but is 6!
```

```cpp
// With atomics (CORRECT):
void add_ref() {
    ref_count.fetch_add(1, std::memory_order_relaxed);
    // Atomic operation - thread-safe!
}

// Thread 1:                 Thread 2:
atomic increment (5→6)      atomic increment (6→7)
                            →
                            CORRECT! Count is 7
```

---

## ⚡ Performance Implications

### Memory Overhead

```cpp
sizeof(Widget*)                         // 8 bytes
sizeof(hft::smart::UniquePtr<Widget>)   // 16 bytes (with deleter)
sizeof(hft::smart::SharedPtr<Widget>)   // 8 bytes (just control block pointer)
```

**Plus control block:**
- Control block: ~24 bytes (ptr + 2 counters)
- Total overhead: ~32 bytes per object

---

### CPU Overhead

```cpp
// unique_ptr copy: ERROR (deleted)
// unique_ptr move: ~3 instructions

// shared_ptr copy: ~10-20 instructions
//   - Atomic increment
//   - Memory barriers
//   - Cache coherency traffic

// shared_ptr destructor: ~10-30 instructions
//   - Atomic decrement
//   - Conditional delete
//   - Potential control block cleanup
```

**Atomic operations are NOT free!**

---

### make_shared Optimization

```cpp
// Method 1: Constructor (2 allocations)
hft::smart::SharedPtr<Widget> p1(new Widget());

// Memory layout:
// [Widget object]    (allocation 1)
// [Control block]    (allocation 2)
// Not cache-friendly!

// Method 2: make_shared (1 allocation)
auto p2 = hft::smart::make_shared<Widget>();

// Memory layout:
// [Control block | Widget object]  (single allocation!)
// Better cache locality!
```

**Always prefer `make_shared` when possible!**

---

## 🚨 Common Pitfalls

### 1. Double Control Block

```cpp
Widget* raw = new Widget();

hft::smart::SharedPtr<Widget> p1(raw);  // Control block 1
hft::smart::SharedPtr<Widget> p2(raw);  // Control block 2 ❌

// Problem: Two control blocks, both think they own the widget!
// When p1 destructs: delete widget
// When p2 destructs: delete widget AGAIN! 💥
```

**Solution:** Create shared_ptr only once, then copy it:

```cpp
auto p1 = hft::smart::make_shared<Widget>();
auto p2 = p1;  // Share same control block ✓
```

---

### 2. Circular References (Memory Leak!)

```cpp
struct Node {
    hft::smart::SharedPtr<Node> next;
};

auto node1 = hft::smart::make_shared<Node>();
auto node2 = hft::smart::make_shared<Node>();

node1->next = node2;  // node2 ref_count: 2
node2->next = node1;  // node1 ref_count: 2

// Both go out of scope:
// node1: ref_count 2 → 1 (still owned by node2->next)
// node2: ref_count 2 → 1 (still owned by node1->next)
// MEMORY LEAK! Neither can reach 0!
```

**Solution:** Use `weak_ptr` for back-references:

```cpp
struct Node {
    hft::smart::SharedPtr<Node> next;  // Strong: parent owns child
    hft::smart::WeakPtr<Node> prev;    // Weak: child observes parent
};
```

---

### 3. this Pointer Confusion

```cpp
class Widget {
public:
    hft::smart::SharedPtr<Widget> get_self() {
        // ❌ WRONG:
        return hft::smart::SharedPtr<Widget>(this);
        // Creates new control block! Double-delete!
    }
};
```

**Solution:** Store a WeakPtr to yourself:

```cpp
class Widget {
    hft::smart::WeakPtr<Widget> self_;
    
public:
    void set_self(hft::smart::SharedPtr<Widget> self) {
        self_ = self;
    }
    
    hft::smart::SharedPtr<Widget> get_self() {
        return self_.lock();  // Uses existing control block
    }
};
```

---

### 4. Mixing with unique_ptr

```cpp
// You can convert unique_ptr → shared_ptr
hft::smart::UniquePtr<Widget> unique = hft::smart::make_unique<Widget>();
hft::smart::SharedPtr<Widget> shared = std::move(unique);  // OK ✓

// But NOT shared_ptr → unique_ptr
hft::smart::SharedPtr<Widget> shared = hft::smart::make_shared<Widget>();
hft::smart::UniquePtr<Widget> unique = ???  // No good way! ❌
```

---

## 💡 When to Use shared_ptr

### ✅ Use shared_ptr When:

1. **Multiple owners needed** - Unclear who should delete
2. **Object outlives creator** - Lifetime longer than function scope
3. **Caching/pooling** - Shared resources
4. **Callbacks** - Async operations that may outlive caller
5. **Graph structures** - (with weak_ptr for back-references)

**Example: Shared Resource Pool**

```cpp
class ResourcePool {
    std::vector<hft::smart::SharedPtr<Resource>> resources_;
    
public:
    hft::smart::SharedPtr<Resource> get_resource() {
        // Multiple clients can hold same resource
        return resources_[find_available()];
    }
};

// Client code:
auto resource = pool.get_resource();
// Even if pool is destroyed, resource stays alive
// until we're done with it!
```

---

### ❌ Don't Use shared_ptr When:

1. **Single clear owner** - Use `unique_ptr`
2. **Performance critical** - Atomic ops have overhead
3. **Tight loops** - Copy overhead matters
4. **Embedded systems** - Memory overhead significant
5. **Just want RAII** - `unique_ptr` is enough

---

## 📊 Comparison: unique_ptr vs shared_ptr

| Feature           | unique_ptr     | shared_ptr                |
|-------------------|----------------|---------------------------|
| **Ownership**     | Exclusive      | Shared                    |
| **Copy**          | No             | Yes                       |
| **Move**          | Yes            | Yes                       |
| **Size**          | 16 bytes       | 8 bytes                   |
| **Control block** | No             | Yes (~24 bytes)           |
| **Ref counting**  | No             | Yes (atomic)              |
| **Thread-safe**   | Count: N/A     | Count: Yes     Object: No |
| **Performance**   | Fastest        | Slower (atomics)          |
| **Use case**      | Default choice | Multiple owners           |

---

## 🎯 Real-World Example: Event System

```cpp
class EventListener {
public:
    virtual ~EventListener() = default;
    virtual void onEvent(const Event& e) = 0;
};

class EventDispatcher {
    // Store weak references to listeners
    std::vector<hft::smart::WeakPtr<EventListener>> listeners_;
    
public:
    void subscribe(hft::smart::SharedPtr<EventListener> listener) {
        listeners_.push_back(listener);  // Store weak reference
    }
    
    void dispatch(const Event& e) {
        // Notify all alive listeners
        auto it = listeners_.begin();
        while (it != listeners_.end()) {
            if (auto listener = it->lock()) {
                listener->onEvent(e);
                ++it;
            } else {
                // Listener died, remove it
                it = listeners_.erase(it);
            }
        }
    }
};

// Usage:
EventDispatcher dispatcher;

{
    auto listener = hft::smart::make_shared<MyListener>();
    dispatcher.subscribe(listener);
    dispatcher.dispatch(Event{});  // listener notified
}
// listener destroyed here

dispatcher.dispatch(Event{});  // Automatically skips dead listener
```

---

## 🔬 Advanced Topics

### Custom Deleters

```cpp
// Custom deleter for FILE*
auto file_deleter = [](FILE* f) {
    if (f) {
        std::cout << "Closing file\n";
        fclose(f);
    }
};

hft::smart::SharedPtr<FILE> file(
    fopen("data.txt", "r"),
    file_deleter
);
```

### Array Support

```cpp
// For arrays, use unique_ptr or std::vector instead
// shared_ptr of arrays is rarely needed

// If you must:
hft::smart::SharedPtr<int[]> arr(new int[100]);
```

### Thread Safety Details

```cpp
// Thread-safe (reference counting):
hft::smart::SharedPtr<Widget> ptr = make_shared<Widget>();

std::thread t1([ptr]() { auto copy = ptr; });  // Safe
std::thread t2([ptr]() { auto copy = ptr; });  // Safe

// NOT thread-safe (object access):
std::thread t1([ptr]() { ptr->modify(); });  // Race condition!
std::thread t2([ptr]() { ptr->modify(); });  // Race condition!

// Need mutex for object access:
std::mutex mtx;
std::thread t1([ptr, &mtx]() { 
    std::lock_guard<std::mutex> lock(mtx);
    ptr->modify();  // Safe
});
```

---

## 📚 Summary

**Key Takeaways:**

1. **SharedPtr** enables shared ownership through reference counting
2. **Control block** manages lifetime with atomic counters
3. **Use `make_shared`** for better performance (single allocation)
4. **Avoid circular references** - use WeakPtr for back-pointers
5. **Thread-safe counting** - but object access needs synchronization
6. **Default to unique_ptr** - use shared_ptr only when needed
7. **Never create multiple SharedPtrs** from the same raw pointer

**The Golden Rules:**

- ✅ Create once with `make_shared`, copy freely
- ✅ Use `weak_ptr` to break cycles
- ✅ Prefer `unique_ptr` when ownership is clear
- ❌ Don't create from raw pointer multiple times
- ❌ Don't use for performance-critical hot paths
- ❌ Don't assume object access is thread-safe

---

**Next:** Learn about [WeakPtr](WEAK_PTR.md) and how to break circular references!