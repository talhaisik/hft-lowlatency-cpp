# ITCH/OUCH Trading System Architecture

**Version:** 1.0 FINAL  
**Date:** October 2025  
**Type:** Educational Low-Latency Market Data & Order Entry System

---

## 🎯 Purpose

Build a production-quality educational trading system demonstrating:
- Real-world HFT protocols (NASDAQ ITCH 5.0 and OUCH 4.2)
- Low-latency programming techniques (< 10 μs internal processing)
- Modern C++20 best practices
- Lock-free data structures
- Financial market microstructure

**Primary Use Cases:** Learning HFT systems, demonstrating technical expertise, building foundation for production systems.

**Key Metrics:**
- Internal latency: < 10 μs (tick-to-order-send)
- Full round-trip: 70-200 μs (including network + venue)
- Throughput: 100K+ messages/second per symbol
- Uptime target: 99.9% (during market hours)

---

## 📊 System Architecture

```
Market Data → Parse → Order Book → Strategy → Risk → Build Orders → Exchange
   (ITCH)      ↓         ↓            ↓        ↓         (OUCH)        ↓
            Sequence  Maintain L2  Generate  Validate   SoupBinTCP   Match &
            & Gaps    Dense Ladder  Signals   Limits     Session      Confirm
```

### Critical Path Latency
1. **Parse ITCH:** 500 ns (safe memcpy + endian swap)
2. **Update Book:** 500 ns (O(1) dense array access)
3. **Strategy Decision:** 5 μs (market making logic)
4. **Risk Check:** 200 ns (token bucket + hash lookups)
5. **Build OUCH:** 300 ns (struct packing)
6. **Total Internal:** ~7 μs

### Data Flow Philosophy
- **Event-driven:** Triggered by market data, not polling
- **Single-writer:** One thread modifies each data structure
- **Lock-free reads:** Readers use seqlock for top-of-book
- **Zero-copy:** Reference buffers in place when safe
- **Pre-allocated:** No heap allocations in hot path

---

## 🏗️ Core Components

### 1. Network Layer

**Purpose:** Ingest market data, send orders

**Design:**
- **UDP Multicast** for ITCH (low latency broadcast)
- **TCP** for OUCH (reliability required)
- **Pluggable I/O:** Standard sockets → AF_XDP/DPDK for production

**Resilience:**
- Automatic reconnection with exponential backoff (100ms, 200ms, 400ms, ...)
- Multicast rejoin on failure
- SSL/TLS support for order sessions
- Sequence number replay after disconnect

---

### 2. MoldUDP64 Handler

**Purpose:** Unwrap packets, detect gaps, request retransmits

**Key Features:**
- Extract session ID and sequence from each packet
- Detect gaps in sequence numbers
- Request retransmits on separate channel (TCP/UDP)
- Buffer out-of-order packets (bounded to 200KB default)
- Exponential backoff for retransmit requests

**Gap Policy:**
- Max gap window: 1000 messages (configurable)
- Beyond window: Fatal error, restart feed
- Recovery metrics: Track gap frequency, recovery time

**Performance:** < 100 ns unwrap overhead per packet

---

### 3. ITCH Message Parser

**Purpose:** Deserialize 20+ ITCH 5.0 binary message types

**Supported Messages:**
- System: Market open/close, halts, short sale restrictions
- Order Book: Add, execute, cancel, delete, replace
- Trades: Regular and cross trades, broken trades

**Design:**
- **Safe parsing:** memcpy to avoid undefined behavior
- **Zero-copy:** Reference buffer data when lifetime guaranteed
- **Type-safe:** std::variant for message dispatch
- **Validated:** Size checks, message type verification

**Optimizations:**
- SIMD (AVX2) for batch endian conversions
- Shared buffer ownership (shared_ptr) when needed

**Error Handling:**
- Malformed messages: Log, skip, continue
- Invalid types: Track metrics, alert
- Checksum failures: Request retransmit

**Performance:** < 500 ns per message

---

### 4. Order Book Manager

**Purpose:** Maintain accurate Level 2 order book

**Design Choice: Dense Price Ladder**

**Why not std::map?**
- std::map: O(log N) updates, poor cache locality, ~100 ns
- Dense ladder: O(1) updates, excellent cache, ~10 ns
- **10x performance improvement**

**Implementation:**
- Array indexed by: (price - min_price) / tick_size
- Each index holds aggregate quantity and order count
- Best bid/ask tracked incrementally (no search needed)

**Memory Trade-off:**
- 20,000 price points × 8 bytes = 160 KB per symbol
- 100 symbols = 16 MB (acceptable for educational system)

**For Volatile Stocks:**
- Hybrid approach: Core ladder + overflow map
- Rare prices use std::map (slow path, infrequent)

**Lock-Free Top-of-Book:**
- Seqlock protocol for readers
- Single writer updates sequence number + data
- Readers retry if sequence changed during read

**Integrity:**
- Periodic validation of total quantities (off hot path)
- Cross-check with ITCH message stream

**Performance:** < 500 ns per update

---

### 5. Strategy Engine

**Purpose:** Generate trading signals

**Architecture:**
- **Pluggable:** Interface-based design
- **Event-driven:** Callbacks on book updates
- **Stateful:** Maintain positions, PnL, history

**Example: Market Maker**
- Monitor spread (ask - bid)
- If spread > threshold: Quote inside spread
- Manage inventory: Adjust quotes based on position
- Cancel/replace when market moves

**Advanced Features:**
- **Backtesting:** Replay historical ITCH data, record hypothetical trades
- **ML Integration:** PyTorch C++ API for predictions (optional)
- **Pairs Trading:** Monitor correlated symbols, exploit divergence

**Multi-Symbol Support:**
- Single thread: Process all symbols sequentially (simple)
- Symbol sharding: Group symbols across threads (scalable)
- Thread-per-symbol: Dedicated thread (maximum isolation)

**Performance:** < 5 μs decision time

---

### 6. Risk Manager

**Purpose:** Validate orders before submission

**Pre-Trade Checks:**
1. **Symbol whitelist:** Only approved symbols
2. **Market state:** Synced with ITCH system events (open/halted/closed)
3. **Price collar:** Reject orders > 5% from mid-price (configurable)
4. **Position limits:** Per-symbol and total exposure
5. **Rate limiting:** Token bucket (100 orders/sec default)
6. **Short sale restrictions:** Enforce SSR from ITCH 'Y' messages

**Advanced Risk:**
- **Value-at-Risk (VaR):** Historical simulation, 95% confidence
- **Stress testing:** Simulate adverse scenarios
- **Real-time PnL:** Track unrealized gains/losses

**Token Bucket Algorithm:**
- O(1) complexity vs O(k) sliding window
- Refill rate: 100 tokens/second
- Burst capacity: 200 tokens
- No allocations, no queue cleanup

**Thread Safety:**
- Positions: Read-write lock (shared_mutex)
- Token bucket: Single-threaded per component

**Performance:** < 200 ns per check

---

### 7. OUCH Order Manager

**Purpose:** Build spec-compliant OUCH 4.2 messages

**Critical Specifications:**
- Price: Dollars × 10,000 (uint32_t big-endian)
- Time-in-force: Seconds (0 = Day order)
- Capacity: Single char ('A'/'P'/'R'), not int64!
- All multi-byte: Big-endian (network byte order)

**Order Token Generation:**
- Format: 14-byte ASCII (YYYYMMDDHHMMSS + sub-second)
- Uniqueness: Timestamp (μs) + atomic sequence counter
- Prevents collisions even at sub-millisecond rates

**Lifecycle Tracking:**
- Map token → order state
- Idempotent handling: Ignore duplicate acks
- Track: Sent, accepted, partial fills, filled, canceled, rejected

**Optimizations:**
- Object pooling: Reuse order structures
- Pre-allocated buffers: No runtime allocation
- Audit logging: All events to append-only log

**Performance:** < 300 ns message build time

---

### 8. SoupBinTCP Client

**Purpose:** Manage TCP session for order entry

**Session Lifecycle:**
1. Connect → Login → Receive session ID + start sequence
2. Send/receive sequenced data packets (OUCH wrapped)
3. Heartbeat every 1 second
4. On disconnect: Reconnect + replay from last sequence

**Key Features:**
- **Async I/O:** Non-blocking TCP (epoll/io_uring)
- **Sequence management:** Track inbound/outbound
- **Replay buffer:** Store sent messages for recovery
- **Configurable timeouts:** Heartbeat, reconnect delays

**Security:**
- Optional SSL/TLS encryption
- Certificate validation
- Secure credential storage

---

### 9. Exchange Simulator

**Purpose:** Test environment mimicking NASDAQ

**Features:**
- **Price-time priority:** Standard matching algorithm
- **ITCH generation:** Creates Add/Execute/Cancel messages
- **Latency simulation:** Configurable base + jitter (50±10 μs)
- **Multi-client:** Support multiple simultaneous connections

**Testing Modes:**
- **Deterministic:** Repeatable for regression tests
- **Realistic:** Variable latencies, occasional rejects
- **Chaos:** Random packet drops (0.1%), order rejects (1%), halts

**Integration:**
- Separate process for realism
- Loopback network for low latency
- Can run distributed for multi-venue simulation

---

## 🎯 Design Principles & Rationale

### 1. Zero-Copy Where Possible

**Decision:** Avoid memory allocations in hot path

**Implementation:**
- Reference data in original buffers (std::span)
- Shared buffer ownership (shared_ptr) when needed
- Pre-allocated pools for messages and orders

**Why:** Allocations cost 50-100 ns minimum. At microsecond latencies, allocations dominate.

---

### 2. Single-Writer Concurrency

**Decision:** One thread modifies each data structure

**Implementation:**
- Order book: Single writer, lock-free readers (seqlock)
- Queues: SPSC (single producer, single consumer)
- No mutexes on critical path

**Why:** Locks add 20-50 ns overhead and contention risk. Single writer + lock-free readers is faster and simpler.

---

### 3. Cache-Friendly Data Structures

**Decision:** Optimize for CPU cache hierarchy

**Implementation:**
- Cache-line alignment (64 bytes) for hot structures
- Separate hot data from cold data
- Dense arrays over sparse trees
- Sequential access patterns

**Why:** Cache misses cost 50-100 ns. Proper alignment eliminates most misses.

---

### 4. Fail-Fast Error Handling

**Decision:** Validate early, handle errors explicitly

**Implementation:**
- Check sizes and types before processing
- Use return codes on hot path (not exceptions)
- Exceptions only for unrecoverable errors
- Comprehensive metrics and logging

**Why:** Exception unwinding is slow (microseconds). Hot path must return quickly.

---

### 5. Configuration-Driven

**Decision:** All parameters in external config

**Implementation:**
- YAML configuration files
- Per-symbol specifications (tick size, price ranges)
- Runtime limits (positions, rates, prices)
- No hardcoded magic numbers

**Why:** Different symbols have different characteristics. Configuration allows adaptation without recompilation.

---

## 🧵 Threading Models

### Industry Reality: What Elite HFT Firms Use

**Firms like Optiver, Citadel, and Jump Trading use: Symbol-Per-Core Architecture**

```
Core 0 (Isolated): AAPL  → Full pipeline (Parse → Book → Strategy → Orders)
Core 1 (Isolated): MSFT  → Full pipeline
Core 2 (Isolated): GOOGL → Full pipeline
Core 3 (Isolated): AMZN  → Full pipeline
...
Core N (Isolated): Market data receiver (multicast fanout)
```

**Key Characteristics:**
- **One symbol = One dedicated CPU core** (no time-sharing)
- **CPU isolation:** Removed from OS scheduler (`isolcpus` boot parameter)
- **CPU pinning:** Process locked to core (`sched_setaffinity`)
- **Real-time priority:** SCHED_FIFO with priority 99
- **No interrupts:** IRQs steered away from trading cores
- **Shared-nothing:** Each core owns its data exclusively (zero locks)

**Why This Model:**
- **Predictable latency:** Eliminates OS scheduling jitter (< 1 μs worst-case)
- **Cache efficiency:** Each core's L1/L2 contains only its symbol data (no bouncing)
- **Fault isolation:** Bug in one symbol doesn't affect others
- **Linear scaling:** More symbols = more cores (no contention)

**System Tuning Required:**
```bash
# Boot parameters
isolcpus=1-15                    # Isolate cores from scheduler
intel_pstate=disable             # Lock CPU frequency
processor.max_cstate=0           # Disable CPU sleep states
hugepages=4096                   # 2MB pages for TLB efficiency

# Runtime
taskset -c 5 ./aapl_trader      # Pin to core 5
chrt -f 99 ./aapl_trader        # Real-time priority
numactl --cpunodebind=0 --membind=0  # NUMA locality
```

---

### Our Educational Implementation: Progressive Models

#### Option A: Single-Threaded (Phase 1 - Recommended Start)

**Design:**
```
Main Loop:
  Receive packet → Unwrap → Parse → Update book →
  Run strategy → Check risk → Build order → Send
```

**Advantages:**
- ✅ No synchronization complexity
- ✅ Deterministic execution (easy debugging)
- ✅ Sufficient for 100-500K msgs/sec per symbol
- ✅ Learn fundamentals without threading complexity

**Disadvantages:**
- ❌ Limited to single core (~500K msgs/sec ceiling)
- ❌ I/O can block processing

**When to Use:** Initial implementation, learning, single-symbol focus

---

#### Option B: Symbol-Per-Core (Phase 2 - Production Pattern)

**Design:**
```
Core 1: AAPL  (dedicated: ingest → parse → book → strategy → orders)
Core 2: MSFT  (dedicated: full pipeline)
Core 3: GOOGL (dedicated: full pipeline)
Core 4: AMZN  (dedicated: full pipeline)
...
Core 0: Market data fanout (receives multicast, distributes to cores)
```

**Implementation:**
- Each symbol process pinned to dedicated core
- Shared memory ring buffers for market data distribution
- Lock-free SPSC queues (single producer, single consumer)
- Zero cross-core communication (shared-nothing)

**Market Data Fanout Core:**
```
Receive UDP → Unwrap MoldUDP64 → Dispatch by symbol
    ↓ (zero-copy via shared memory ring)
Symbol cores read their data independently
```

**Advantages:**
- ✅ Matches production HFT architecture
- ✅ Predictable latency (no scheduling jitter)
- ✅ Linear scaling (N symbols = N cores)
- ✅ No lock contention (shared-nothing)

**Disadvantages:**
- ❌ Requires tuned Linux (isolcpus, hugepages)
- ❌ Higher CPU count needed
- ❌ More complex deployment

**When to Use:** 
- Multiple symbols with high message rates
- Demonstrating production knowledge to recruiters
- Target latency < 5 μs

**CPU Affinity Example:**
```cpp
void pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    
    // Set real-time priority
    struct sched_param param;
    param.sched_priority = 99;
    sched_setscheduler(0, SCHED_FIFO, &param);
}

int main() {
    pin_to_core(5);  // This process owns core 5
    // ... trading logic
}
```

---

#### Option C: Multi-Symbol Sharding (Alternative)

**Design:**
```
Thread 1: Symbols 0-24   (AAPL, MSFT, ...)
Thread 2: Symbols 25-49  (GOOGL, AMZN, ...)
Thread 3: Symbols 50-74  (NVDA, AMD, ...)
Thread 4: Symbols 75-99  (INTC, ...)
```

**Advantages:**
- ✅ Lower core count (4 cores vs 100 cores)
- ✅ Simpler than symbol-per-core
- ✅ Still scalable to 4× throughput

**Disadvantages:**
- ❌ Load imbalance if symbols have different activity
- ❌ Shared data structures need synchronization
- ❌ Not production HFT pattern

**When to Use:** Educational middle-ground, limited cores available

---

### NUMA Awareness (Critical for Production)

**Modern servers have multiple CPU sockets:**
```
NUMA Node 0:              NUMA Node 1:
├─ CPU 0-15              ├─ CPU 16-31
├─ Memory Bank 0         ├─ Memory Bank 1
└─ NIC 0 (local)         └─ NIC 1 (local)
```

**Golden Rule:** Allocate memory on same NUMA node as CPU

**Performance Impact:**
- Local memory: 80-100 ns
- Remote memory: 150-200 ns (50-100% slower!)

**Implementation:**
```bash
# Check NUMA topology
numactl --hardware

# Run with NUMA binding
numactl --cpunodebind=0 --membind=0 ./trader_node0
numactl --cpunodebind=1 --membind=1 ./trader_node1

# Verify with
numastat -p $(pidof trader_node0)
```

---

### Backpressure Strategy

**Problem:** What if processing can't keep up with market data?

**Solutions:**

1. **Drop Oldest (Recommended for momentum strategies)**
   - Ring buffer: Overwrite oldest data
   - Prefer recent market state
   - Risk: Missing historical context

2. **Drop Newest (Recommended for bookkeeping)**
   - Queue: Reject new when full
   - Preserve message ordering
   - Risk: Stale market view

3. **Block Producer (Recommended for accuracy)**
   - Slow down ingestion
   - Guarantee no drops
   - Risk: Increased latency, potential timeout

**Implementation:**
```cpp
enum class BackpressurePolicy {
    DROP_OLDEST,   // Overwrite ring buffer
    DROP_NEWEST,   // Reject at queue
    BLOCK_SENDER   // Apply backpressure
};

// Monitor queue depth
if (queue.size() > high_watermark) {
    metrics_.backpressure_events++;
    apply_policy(policy_);
}
```

**Metrics to Track:**
- Queue depth (current/max/p99)
- Messages dropped (count/rate)
- Backpressure events (frequency)
- End-to-end latency (under load)

---

### Kernel Bypass Networking (Production Evolution)

**Standard Sockets:** 10-50 μs latency (kernel overhead)

**Kernel Bypass Options:**
- **Solarflare Onload:** 1-3 μs (TCP/UDP bypass)
- **Mellanox VMA:** 1-2 μs (RDMA acceleration)
- **ExaNIC:** 500-800 ns (FPGA-based NIC)
- **Custom FPGA:** 200-500 ns (hardware parsing)

**Why Elite Firms Use It:**
- Eliminates kernel context switches
- Direct NIC → userspace memory
- Zero-copy packet handling
- Predictable latency (no OS jitter)

**For Educational Project:**
- Phase 1: Standard sockets (learning)
- Phase 2: Document kernel bypass (show knowledge)
- Phase 3: Integrate Onload (if pursuing HFT job)

---

### Threading Model Recommendation for This Project

**Phase 1 (Weeks 1-4): Single-Threaded**
- Focus: Learn protocols, get it working
- Sufficient for educational demonstration
- Easy to debug and reason about

**Phase 2 (Optional Enhancement): Symbol-Per-Core**
- Focus: Demonstrate production knowledge
- Show understanding of HFT architecture

**Phase 3 (Advanced): Add NUMA + Kernel Bypass**
- Focus: Real production pattern
- Kernel bypass with Solarflare Onload
- NUMA-aware memory allocation
- Competitive with real HFT systems

---

### What HFT Recruiters Look For

**Core Knowledge (Must Have):**
- ✅ Understand why symbol-per-core is used
- ✅ Know what NUMA is and why it matters
- ✅ Explain cache line size (64 bytes) and false sharing
- ✅ Understand lock-free programming (no mutexes)
- ✅ Know latency of CPU operations (L1/L2/L3/RAM)

**Advanced Knowledge (Strong Plus):**
- ✅ Experience with kernel bypass (Onload/VMA)
- ✅ CPU pinning and real-time scheduling
- ✅ NUMA-aware allocation
- ✅ Latency percentiles (p99.99 matters more than average)
- ✅ Hardware counters (cache misses, branch mispredictions)

**Interview Questions You'll Get:**
- "Why not use a thread pool for symbols?" → Scheduling jitter, cache thrashing
- "How do you prevent false sharing?" → Cache line alignment, padding
- "What's the cost of a mutex?" → 20-50 ns + contention + jitter
- "Why symbol-per-core instead of work-stealing?" → Predictability, isolation
- "Explain your memory layout strategy" → NUMA-local, cache-aligned, hot/cold separation

---

## 💾 Memory Management

### Pre-Allocation Strategy
- Allocate all structures at startup
- Order books: 160 KB × 100 symbols = 16 MB
- Message pools: 100 MB
- Total budget: < 500 MB

### Object Pooling
- Message objects: Reuse after processing
- Order structures: Reuse after fill/cancel
- Buffers: Circular buffer for packets

### Buffer Sizing
- Gap buffer: 1000 messages × 100 bytes = 100 KB
- Replay buffer: 10,000 messages × 50 bytes = 500 KB
- Packet ring: 4096 slots × 1500 bytes = 6 MB

**Rationale:** Fixed memory footprint prevents fragmentation and ensures predictable performance.

---

## 🧪 Testing Philosophy

### Unit Tests
- Each component independently
- Mock dependencies
- Verify correctness first
- Framework: Google Test

### Integration Tests
- Component interactions
- ITCH → Book → Strategy → OUCH
- Verify data flows correctly
- Real protocol messages

### Performance Tests
- Latency measurements (rdtsc for nanosecond precision)
- Throughput benchmarks
- Memory profiling
- Framework: Google Benchmark

### Stress Tests
- High message rates (1M+/sec)
- Long runs (hours)
- Verify stability and consistency
- No memory leaks, no crashes

### Chaos Testing
- Random packet drops (0.1%)
- Random order rejections (1%)
- Network disconnections
- Verify graceful recovery

---

## 🔧 Production Hardening

### Network Resilience
- Exponential backoff for reconnects
- Duplicate message detection
- Sequence gap recovery
- Multicast rejoin logic

### Error Recovery
- Parse errors: Skip and continue
- Book inconsistency: Alert and rebuild
- Risk violations: Log and reject
- Network failures: Reconnect and replay

### Monitoring
- Latency percentiles (p50, p95, p99)
- Throughput rates (msgs/sec)
- Error counts by type
- Gap recovery metrics
- Position tracking

### Security
- SSL/TLS for order sessions
- Credential encryption
- Rate limiting by client
- Audit logging (immutable)

---

## 📊 Success Criteria

### Functional Requirements
- ✅ Parse all ITCH 5.0 message types correctly
- ✅ Maintain accurate Level 2 order book
- ✅ Generate spec-compliant OUCH 4.2 messages
- ✅ Handle sequence gaps and retransmits
- ✅ Enforce all risk checks

### Performance Requirements
- ✅ Internal latency < 10 μs (p99)
- ✅ Process 100K+ messages/second sustained
- ✅ Zero packet drops at target rate
- ✅ Stable over 8-hour trading session

### Quality Requirements
- ✅ No undefined behavior
- ✅ No memory leaks
- ✅ Clean separation of concerns
- ✅ > 80% test coverage

### Educational Value
- ✅ Demonstrates real HFT protocols
- ✅ Shows low-latency techniques
- ✅ Exhibits production practices
- ✅ Portfolio-ready implementation

---

## 🚀 Implementation Roadmap

### Phase 1: Foundation (Week 1)
- Common types and utilities
- ITCH parser with test data
- Dense order book implementation
- Unit tests for core components

### Phase 2: Market Data (Week 2)
- MoldUDP64 handler (gaps, retransmits)
- UDP feed handler (file replay first)
- All ITCH message types
- Integration: File → Parse → Book

### Phase 3: Order Entry (Week 3)
- SoupBinTCP session client
- OUCH message builder
- Risk manager with token bucket
- Integration: Strategy → Risk → OUCH → Sim

### Phase 4: Full System (Week 4)
- Market maker strategy
- Exchange simulator with matching
- End-to-end demo
- Performance benchmarking

### Phase 5: Enhancements (Optional)
- Multi-symbol support
- Backtesting framework
- Chaos testing
- ML strategy integration
- Production hardening

---

## 🎓 Learning Outcomes

### Protocol Engineering
- Binary protocol design and parsing
- Network byte order (endianness)
- Session management and sequencing
- Gap detection and recovery

### Performance Engineering
- Sub-microsecond latency techniques
- Lock-free data structures
- Cache optimization
- SIMD vectorization

### Financial Markets
- Order book mechanics (L2/L3)
- Price-time priority matching
- Market making strategies
- Risk management practices

### Systems Architecture
- Event-driven design
- Component decoupling
- Configuration management
- Testing strategies

---

## 📚 When to Use This Architecture

### Ideal For
- Learning HFT systems end-to-end
- Market data analysis and research
- Strategy backtesting and simulation
- Technical interviews and portfolios
- Understanding low-latency programming

### Not Suitable For
- Real money trading (missing safeguards)
- Multiple exchanges simultaneously
- Sub-microsecond latency requirements
- Distributed multi-datacenter systems

### Path to Production
1. Add comprehensive monitoring and alerting
2. Implement redundancy and failover
3. Add compliance and regulatory reporting
4. Connect to real exchange (certified connection)
5. Extensive load testing and stress testing
6. 24/7 operations and on-call procedures
7. Regulatory approval and oversight

---

## 📖 Key References

### Protocol Specifications
- NASDAQ ITCH 5.0 Specification
- NASDAQ OUCH 4.2 Specification
- MoldUDP64 Documentation
- SoupBinTCP Specification

---

## ⚠️ Important Notes

### Latency Clarification
- **Internal processing:** < 10 μs (our code, achievable)
- **Full round-trip:** 70-200 μs (includes network + exchange)
- Sub-10 μs end-to-end requires specialized hardware (FPGA, kernel bypass)

### Protocol Versioning
- ITCH 5.0 is current (as of 2025)
- OUCH 4.2 is current
- Protocols evolve; parser should support version detection
- Check exchange documentation for updates

### Hardware Considerations
- Development: Any modern CPU (3+ GHz)
- Production: Tuned system (isolated CPUs, huge pages, kernel bypass)
- Co-location: Exchange data center for best latency
- FPGA: For sub-microsecond requirements

---

**This architecture provides a comprehensive foundation for understanding and implementing real-world low-latency trading systems using industry-standard protocols.**

---

**Version 1.0 FINAL - October 2025**  
**Expert Reviewed - Production-Quality Design - Educational Focus**