# Atomic Operations Deep Dive

## 📖 Overview

This module provides a comprehensive deep-dive into C++ atomic operations and memory ordering models. It covers everything you need to know to write correct, high-performance lock-free code.

## 🎯 What You'll Learn

- **Atomicity vs Synchronization:** Understanding the difference
- **All 6 Memory Orderings:** When and why to use each one
- **Hardware Memory Models:** x86 (strong) vs ARM (weak)
- **Common Patterns:** Spinlocks, reference counting, work queues
- **Performance Impact:** Benchmarks showing real costs
- **False Sharing:** How to detect and eliminate it
- **Compare-and-Swap (CAS):** The foundation of lock-free programming

## 📁 Module Contents

```
05_atomics/
├── include/
│   └── atomic_examples.hpp       # 8 complete atomic patterns
├── src/
│   ├── atomic_demo.cpp           # Interactive demonstrations
│   └── atomic_benchmark.cpp      # Performance benchmarks
├── docs/
│   ├── ATOMIC_OPERATIONS.md      # Comprehensive guide (tutorial)
│   └── LEARNINGS.md              # Key insights and takeaways
└── CMakeLists.txt
```

## 🔨 Building

### From Project Root
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release

# Or on Windows with Visual Studio:
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

### From This Directory
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## 🚀 Running

### 1. Interactive Demo (Recommended to start)

```bash
# Linux/Mac
./build/05_atomics/atomic_demo

# Windows
.\build\05_atomics\Release\atomic_demo.exe
```

**What it shows:**
- Relaxed counter with 4 threads
- Flag-protected data (acquire/release)
- Spinlock protecting shared counter
- Reference counting patterns
- CAS loops
- Work queue (SPSC)
- Sequential consistency demo
- Relaxed ordering failure (race condition)

**Expected output:**
```
=== Demo 1: Relaxed Counter ===
Threads: 4
Increments per thread: 100000
Expected count: 400000
Actual count: 400000
Duration: 12453 μs
Throughput: 32.11 million ops/sec
[PASS] Count is correct!

...
```

### 2. Performance Benchmarks

```bash
# Linux/Mac
./build/05_atomics/atomic_benchmark

# Windows
.\build\05_atomics\Release\atomic_benchmark.exe
```

**What it measures:**
- Load performance (relaxed vs acquire vs seq_cst)
- Store performance (relaxed vs release vs seq_cst)
- fetch_add performance across memory orderings
- Compare-exchange (CAS) performance
- Multi-threaded contention effects
- False sharing impact (2-4x speedup from eliminating it!)

**Expected insights:**
- On x86: `relaxed` ≈ `acquire`/`release` for loads/stores
- `seq_cst` stores are **10-20x slower** (MFENCE overhead)
- False sharing causes **2-4x slowdown**
- Multi-threaded contention significantly reduces throughput

## 📚 Study Guide

### For Beginners:
1. Read [`docs/ATOMIC_OPERATIONS.md`](docs/ATOMIC_OPERATIONS.md) - Start here!
2. Run `atomic_demo` and understand each demo
3. Read the code in `include/atomic_examples.hpp`
4. Run `atomic_benchmark` and understand the results

### For Intermediate:
1. Review [`docs/LEARNINGS.md`](docs/LEARNINGS.md) for key insights
2. Implement your own spinlock using atomics
3. Modify the work queue to use different memory orderings
4. Measure performance changes

### For Advanced:
1. Port examples to ARM and compare performance
2. Implement a lock-free stack using CAS
3. Add TSAN (ThreadSanitizer) to detect races
4. Study generated assembly (`-S -masm=intel`)

## 🔍 Code Examples

### Example 1: Relaxed Counter
```cpp
std::atomic<uint64_t> count{0};

// Multiple threads can safely do:
count.fetch_add(1, std::memory_order_relaxed);

// Order doesn't matter, just want accurate count
```

### Example 2: Flag-Protected Data
```cpp
int data = 0;
std::atomic<bool> ready{false};

// Producer
data = 42;
ready.store(true, std::memory_order_release);

// Consumer  
if (ready.load(std::memory_order_acquire)) {
    assert(data == 42);  // Always passes!
}
```

### Example 3: Spinlock
```cpp
std::atomic<bool> locked{false};

// Lock
while (locked.exchange(true, std::memory_order_acquire)) {
    while (locked.load(std::memory_order_relaxed)) {
        _mm_pause();  // Spin with less contention
    }
}

// Unlock
locked.store(false, std::memory_order_release);
```

## 📊 Performance Results (Example Hardware)

**Single-threaded operations (x86-64, 4.0 GHz):**
| Operation | Memory Order | ns/op | Speedup vs seq_cst |
|-----------|-------------|-------|-------------------|
| `store()` | `relaxed` | 1.0 | **20x faster** |
| `store()` | `release` | 1.0 | **20x faster** |
| `store()` | `seq_cst` | 18.5 | 1x (baseline) |
| `fetch_add()` | `relaxed` | 2.3 | **10x faster** |
| `fetch_add()` | `seq_cst` | 25.0 | 1x (baseline) |

**False sharing impact:**
- Without padding: 8.2M ops/sec
- With padding: 24.5M ops/sec
- **Speedup: 3.0x**

## ⚠️ Common Mistakes

### ❌ Wrong: Using relaxed for synchronization
```cpp
data = 42;
flag.store(true, std::memory_order_relaxed);  // Bug!
```

### ✅ Correct: Use acquire/release
```cpp
data = 42;
flag.store(true, std::memory_order_release);  // OK
```

### ❌ Wrong: Mixing atomic and non-atomic access
```cpp
std::atomic<int> x{0};
int val = x;  // Undefined behavior!
```

### ✅ Correct: Always use atomic operations
```cpp
int val = x.load(std::memory_order_relaxed);  // OK
```

## 🎓 Next Steps

After mastering this module, you're ready for:
1. **Memory Barriers & Fences** - Explicit synchronization
2. **ABA Problem** - Understanding CAS pitfalls
3. **Hazard Pointers** - Safe memory reclamation
4. **MPSC/MPMC Queues** - Advanced lock-free structures

## 📖 Resources

- **C++ Reference:** https://en.cppreference.com/w/cpp/atomic
- **Preshing on Programming:** https://preshing.com/archives/
- **Herb Sutter's "atomic Weapons"** - CppCon talks
- **"C++ Concurrency in Action"** - Anthony Williams (Chapters 5-7)

## 🐛 Debugging Tips

### Use ThreadSanitizer (TSAN)
```bash
# Compile with TSAN
g++ -fsanitize=thread -g atomic_demo.cpp -pthread

# Run and detect data races
./a.out
```

### Examine Generated Assembly
```bash
# See what the compiler generates
g++ -S -masm=intel -O3 atomic_examples.cpp

# Look for:
# - LOCK prefix (atomics)
# - MFENCE (seq_cst barriers)
# - MOV vs LOCK CMPXCHG
```

## 💡 Pro Tips

1. **Start with `seq_cst`**, optimize to relaxed only after profiling
2. **Always pad hot atomics** to 64 bytes (cache line size)
3. **Use two-phase spinlocks** to reduce contention
4. **Benchmark on target hardware** (x86 vs ARM behave differently)
5. **Use TSAN** during development to catch races early

---

**Ready to master atomics?** Start with the demo, then dive into the documentation!

