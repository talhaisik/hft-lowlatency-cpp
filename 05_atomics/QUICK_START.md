# Quick Start: Atomic Operations Module

## ⚡ 5-Minute Quick Start

### 1. Build
```bash
cd 05_atomics
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### 2. Run Demo
```bash
# Linux/Mac
./atomic_demo

# Windows
.\Release\atomic_demo.exe
```

### 3. Run Benchmarks
```bash
# Linux/Mac
./atomic_benchmark

# Windows  
.\Release\atomic_benchmark.exe
```

## 📚 What to Read

**Start here:** [`docs/ATOMIC_OPERATIONS.md`](docs/ATOMIC_OPERATIONS.md)
- Complete tutorial on atomics
- All 6 memory orderings explained
- When to use each one
- Real-world examples

**Then read:** [`docs/LEARNINGS.md`](docs/LEARNINGS.md)
- Key insights and patterns
- Performance results
- Common pitfalls
- Best practices

## 🎯 Key Takeaways (TL;DR)

### Memory Orderings Quick Reference:

| Use Case           | Memory Order        | Why                         |
|--------------------|---------------------|-----------------------------|
| Simple counter     | `relaxed`           | Fast, order doesn't matter  |
| Flag + data        | `acquire`/`release` | Synchronization needed      |
| Spinlock           | `acquire`/`release` | Critical section boundaries |
| fetch_add in queue | `acq_rel`           | Both read and modify        |
| Complex/unsure     | `seq_cst`           | Safest (but slower)         |

### Performance Rules:
1. **`seq_cst` is 10-20x slower** than `relaxed` for stores (on x86)
2. **False sharing causes 2-4x slowdown** - pad to 64 bytes!
3. **On x86:** `acquire`/`release` are nearly free
4. **On ARM:** `acquire`/`release` cost more

### Common Patterns:

**Counter:**
```cpp
counter.fetch_add(1, std::memory_order_relaxed);
```

**Flag + Data:**
```cpp
// Producer
data = value;
flag.store(true, std::memory_order_release);

// Consumer
if (flag.load(std::memory_order_acquire)) {
    use(data);
}
```

**Spinlock:**
```cpp
while (lock.exchange(true, std::memory_order_acquire)) {
    while (lock.load(std::memory_order_relaxed)) {
        _mm_pause();
    }
}
```

## 🚀 What's Next?

After this module, you're ready for:
1. Memory Barriers & Fences
2. ABA Problem & Solutions
3. Hazard Pointers
4. Lock-Free Data Structures (MPSC/MPMC)
