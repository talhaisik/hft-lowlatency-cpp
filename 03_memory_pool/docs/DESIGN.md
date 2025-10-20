# Memory Pool Allocator: Design & Implementation

**Module**: 03_memory_pool  
**Performance**: ~30x faster than new/delete

---

## 🎯 Overview

A high-performance fixed-size memory pool designed for HFT applications. Pre-allocates all memory at construction time, providing O(1) allocation and deallocation with zero fragmentation.

### Key Metrics

```
Pure Allocation Performance:
- new/delete:   22.8 Mops/s
- Memory Pool:  724 Mops/s
- Speedup:      31.8x FASTER

Realistic Batch Pattern:
- new/delete:   24.4 Mops/s  
- Memory Pool:  399 Mops/s
- Speedup:      16.3x FASTER

Latency Characteristics:
- Average: 1.63x faster
- Worst-case: 2.3x better (10.4μs vs 23.9μs)
- Consistent performance (low variance)
```

---

## 🏗️ Design Decisions

### 1. Fixed-Size Pool

**Decision:** Pre-allocate all memory at construction time.

**Rationale:**
- Predictable memory usage (critical for HFT)
- No runtime allocation (no OS syscalls in hot path)
- Zero fragmentation
- Deterministic performance

**Trade-off:**
- ✅ Maximum speed and predictability
- ❌ Must know maximum capacity upfront
- ❌ Memory allocated even if unused

**For HFT:** Acceptable trade-off. We can estimate max orders/quotes based on regulatory limits and historical data.

---

### 2. Intrusive Free List

**Decision:** Use union to store free list pointers inside object memory.

**Implementation:**
```cpp
union Slot {
    T object;      // When allocated: holds object
    Slot* next;    // When free: holds pointer to next free slot
};
```

**Rationale:**
- Zero memory overhead (no separate free list)
- Cache-friendly (free list data adjacent to objects)
- Simple pointer manipulation (O(1) operations)

**Trade-off:**
- ✅ Zero overhead
- ✅ Maximum speed
- ❌ Requires `sizeof(T) >= sizeof(void*)` (8 bytes on x64)

**Alternative Considered:** External free list (using `std::vector<size_t>`)
- Would work for any size type
- But: 25% memory overhead + ~10x slower
- **Rejected:** Overhead unacceptable for HFT

---

### 3. Manual Lifetime Management

**Decision:** Use placement new and explicit destructor calls.

**Why:**
```cpp
// We separate allocation from construction:
void* memory = ::operator new(...);    // Allocate ONCE
new (memory) T(args...);               // Construct MANY times
t->~T();                               // Destruct
// memory still exists, ready for reuse!
```

**Rationale:**
- Enables memory reuse
- Objects constructed/destructed in pre-allocated memory
- No heap search needed

**Safety:**
- User must call `deallocate()` (or use `PooledPtr`)
- Not exception-safe by default
- Intentional trade-off for maximum performance

---

### 4. RAII Wrapper (PooledPtr)

**Decision:** Provide optional RAII wrapper for users who want safety.

**Two usage patterns:**

```cpp
// Hot path: Manual (fastest)
Order* order = pool.allocate(1, 100.0);
process(order);
pool.deallocate(order);

// Cold path: RAII (safest)
auto order = make_pooled<Order>(pool, 1, 100.0);
// Automatic cleanup, exception-safe
```

**Rationale:**
- Users choose performance vs safety based on context
- Hot path gets maximum speed
- Business logic gets convenience

---

### 5. Single-Threaded Design

**Decision:** No built-in thread safety.

**Rationale:**
- Thread-local pools more efficient than synchronized pools
- User can add external synchronization if needed
- Avoids lock contention in common case

**Pattern for multi-threading:**
```cpp
// Option 1: Thread-local pools
thread_local MemoryPool<Order, 1000> pool;

// Option 2: External synchronization
std::mutex pool_mutex;
{
    std::lock_guard lock(pool_mutex);
    auto* order = pool.allocate(...);
}

// Option 3: Multiple pools (best)
MemoryPool<Order, 1000> pools[NUM_THREADS];
auto* order = pools[thread_id].allocate(...);
```

---

## 🔧 Implementation Details

### Memory Layout

```cpp
template<typename T, size_t PoolSize>
class MemoryPool {
private:
    std::byte* storage_;       // One contiguous block
    Slot* free_list_;          // Head of free list
    size_t available_count_;   // For monitoring
};
```

**Memory allocation (constructor):**
```cpp
storage_ = static_cast<std::byte*>(
    ::operator new(
        sizeof(Slot) * PoolSize,
        std::align_val_t{alignof(Slot)}
    )
);
```

**Why `::operator new` with alignment:**
- Allocates raw memory (no construction)
- Guarantees proper alignment for type T
- Single allocation for entire pool

---

### Free List Management

**Initial state (all free):**
```
Slot[0] → Slot[1] → Slot[2] → ... → Slot[N-1] → nullptr
  ↑
free_list_
```

**After allocating 2 objects:**
```
Slot[0]: Order object
Slot[1]: Order object  
Slot[2] → Slot[3] → ... → Slot[N-1] → nullptr
  ↑
free_list_
```

**After deallocating Slot[0]:**
```
Slot[0] → Slot[2] → Slot[3] → ... → Slot[N-1] → nullptr
  ↑         ↑
  |         formerly allocated
  |
free_list_
```

**Key insight:** Deallocated slots pushed to front of free list (LIFO).

**Performance characteristics:**
- Push (deallocate): O(1) - update 2 pointers
- Pop (allocate): O(1) - update 2 pointers
- No traversal needed!

---

### The Union Trick

**Memory reuse visualization for one slot:**

```
When FREE:
┌─────────────────┐
│ Bytes 0-7:      │
│ [Slot* pointer] │ ← Points to next free slot
│ Bytes 8-31:     │
│ [unused]        │
└─────────────────┘

When ALLOCATED:
┌─────────────────┐
│ Bytes 0-31:     │
│ [Order object]  │ ← Full object occupies space
│   id: 4 bytes   │
│   price: 8 bytes│
│   qty: 4 bytes  │
│   padding...    │
└─────────────────┘

SAME PHYSICAL MEMORY, different interpretation!
```

**Why this works:**
- When object exists, we don't need the pointer
- When slot is free, we don't have an object
- Union allows both interpretations of same bytes
- No wasted space!

---

## 📊 Performance Analysis

### Why Memory Pools Are Faster

**new/delete overhead:**
```
1. Search heap for suitable block      ~200 cycles
2. Lock heap data structures          ~50 cycles
3. Update heap metadata               ~50 cycles
4. Return memory                      ~50 cycles
───────────────────────────────────────────────
Total per operation:                  ~350 cycles
```

**Memory pool overhead:**
```
1. Load free_list_ pointer            ~3 cycles
2. Update free_list_ to next          ~2 cycles
3. Construct/destruct object          ~variable
───────────────────────────────────────────────
Total per operation:                  ~5-10 cycles
```

**Speedup: ~35-70x for allocation/deallocation alone!**

---

### Cache Performance

**Memory pool advantages:**
- Sequential memory layout (good spatial locality)
- Pre-allocated (memory already in cache)
- No heap traversal (no cache misses)
- Reuse = hot cache lines

**new/delete problems:**
- Random heap addresses (poor locality)
- Cold memory on allocation
- Heap metadata scattered
- Frequent cache misses

---

### Tail Latency

**Critical for HFT:** Worst-case performance matters!

```
                    Average    P99.9    Max
new/delete:         75.8 ns    200 ns   23,900 ns  ← Spike!
Memory Pool:        46.4 ns    100 ns   10,400 ns  ← Stable

Worst-case improvement: 2.3x better
```

**Why spikes in new/delete:**
- Heap fragmentation requires compaction
- OS page faults
- Allocator locks contention
- Background GC/compaction

**Why pool is stable:**
- No heap management
- No locks (single-threaded)
- No OS involvement
- Predictable O(1) operations

---

## 🎓 When to Use Memory Pools

### ✅ Use Memory Pools When:

1. **Fixed-size objects** - All objects same size
2. **Known maximum count** - Can estimate capacity
3. **High allocation frequency** - Hot path bottleneck
4. **Latency critical** - Predictable timing needed
5. **Controllable lifetime** - You manage object lifecycle

**Example use cases:**
- HFT order management
- Game entity systems
- Network packet buffers
- Event systems
- Request handlers

---

### ❌ Don't Use Memory Pools When:

1. **Variable sizes** - Objects have different sizes
   - **Alternative:** Slab allocator (multiple pools)

2. **Unknown capacity** - Can't estimate maximum
   - **Alternative:** Growing pool or general allocator

3. **Infrequent allocation** - Not a bottleneck
   - **Alternative:** std::allocator (simpler)

4. **Shared across threads** - Complex synchronization
   - **Alternative:** Thread-local pools

5. **Long-lived singletons** - Allocated once
   - **Alternative:** Just use new/delete

---

## 🔄 Alternative Designs Considered

### 1. External Free List

**Design:**
```cpp
std::vector<size_t> free_indices_;  // Separate free list
```

**Pros:**
- Works for any size type (even char)
- Easier to debug (free list separate)

**Cons:**
- 25% memory overhead (8 bytes per slot for index)
- ~10x slower (vector operations + indirection)

**Decision:** Rejected - overhead unacceptable for HFT

---

### 2. Growing Pool

**Design:**
```cpp
// Allocate more memory when exhausted
if (free_list_ == nullptr) {
    allocate_more_chunks();
}
```

**Pros:**
- Don't need to know exact capacity
- Flexible memory usage

**Cons:**
- Runtime allocation (defeats purpose!)
- Unpredictable timing
- Fragmentation possible
- Complex bookkeeping

**Decision:** Rejected - unpredictable latency unacceptable

---

### 3. std::variant for Type Safety

**Design:**
```cpp
std::variant<T, Slot*> slot;
```

**Pros:**
- Type-safe (tracks which type active)
- Exception-safe
- Easier to debug

**Cons:**
- 25% memory overhead (type index)
- ~10x slower (type checking)
- Unnecessary for internal implementation

**Decision:** Rejected - overhead not justified

---

### 4. Thread-Safe Pool

**Design:**
```cpp
std::atomic<Slot*> free_list_;  // Lock-free
// or
std::mutex mutex_;              // Mutex-protected
```

**Pros:**
- Shareable across threads
- Single pool instance

**Cons:**
- Lock-free: Complex, requires CAS, ABA problem
- Mutex: Contention kills performance
- Both: Slower than thread-local

**Decision:** Rejected - thread-local pools better for HFT

---

## 💡 Key Learnings

### 1. Separation of Concerns

**Memory lifetime ≠ Object lifetime:**
- Memory allocated once, exists for program lifetime
- Objects created/destroyed many times in that memory
- This separation enables reuse!

### 2. Zero-Cost Abstractions

**Union trick achieves:**
- Zero memory overhead (no separate free list)
- Zero runtime overhead (simple pointer manipulation)
- Maximum performance

**Trade-off:** Type must be large enough for pointer.

### 3. Manual Memory Management Can Be Safe

**With RAII wrapper:**
- Users who want safety: use `PooledPtr`
- Users who need speed: use manual management
- Best of both worlds!

### 4. Profile Before Optimizing

**Our benchmarking journey:**
1. First benchmark: pool seemed slow
2. Problem: measuring vector operations, not allocation!
3. Fixed benchmark: pool 31.8x faster
4. **Lesson:** Always verify you're measuring what you think!

### 5. Context Matters

**Memory pools perfect for:**
- HFT (our use case) ✓
- Game engines ✓
- Embedded systems ✓

**Memory pools wrong for:**
- General purpose code ✗
- Variable size objects ✗
- Unknown capacity ✗

---

## 🚀 Future Enhancements

### Potential Improvements

1. **Batch operations**
   ```cpp
   void allocate_batch(T** out, size_t count);
   void deallocate_batch(T** in, size_t count);
   ```
   - Amortize overhead
   - Better for bulk operations

2. **Statistics tracking**
   ```cpp
   size_t peak_usage() const;
   size_t total_allocations() const;
   double average_lifetime() const;
   ```
   - Monitor usage patterns
   - Capacity planning

3. **Debug mode**
   ```cpp
   #ifdef DEBUG_POOL
   // Track all allocations
   // Detect double-free
   // Detect use-after-free
   #endif
   ```
   - Safety during development
   - Zero overhead in release

4. **Growing variant** (optional)
   ```cpp
   template<typename T>
   class GrowingPool {
       std::vector<FixedPool<T>> chunks_;
   };
   ```
   - For unknown capacity
   - Keeps per-chunk performance

---

## 🎯 Summary

### What We Built

A production-quality fixed-size memory pool with:
- ✅ 31.8x speedup for pure allocation
- ✅ 16.3x speedup for realistic patterns
- ✅ 2.3x better tail latency
- ✅ Zero fragmentation
- ✅ Predictable O(1) performance
- ✅ Optional RAII wrapper
- ✅ Cache-friendly design

### Why It Matters

**In HFT:**
- Market ticks arrive every microsecond
- Every nanosecond of latency matters
- Tail latency can mean missed trades
- Memory pools eliminate allocation as bottleneck

**Our pool:**
- Allocation: ~5-10 cycles (~1.5-3ns)
- Predictable: Max 10.4μs vs 23.9μs for heap
- Enables processing ~30x more orders per second

### Key Insight

> "Don't use malloc in the hot path."  
> – Every HFT engineer ever

Memory pools are the solution.