# hft-lowlatency-cpp

This repository is a personal notebook of modern C++ concepts applied in the context of **high-frequency trading (HFT)** and **low-latency systems**. It consists of focused, self-contained examples exploring performance-critical idioms, concurrency, memory management, and lock-free data structures.

> Each subdirectory is structured as a CMake target and can be compiled independently.

---

## 📁 `01_modern_cpp_basics`

Explores core modern C++ features relevant to performance:

- `move_semantics.cpp`:
  - Copy vs Move constructors
  - Return Value Optimization (RVO), Named RVO
  - `std::vector::emplace_back` vs `push_back`
  - `std::move_if_noexcept`, `std::is_nothrow_move_constructible`

Upcoming:
- `copy_elision_benchmark.cpp` (micro-benchmarks)
- `std::optional` and copy elision
- `std::function` vs `move_only_function`

---

## 📁 `02_ring_buffer` *(planned)*

Lock-free ring buffer implementation:
- `std::atomic` with `memory_order_relaxed`
- Single-producer, single-consumer (SPSC) design
- Benchmarks vs `std::queue`
- Integration with producer-consumer pipeline

---

## 🔧 Build Instructions

Assuming WSL or native Linux with g++ ≥ 11:

```bash
git clone https://github.com/talha-isik/hft-lowlatency-cpp.git
cd hft-lowlatency-cpp
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=1
cmake --build build -j
./build/01_modern_cpp_basics/move_semantics
