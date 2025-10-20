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

### Phase 1: C++ Fundamentals
- [x] Move Semantics
- [x] Compile-Time Optimizations
- [x] **Lock-Free Ring Buffer Implementation** ✅
  - **Performance**: 31.5x faster than vector for circular buffer operations
  - **Throughput**: 544 million operations/second (SPSC)
  - **Key learnings**:
    - Cache line alignment to prevent false sharing
    - Acquire-release memory ordering for lock-free synchronization
    - Power-of-2 bit masking for fast modulo operations
    - Single Producer Single Consumer (SPSC) pattern optimization
    - Proper benchmarking methodology (warmup, multiple runs, fair comparisons)
- [ ] Smart Pointer Deep Dive
- [ ] Advanced Memory Management Techniques

### Phase 2: Concurrency and Performance
- [ ] Lock-Free Data Structures
  - [ ] MPSC Concurrent Queue (with CAS operations)
  - [ ] Lock-Free Hash Map
  - [ ] Atomic Smart Pointers
- [ ] Memory Pool Allocators
- [ ] Thread Synchronization Primitives
- [ ] Cache-Aware Programming Techniques

### Phase 3: Low-Latency Design Patterns
- [ ] Zero-Copy Message Passing
- [ ] High-Performance Serialization
- [ ] SIMD and Vectorization Techniques
- [ ] Minimizing Allocation Overhead
- [ ] Context Switching Optimization

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

## 📈 Progress Notes

### Completed Modules

#### Lock-Free Ring Buffer (SPSC)

**What I Built**:
- Single Producer Single Consumer lock-free ring buffer
- Power-of-2 sized buffer with bit-masking for fast indexing
- Cache-line aligned atomics to prevent false sharing

**Key Technical Insights**:
- Memory ordering: Acquire-release pairs create synchronization points without seq_cst overhead
- False sharing: Separate cache lines for head/tail prevented ~10x performance degradation
- SPSC optimization: No CAS needed when single thread owns each write index

**Performance Results**:
- 31.5x faster than std::vector for circular buffer operations
- 544 Million operations/second throughput
- Consistent ~16-24ms for 10M operations across runs

**What I Learned**:
- Atomics provide indivisibility, not just thread-safety
- Cache coherency is critical for multi-threaded performance
- Proper benchmarking methodology prevents misleading results
- Lock-free doesn't always mean CAS - ownership patterns matter

**Future Improvements to Explore**:
- Multi-threaded producer/consumer benchmark
- MPSC variant with CAS operations
- Performance profiling with perf/vtune
- Different memory ordering experiments

---

## 🎓 Next Steps

**Immediate**: Memory Pool Allocator
- Eliminate heap allocation overhead
- O(1) allocation/deallocation
- Cache-aware design patterns

**Future**: Lock-Free MPSC Queue, Advanced Allocators, SIMD optimizations