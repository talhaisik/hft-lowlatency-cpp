# Atomic Operations Deep Dive

## 🎯 Learning Objectives

After completing this module, you will understand:
- What atomics are and why they're needed
- How CPUs execute concurrent operations
- The different memory ordering guarantees
- When to use each memory ordering
- Common atomic patterns in lock-free programming
- Performance implications of different atomic operations

---

## 📚 What Are Atomic Operations?

### The Problem: Non-Atomic Operations

```cpp
// Two threads executing this simultaneously
int counter = 0;

// Thread 1 & Thread 2 both do:
counter++;  // NOT ATOMIC!
```

**What actually happens at assembly level:**
```asm
; counter++ compiles to THREE instructions:
MOV  eax, [counter]    ; 1. Read current value
INC  eax               ; 2. Increment in register
MOV  [counter], eax    ; 3. Write back to memory
```

**Race Condition Example:**
```
Initial: counter = 0

Thread 1: MOV eax, [counter]  → eax = 0
Thread 2: MOV eax, [counter]  → eax = 0  (reads same value!)
Thread 1: INC eax             → eax = 1
Thread 2: INC eax             → eax = 1
Thread 1: MOV [counter], eax  → counter = 1
Thread 2: MOV [counter], eax  → counter = 1  (overwrites!)

Expected: counter = 2
Actual:   counter = 1  ❌ BUG!
```

### The Solution: Atomic Operations

```cpp
std::atomic<int> counter{0};

// Thread 1 & Thread 2 both do:
counter.fetch_add(1, std::memory_order_seq_cst);
```

**What happens:**
```asm
; Compiles to SINGLE instruction with LOCK prefix:
LOCK INC DWORD PTR [counter]  ; Atomic read-modify-write
```

The `LOCK` prefix ensures:
- ✅ No other CPU can access the cache line
- ✅ Operation appears instantaneous to all threads
- ✅ No race conditions possible

---

## 🧠 CPU Architecture & Memory Visibility

### The Memory Hierarchy

```
CPU Core 0              CPU Core 1
┌─────────────┐        ┌─────────────┐
│ Registers   │        │ Registers   │
├─────────────┤        ├─────────────┤
│ L1 Cache    │        │ L1 Cache    │
│ (32-64 KB)  │        │ (32-64 KB)  │
│ ~4 cycles   │        │ ~4 cycles   │
├─────────────┤        ├─────────────┤
│ L2 Cache    │        │ L2 Cache    │
│ (256-512KB) │        │ (256-512KB) │
│ ~12 cycles  │        │ ~12 cycles  │
└──────┬──────┘        └──────┬──────┘
       │                      │
       └──────────┬───────────┘
                  │
          ┌───────▼────────┐
          │   L3 Cache     │
          │   (8-32 MB)    │
          │   ~40 cycles   │
          └───────┬────────┘
                  │
          ┌───────▼────────┐
          │   Main RAM     │
          │   (8-64 GB)    │
          │   ~100 cycles  │
          └────────────────┘
```

### The Cache Coherency Problem

```cpp
// Initially: x = 0 (in main memory)
std::atomic<int> x{0};
std::atomic<int> y{0};

// Thread 1 (CPU Core 0)        // Thread 2 (CPU Core 1)
x.store(1);                      while (x.load() == 0) {}
y.store(1);                      assert(y.load() == 1);  // Can fail!
```

**Why can the assert fail?**

1. CPU cores have **private L1/L2 caches**
2. Writes may not be immediately visible to other cores
3. CPUs can **reorder instructions** for performance
4. Compiler can also reorder instructions

**Memory Ordering** solves this by providing synchronization guarantees.

---

## 🔢 Memory Ordering: The Six Flavors

C++ provides 6 memory orderings (from weakest to strongest):

### 1. `memory_order_relaxed` - No Synchronization

**Guarantees:**
- ✅ Operation is atomic (no torn reads/writes)
- ❌ NO ordering guarantees relative to other operations
- ❌ NO visibility guarantees across threads

**When to use:**
- Counters where order doesn't matter
- Statistics gathering
- Incrementing request IDs

**Example: Simple Counter**
```cpp
std::atomic<uint64_t> request_count{0};

// Thread 1, 2, 3... all do:
request_count.fetch_add(1, std::memory_order_relaxed);
// We only care about the final count, not the order
```

**Performance:** Fastest (~1-2 cycles on x86)

---

### 2. `memory_order_acquire` - Synchronizes Reads

**Guarantees:**
- ✅ All reads/writes AFTER this acquire cannot be reordered BEFORE it
- ✅ Pairs with a `release` to establish "happens-before" relationship

**Mental Model:** 
"Acquire a lock - nothing can move UP above this line"

```cpp
// Acquire fence
─────────────────  ← Nothing below can move up
| Reads & Writes |
| can move down  |
```

**Example: Flag-Protected Data**
```cpp
int data = 0;
std::atomic<bool> ready{false};

// Writer Thread
data = 42;                                    // 1
ready.store(true, std::memory_order_release); // 2

// Reader Thread
if (ready.load(std::memory_order_acquire)) {  // 3
    assert(data == 42);  // ✅ Always passes!    4
}
```

**Why it works:**
- Store(release) at [2] prevents [1] from moving after it
- Load(acquire) at [3] prevents [4] from moving before it
- Together: [1] happens-before [4]

---

### 3. `memory_order_release` - Synchronizes Writes

**Guarantees:**
- ✅ All reads/writes BEFORE this release cannot be reordered AFTER it
- ✅ Pairs with an `acquire` to establish synchronization

**Mental Model:**
"Release a lock - nothing can move DOWN below this line"

```cpp
| Reads & Writes |
| can move up    |
─────────────────  ← Nothing above can move down
// Release fence
```

**Performance:** Cheap on x86 (~1-2 cycles), more expensive on ARM

---

### 4. `memory_order_acq_rel` - Both Acquire & Release

**Guarantees:**
- ✅ Combines acquire + release semantics
- ✅ Nothing moves up (acquire) or down (release)

**When to use:**
- Read-modify-write operations (fetch_add, compare_exchange)
- Protecting both reads and writes

```cpp
─────────────────  ← Nothing below can move up
| This operation |
─────────────────  ← Nothing above can move down
```

**Example: Work Queue**
```cpp
std::atomic<int> tail{0};
std::array<Task, 1024> queue;

// Producer
queue[tail.load(memory_order_relaxed)] = task;
tail.fetch_add(1, memory_order_acq_rel);  // Publish task

// Consumer
int idx = tail.load(memory_order_acquire);
Task t = queue[idx];  // Safe - happens-after producer
```

---

### 5. `memory_order_seq_cst` - Sequential Consistency (Default)

**Guarantees:**
- ✅ All previous operations complete before this
- ✅ All subsequent operations start after this
- ✅ **Global total order** across all threads
- ✅ Most intuitive but slowest

**Mental Model:**
"Everything is ordered as if executed on a single CPU"

```cpp
// Thread 1               // Thread 2
x.store(1, seq_cst);      y.store(1, seq_cst);
int r1 = y.load(seq_cst); int r2 = x.load(seq_cst);

// Impossible outcome: r1 == 0 && r2 == 0
// One thread MUST see the other's write
```

**Performance:** Slowest on x86 (~5-20 cycles due to MFENCE)

**When to use:**
- Default when unsure
- Complex synchronization patterns
- When correctness matters more than performance

---

### 6. `memory_order_consume` - Data-Dependency Ordering

**Status:** ⚠️ **Deprecated in C++17** - use `acquire` instead

**Why deprecated:** Too subtle, compilers couldn't optimize well

---

## 🎯 Choosing the Right Memory Ordering

### Decision Tree

```
┌─────────────────────────────────────────┐
│ Do you need synchronization with other  │
│ atomic operations?                      │
└───────┬─────────────────────┬───────────┘
        │ NO                  │ YES
        ▼                     ▼
  ┌──────────┐        ┌──────────────────┐
  │ relaxed  │        │ Need to publish  │
  └──────────┘        │ non-atomic data? │
                      └────┬────────┬────┘
                      YES  │        │ NO
                           ▼        ▼
                    ┌───────────┐ ┌─────────────┐
                    │ acquire/  │ │ Read-modify │
                    │ release   │ │ -write?     │
                    └───────────┘ └──────┬──────┘
                                          ▼
                                   ┌────────────┐
                                   │  acq_rel   │
                                   └────────────┘

                    ┌──────────────────────────┐
                    │ Unsure / Complex?        │
                    │ → seq_cst (safest)       │
                    └──────────────────────────┘
```

### Common Patterns

| Use Case            | Memory Ordering                              | Reasoning                   |
|---------------------|----------------------------------------------|-----------------------------|
| Simple counter      | `relaxed`                                    | Order doesn't matter        |
| Flag + data publish | `release` (store) + `acquire` (load)         | Synchronize data            |
| Spinlock            | `acquire` (lock) + `release` (unlock)        | Critical section boundaries |
| fetch_add in queue  | `acq_rel`                                    | Both read and modify        |
| Reference counting  | `relaxed` (increment), `acq_rel` (decrement) | Specific patterns           |
| When unsure         | `seq_cst`                                    | Safest default              |

---

## ⚡ Performance Comparison (x86-64)

| Operation                 | Memory Order | Cycles | Notes                   |
|---------------------------|--------------|--------|-------------------------|
| `load()`                  | `relaxed`    | 1      | Just a MOV              |
| `load()`                  | `acquire`    | 1      | Free on x86!            |
| `load()`                  | `seq_cst`    | 1      | Also free               |
| `store()`                 | `relaxed`    | 1      | Just a MOV              |
| `store()`                 | `release`    | 1      | Free on x86!            |
| `store()`                 | `seq_cst`    | 5-20   | MFENCE barrier          |
| `fetch_add()`             | `relaxed`    | 2-5    | LOCK ADD                |
| `fetch_add()`             | `acq_rel`    | 2-5    | Same as relaxed         |
| `fetch_add()`             | `seq_cst`    | 10-30  | LOCK ADD + MFENCE       |
| `compare_exchange_weak()` | `acq_rel`    | 5-50   | LOCK CMPXCHG (may fail) |

**Key Insights:**
- On **x86**: acquire/release are nearly free (strong memory model)
- On **ARM**: acquire/release cost more (weaker memory model)
- `seq_cst` stores are expensive everywhere (MFENCE on x86, DMB on ARM)
- Atomic RMW operations (fetch_add, CAS) are always expensive

**Rule of Thumb:**
- Use `relaxed` when possible
- Use `acquire/release` for synchronization (cheap on x86)
- Avoid `seq_cst` unless truly needed

---

## 🔍 Common Atomic Operations

### 1. Load & Store

```cpp
std::atomic<int> x{0};

// Load
int val = x.load(std::memory_order_acquire);

// Store
x.store(42, std::memory_order_release);
```

### 2. Exchange (Atomic Swap)

```cpp
std::atomic<int> x{10};

// Atomically: old = x; x = 20; return old;
int old = x.exchange(20, std::memory_order_acq_rel);
// old == 10, x == 20
```

### 3. Fetch-Add / Fetch-Sub

```cpp
std::atomic<int> counter{0};

// Atomically: old = counter; counter += 5; return old;
int old = counter.fetch_add(5, std::memory_order_relaxed);

// Can also do: counter += 5 (operator overload)
counter += 5;  // Equivalent to fetch_add with seq_cst
```

### 4. Compare-Exchange (CAS)

**The most important atomic operation for lock-free programming!**

```cpp
std::atomic<int> x{10};

int expected = 10;
int desired = 20;

// Atomically:
// if (x == expected) {
//     x = desired;
//     return true;
// } else {
//     expected = x;  // Update expected!
//     return false;
// }
bool success = x.compare_exchange_weak(
    expected, desired,
    std::memory_order_acq_rel
);
```

**Weak vs Strong:**
- `compare_exchange_weak`: Can **spuriously fail** (faster, use in loops)
- `compare_exchange_strong`: Never spuriously fails (use when retrying is expensive)

**Usage Pattern:**
```cpp
// Typical CAS loop
int expected = x.load(std::memory_order_relaxed);
while (!x.compare_exchange_weak(
    expected, expected + 1,
    std::memory_order_acq_rel,
    std::memory_order_relaxed  // Ordering on failure
)) {
    // expected was updated by CAS, retry
}
```

---

## 🚨 Common Pitfalls

### 1. Using Relaxed for Synchronization

```cpp
❌ WRONG:
int data = 0;
std::atomic<bool> ready{false};

// Thread 1
data = 42;
ready.store(true, std::memory_order_relaxed);  // ❌

// Thread 2
if (ready.load(std::memory_order_relaxed)) {   // ❌
    assert(data == 42);  // CAN FAIL!
}
```

**Fix:** Use `release/acquire`

### 2. Mixing Atomic and Non-Atomic Access

```cpp
❌ WRONG:
std::atomic<int> x{0};

// Thread 1
x.store(42);

// Thread 2
int val = x;  // ❌ Non-atomic read! Undefined behavior!
```

**Fix:** Always use `.load()` and `.store()`

### 3. Assuming seq_cst is Free

```cpp
❌ WRONG (performance):
// Hot loop
for (int i = 0; i < 1000000; i++) {
    counter.fetch_add(1);  // Defaults to seq_cst - SLOW!
}
```

**Fix:** Use `relaxed` for counters

---

## 📊 Real-World Example: Spinlock

```cpp
class Spinlock {
    std::atomic<bool> locked_{false};

public:
    void lock() {
        // Try to acquire with CAS
        while (locked_.exchange(true, std::memory_order_acquire)) {
            // Spin while locked
            while (locked_.load(std::memory_order_relaxed)) {
                // Hint CPU we're spinning (reduces power)
                _mm_pause();  // x86: PAUSE instruction
            }
        }
    }

    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};
```

**Why this ordering?**
- `exchange(..., acquire)`: Acquire memory when entering critical section
- `load(..., relaxed)`: Just checking, no synchronization needed
- `store(..., release)`: Release memory when exiting critical section

---

## 🎓 Next Steps

After mastering atomics, you're ready for:
1. **Memory Barriers & Fences** - Explicit synchronization
2. **ABA Problem** - Understanding CAS pitfalls
3. **Hazard Pointers** - Safe memory reclamation
4. **Lock-Free Data Structures** - MPSC/MPMC queues

---

**Remember:** When in doubt, use `memory_order_seq_cst`. Correctness first, optimize later!
