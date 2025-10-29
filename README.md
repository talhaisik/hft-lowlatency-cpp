# Modern C++ Exploration: HFT and Low-Latency Systems

## 🔬 Project Overview

This repository serves as a personal deep-dive into modern C++ techniques, specifically explored through the lens of high-frequency trading (HFT) and low-latency system design. It's a hands-on learning journey exploring performance-critical programming concepts.

## 🎯 Learning Objectives

- Master performance-critical C++ idioms
- Understand low-latency system design
- Explore advanced concurrency techniques
- Deep dive into memory management
- Implement lock-free data structures
- Benchmark and optimize C++ code

## 📚 Study Areas

### Key Focus Domains
- Modern C++ Language Features
- Performance Optimization
- Concurrent Programming
- Memory Management
- Low-Latency Algorithm Design
- Systems Programming Techniques

## 🗺 Exploration Roadmap

### Phase 1: Modern C++ Foundations
**Goal:** Master C++20 language features and RAII patterns

- [x] **Move Semantics & Perfect Forwarding** ✅
- [x] **Smart Pointers (Unique, Shared, Weak)** ✅
  - **Components**: `UniquePtr`, `SharedPtr`, `WeakPtr`
  - **Performance**: Zero-overhead unique_ptr, lock-free shared_ptr
  - **Key learnings**:
    - RAII and deterministic resource management
    - Move semantics and perfect forwarding
    - Atomic reference counting with `compare_exchange_weak`
    - Memory ordering semantics (relaxed, acquire, release, acq_rel)
    - Breaking circular references with weak pointers
    - Custom deleters and type erasure
    - Control block design for shared ownership
    - Thread-safe reference counting vs object access
- [ ] Template Metaprogramming Basics
- [ ] Compile-Time Computation (constexpr, consteval)
- [ ] Concepts & Constraints (C++20)

### Phase 2: Memory Management Mastery
**Goal:** Understand and implement custom allocators

- [x] **Memory Pool Allocators (Fixed-size)** ✅
  - **Performance**: 31.8x faster than new/delete for pure allocation
  - **Throughput**: 724 million operations/second
  - **Key learnings**:
    - Intrusive free list with union for zero overhead
    - Separation of memory lifetime from object lifetime
    - Manual memory management vs RAII trade-offs
    - Placement new and explicit destructor calls
    - Cache-friendly sequential memory layout
- [ ] Arena Allocators (Bump allocation)
- [ ] Stack Allocators (Small object optimization)
- [ ] Custom STL Allocators (std::pmr)
- [ ] Memory-Mapped Files (mmap)
- [ ] Alignment & Padding Strategies

### Phase 3: Concurrency Foundations
**Goal:** Understand atomics and memory ordering

- [x] **Atomic Operations Deep Dive** ✅
  - **Coverage**: All 6 memory orderings (relaxed, acquire, release, acq_rel, seq_cst, consume)
  - **Patterns**: Spinlocks, reference counting, flag-protected data, work queues
  - **Key learnings**:
    - Memory ordering controls surrounding operations, not just the atomic
    - x86 vs ARM memory models (strong vs weak)
    - False sharing prevention with cache-line alignment
    - Compare-and-swap (CAS) as foundation of lock-free programming
    - When to use each memory ordering for performance
    - Proper CAS loop patterns (weak vs strong)
    - Two-phase spinlock optimization
- [ ] Memory Ordering Models (relaxed, acquire, release, seq_cst)
- [ ] Memory Barriers & Fences
- [ ] ABA Problem & Solutions
- [ ] Compare-and-Swap (CAS) Patterns
- [ ] Hazard Pointers (Safe Memory Reclamation)

### Phase 4: Lock-Free Data Structures
**Goal:** Build production-quality concurrent structures

- [x] **SPSC Ring Buffer (Single Producer/Consumer)** ✅
  - **Performance**: 31.5x faster than vector for circular buffer operations
  - **Throughput**: 544 million operations/second (SPSC)
  - **Key learnings**:
    - Cache line alignment to prevent false sharing
    - Acquire-release memory ordering for lock-free synchronization
    - Power-of-2 bit masking for fast modulo operations
    - Single Producer Single Consumer (SPSC) pattern optimization
    - Proper benchmarking methodology (warmup, multiple runs, fair comparisons)
- [ ] MPSC Queue (Multi-Producer Single-Consumer)
- [ ] MPMC Queue (Multi-Producer Multi-Consumer)
- [ ] Lock-Free Stack
- [ ] Lock-Free Hash Map
- [ ] Seqlock (Optimistic Reads)

### Phase 5: Hardware-Aware Programming
**Goal:** Optimize for CPU architecture

- [ ] CPU Cache Architecture (L1/L2/L3/TLB)
- [ ] Cache Line Alignment (64 bytes)
- [ ] False Sharing Prevention
- [ ] Prefetching Techniques
- [ ] Branch Prediction Optimization
- [ ] NUMA Architecture & Allocation

### Phase 6: SIMD & Vectorization
**Goal:** Parallel data processing

- [ ] SSE/AVX Intrinsics Basics
- [ ] AVX2 for Batch Operations
- [ ] Auto-Vectorization Hints
- [ ] Aligned Memory for SIMD
- [ ] Practical Use Cases (checksum, parsing, price calculations)

### Phase 7: Performance Engineering
**Goal:** Measure and optimize systematically

- [ ] Benchmarking Methodology (Google Benchmark)
- [ ] CPU Performance Counters (perf)
- [ ] Latency Measurement (rdtsc)
- [ ] Cache Miss Analysis
- [ ] Flame Graphs & Profiling
- [ ] Statistical Analysis of Performance

### Phase 8: System-Level Optimization
**Goal:** OS and kernel tuning

- [ ] CPU Pinning (taskset, sched_setaffinity)
- [ ] Real-Time Scheduling (SCHED_FIFO)
- [ ] CPU Isolation (isolcpus)
- [ ] Huge Pages (2MB/1GB pages)
- [ ] Kernel Bypass Networking (Introduction)
- [ ] Zero-Copy Techniques

### Phase 9: Integration & Real-World Patterns
**Goal:** Combine techniques for production systems

- [ ] Event-Driven Architecture
- [ ] Zero-Allocation Hot Paths
- [ ] Backpressure Handling
- [ ] Thread Communication Patterns
- [ ] Error Handling Without Exceptions
- [ ] Configuration-Driven Design

## 🛠 Technical Approach

### Guiding Principles
- **Minimal Abstractions**: Zero-overhead design
- **Performance First**: Benchmark-driven development
- **Modern C++**: Leveraging C++20/C++23 features
- **Educational Focus**: Clear, commented implementations

## 🏗 Project Structure

```
hft-lowlatency-cpp/
│
├── 01_modern_cpp_basics/
│   └── move_semantics.cpp
│
├── 02_ring_buffer/
│   ├── include/
│   │   └── ring_buffer.hpp
│   ├── src/
│   │   └── ring_buffer_benchmark.cpp
│   └── docs/
│       └── LEARNINGS.md
│
├── 03_memory_pool/
│   ├── include/
│   │   └── memory_pool.hpp
│   ├── src/
│   │   └── memory_pool_benchmark.cpp
│   │   └── memory_pool_example.cpp
│   ├── docs/
│   │   └── DESIGN.md
│   └── CMakeLists.txt
│
├── 04_smart_pointers/
│   ├── include/
│   │   ├── unique_ptr.hpp
│   │   ├── shared_ptr.hpp
│   │   └── weak_ptr.hpp
│   ├── src/
│   │   ├── minimal_test.cpp
│   │   └── smart_ptr_demo.cpp
│   ├── docs/
│   │   ├── UNIQUE_PTR.md
│   │   ├── SHARED_PTR.md
│   │   └── SUMMARY.md
│   └── CMakeLists.txt
│
├── 05_atomics/
│   ├── include/
│   │   └── atomic_examples.hpp
│   ├── src/
│   │   ├── atomic_demo.cpp
│   │   └── atomic_benchmark.cpp
│   ├── docs/
│   │   ├── ATOMIC_OPERATIONS.md
│   │   └── LEARNINGS.md
│   └── CMakeLists.txt
│
└── (Future Modules)
```

## 🚀 Build Instructions

### Prerequisites
- CMake 3.20+
- C++20 Compiler
  - MSVC 19.30+
  - GCC 11+
  - Clang 14+

### Linux Build
```bash
git clone https://github.com/your-username/hft-lowlatency-cpp.git
cd hft-lowlatency-cpp
mkdir build && cd build
cmake ..
make -j
```

### Windows Build
```powershell
git clone https://github.com/your-username/hft-lowlatency-cpp.git
cd hft-lowlatency-cpp
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

## 🤔 Why This Approach?

This repository is not a production trading system, but a systematic exploration of:
- How modern C++ can achieve extreme performance
- Techniques used in high-frequency trading systems
- Low-level system design principles
- Performance optimization strategies

## 📊 Performance Philosophy

- **Goal**: Understanding, not absolute speed
- **Method**: Clear, educational implementations
- **Focus**: Learning performance techniques
