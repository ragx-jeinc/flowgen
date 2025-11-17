# FlowStats - Design Document

**Date**: 2025-11-07
**Version**: 1.0
**Status**: Reference Design for Future Development

## Table of Contents

1. [Overview](#overview)
2. [Design Principles](#design-principles)
3. [Architecture](#architecture)
4. [Threading Model](#threading-model)
5. [Progress Tracking](#progress-tracking)
6. [Extensibility Pattern](#extensibility-pattern)
7. [Design Decisions](#design-decisions)
8. [Future Extensions](#future-extensions)

---

## Overview

**flowstats** is a modular, scalable C++17 application for processing and analyzing network flow data. It uses a subcommand-based CLI interface with a base class template pattern for easy extensibility.

### Design Goals

1. **Modularity**: Easy to add new analysis subcommands
2. **Scalability**: Handle large datasets efficiently
3. **Lock-Free**: Maximize performance via atomic operations
4. **Type Safety**: Compile-time type checking for results
5. **User-Friendly**: Real-time progress feedback

### Non-Goals

- Real-time streaming analysis (batch processing only)
- Distributed processing (single-machine only)
- GUI interface (CLI only)

---

## Design Principles

### 1. Modular & Extensible

**Pattern**: Template Method + Strategy Pattern

Each subcommand is a class inheriting from `FlowStatsCommand<ResultType>`. The base class defines the workflow, subclasses implement specific behaviors.

**Benefits**:
- Add new subcommands by creating new classes
- No changes to existing code required
- Common functionality centralized

### 2. Lock-Free Threading

**Pattern**: Per-Thread Buffers + Atomic Operations

No mutexes, no locks - only atomic operations and move semantics.

**Benefits**:
- Maximum throughput
- No contention
- Predictable performance

### 3. Type-Safe Results

**Pattern**: Template-Based Return Types

Each subcommand defines its own result type, enforced at compile time.

**Benefits**:
- Compile-time error detection
- Self-documenting API
- No runtime type checking

### 4. C++17 Compliance

**Constraint**: Must work with C++17 (no C++20 features)

**Why**: Broader compatibility with existing build systems

### 5. Consistent Naming

**Convention**: Member variables use `m_` prefix

**Example**:
```cpp
class FlowStatsCommand {
    std::string m_config_file;        // Member variable
    size_t m_num_threads;             // Member variable
    std::atomic<bool> m_done;         // Atomic member
};
```

---

## Architecture

### Class Hierarchy

```
┌──────────────────────────────────────────────────────┐
│  FlowStatsCommand<ResultType>                        │
│  (Base Class Template)                               │
│                                                       │
│  - Common workflow (execute())                       │
│  - Thread management                                 │
│  - Progress tracking                                 │
│  - Statistics aggregation                            │
│                                                       │
│  Virtual Functions:                                  │
│    • validate_options()                              │
│    • initialize()                                    │
│    • run_worker_thread(thread_id)                    │
│    • collect_results() -> ResultType                 │
│    • output_results(ResultType)                      │
│    • get_timestamp_range()                           │
└──────────────────────────────────────────────────────┘
                          △
                          │ inherits
                          │
        ┌─────────────────┴─────────────────┐
        │                                   │
┌───────┴────────┐                 ┌────────┴────────┐
│ FlowStatsCollect│                │ FlowStatsAnalyze│
│ <CollectResult> │                │ <AnalyzeResult> │
│                 │                │   (future)      │
│ - Generate flows│                │                 │
│ - Collect data  │                └─────────────────┘
│ - Sort by time  │
└─────────────────┘
```

### Component Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                             │
│  - Parse subcommand                                          │
│  - Dispatch to subcommand handler                            │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│              Subcommand (e.g., FlowStatsCollect)            │
│                                                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ FlowStatsBase│  │ProgressTracker│ │OutputFormatter│     │
│  │  (template)  │  │  (lock-free)  │  │  (strategy)  │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Worker Threads (10 concurrent)                     │    │
│  │                                                      │    │
│  │  Thread 1 ──┐                                       │    │
│  │  Thread 2 ──┼──> Per-Thread Buffers ──> Move ──>   │    │
│  │  Thread 3 ──┘     (no locks)              Results   │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

```
Input (Config + Options)
         │
         ▼
    Initialize
         │
         ├──> Start N Worker Threads
         │         │
         │         ├──> Thread 1: Generate flows → Buffer 1
         │         ├──> Thread 2: Generate flows → Buffer 2
         │         └──> Thread N: Generate flows → Buffer N
         │                  │
         │                  └──> Update Progress (lock-free)
         │
         ▼
  Collect Results
         │
         ├──> Move buffers (zero-copy)
         ├──> Merge & Sort
         └──> Aggregate Statistics
         │
         ▼
  Output Results
         │
         ├──> Format (text/csv/json)
         └──> Write to stdout
```

---

## Threading Model

### Lock-Free Design

**Core Principle**: Each thread owns its data, no sharing during write phase.

#### Per-Thread Data Structure

```cpp
struct PerThreadData {
    uint32_t m_thread_id;                  // Immutable after creation
    std::atomic<size_t> m_flows_generated; // Lock-free counter
    std::atomic<uint64_t> m_bytes_generated; // Lock-free counter
    std::atomic<bool> m_done;              // Lock-free flag
};

// Stored as unique_ptr to avoid atomic move issues
std::vector<std::unique_ptr<PerThreadData>> m_thread_data;
```

**Why `unique_ptr`?**
- `std::atomic` is neither copyable nor movable
- `std::vector::resize()` requires copyable/movable types
- `unique_ptr` is movable (pointer is copied, not the atomic)

#### Thread-Local Buffers

```cpp
struct ThreadFlowBuffer {
    std::vector<EnhancedFlowRecord> m_flows;

    ThreadFlowBuffer() {
        m_flows.reserve(10000);  // Pre-allocate
    }
};

std::vector<ThreadFlowBuffer> m_thread_buffers;  // One per thread
```

**Access Pattern**:
- Thread N writes only to `m_thread_buffers[N]`
- No locks needed (exclusive ownership)
- Main thread reads after all threads complete

#### Collection Phase (Zero-Copy)

```cpp
CollectResult collect_results() override {
    CollectResult result;

    // Wait for all threads (spin on atomic flags)
    for (auto& data_ptr : m_thread_data) {
        while (!data_ptr->m_done.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // Move buffers (zero-copy, no locks needed)
    for (auto& buffer : m_thread_buffers) {
        result.m_flows.insert(
            result.m_flows.end(),
            std::make_move_iterator(buffer.m_flows.begin()),
            std::make_move_iterator(buffer.m_flows.end())
        );
    }

    return result;
}
```

**Memory Ordering**:
- `memory_order_relaxed`: For counters (performance)
- `memory_order_acquire/release`: For synchronization (correctness)

### Thread Lifecycle

```
Main Thread                  Worker Thread 1              Worker Thread N
    │                               │                            │
    ├─ create PerThreadData ───────┼───────────────────────────►│
    │                               │                            │
    ├─ start thread ───────────────►│                            │
    │                               │                            │
    │                               ├─ generate flows            │
    │                               ├─ update atomics            │
    │                               ├─ update progress           │
    │                               │                            │
    │                               ├─ set done flag ────►       │
    │                               │  (release)                 │
    │                               │                            │
    ├─ spin on done flag ───────────┼──────(acquire)────►        │
    │  (acquire)                    │                            │
    │                               │                            │
    ├─ move buffers ────────────────┼───────────────────────────►│
    │  (zero-copy)                  │                            │
    │                               │                            │
    ├─ join threads ────────────────┼───────────────────────────►│
    │                               ▼                            ▼
```

---

## Progress Tracking

### Design Goals

1. **Real-time feedback** during long operations
2. **Timestamp-based** progression (not just flow count)
3. **Lock-free** updates from worker threads
4. **Non-blocking** display updates

### Architecture

```cpp
class ProgressTracker {
private:
    // Time range
    uint64_t m_start_timestamp_ns;
    uint64_t m_end_timestamp_ns;

    // Per-thread timestamps (lock-free)
    std::vector<std::atomic<uint64_t>> m_thread_current_timestamps;

    // Statistics (lock-free)
    std::atomic<uint64_t> m_total_flows_processed;
    std::atomic<uint64_t> m_total_bytes_processed;

    // Display thread
    std::thread m_progress_thread;
    std::atomic<bool> m_shutdown;

public:
    // Worker threads call this (lock-free)
    void update_timestamp(size_t thread_id, uint64_t current_ts);
    void add_flows(uint64_t count);
    void add_bytes(uint64_t bytes);

    // Display thread queries this
    double get_progress_percentage() const;
    uint64_t get_current_timestamp() const;
    std::chrono::seconds get_eta() const;
};
```

### Progress Calculation

**Timestamp-Based** (not flow-count based):

```cpp
double ProgressTracker::get_progress_percentage() const {
    // Find slowest thread (minimum timestamp)
    uint64_t min_ts = m_end_timestamp_ns;
    for (const auto& ts : m_thread_current_timestamps) {
        uint64_t current = ts.load(std::memory_order_relaxed);
        if (current < min_ts) {
            min_ts = current;
        }
    }

    if (min_ts >= m_end_timestamp_ns) return 100.0;

    uint64_t elapsed = min_ts - m_start_timestamp_ns;
    return (elapsed * 100.0) / m_total_duration_ns;
}
```

**Why minimum timestamp?**
- Overall progress = slowest thread
- Prevents over-reporting
- More accurate ETA

### Display Styles

**Strategy Pattern** for display:

```cpp
enum class ProgressStyle {
    BAR,      // [====>    ] 45.2% | Time: ... | ETA: ...
    SIMPLE,   // Progress: 45.2% - 150K flows - ETA: 2m
    SPINNER,  // / 45.2% - 150K flows - 45K flows/s
    NONE      // Silent mode
};
```

**Display Thread** (separate from workers):

```cpp
void ProgressTracker::progress_display_loop() {
    while (!m_shutdown.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(1s);  // Update every second

        // Query current state (lock-free reads)
        double progress = get_progress_percentage();
        auto eta = get_eta();
        double throughput = get_throughput();

        // Display to stderr (non-blocking)
        display_progress();
    }
}
```

**Why stderr?**
- Allows stdout redirection: `flowstats collect > output.csv`
- Progress visible while data goes to file

---

## Extensibility Pattern

### Adding a New Subcommand

**Step 1**: Define Result Type

```cpp
// In subcommands/analyze_command.h
struct AnalyzeResult {
    std::map<std::string, uint64_t> m_protocol_distribution;
    std::map<std::string, uint64_t> m_bandwidth_by_subnet;
    double m_average_packet_size;
    // ... custom fields
};
```

**Step 2**: Create Subcommand Class

```cpp
class FlowStatsAnalyze : public FlowStatsCommand<AnalyzeResult> {
public:
    bool validate_options() override {
        // Validate analyze-specific options
    }

    void initialize() override {
        // Initialize analysis structures
    }

    void run_worker_thread(size_t thread_id) override {
        // Per-thread analysis logic
    }

    AnalyzeResult collect_results() override {
        // Aggregate per-thread results
    }

    void output_results(const AnalyzeResult& results) override {
        // Format and output analysis
    }
};
```

**Step 3**: Add Entry Point

```cpp
// In main.cpp
int flowstats_analyze_main(int argc, char** argv) {
    AnalyzeOptions opts;

    // Parse arguments
    ArgParser parser("flowstats analyze");
    // ... add options

    if (!parser.parse(argc, argv)) {
        return 1;
    }

    // Create and execute
    FlowStatsAnalyze cmd(opts);
    return cmd.execute();
}

int main(int argc, char** argv) {
    // ... existing code ...

    if (subcommand == "analyze") {
        return flowstats_analyze_main(argc - 1, argv + 1);
    }

    // ... rest of main
}
```

**That's it!** The base class handles:
- Thread creation/management
- Progress tracking
- Statistics aggregation
- Error handling

### Template Method Pattern

**Base class defines workflow**:

```cpp
template<typename ResultType>
int FlowStatsCommand<ResultType>::execute() {
    // 1. Validate
    if (!validate_options()) return 1;

    // 2. Initialize
    initialize();

    // 3. Progress tracking
    if (m_show_progress) {
        initialize_progress_tracker();
        m_progress_tracker->start();
    }

    // 4. Start threads
    start_worker_threads();

    // 5. Collect results
    ResultType results = collect_results();

    // 6. Wait for completion
    wait_for_completion();

    // 7. Stop progress
    if (m_progress_tracker) {
        m_progress_tracker->stop();
    }

    // 8. Output
    output_results(results);

    // 9. Summary
    if (m_show_progress) {
        output_summary();
    }

    return 0;
}
```

**Subclass implements specific steps** (virtual functions).

---

## Design Decisions

### 1. Why Template Method Pattern?

**Decision**: Use base class template with virtual functions

**Alternatives Considered**:
- Free functions: Harder to share code
- Strategy pattern only: More boilerplate

**Rationale**:
- Enforces consistent workflow
- Maximizes code reuse
- Clear extension point (virtual functions)

### 2. Why Lock-Free Threading?

**Decision**: Use atomics + per-thread buffers, no mutexes

**Alternatives Considered**:
- Mutex-protected shared queue: Too much contention
- Lock-free queue: Added complexity, similar performance

**Rationale**:
- Maximum throughput (no lock contention)
- Simpler reasoning (no deadlocks possible)
- Predictable performance

### 3. Why unique_ptr for PerThreadData?

**Decision**: `std::vector<std::unique_ptr<PerThreadData>>`

**Alternatives Considered**:
- `std::vector<PerThreadData>`: Doesn't compile (atomics not movable)
- Raw pointers: Manual memory management

**Rationale**:
- Atomics are not copyable/movable
- `unique_ptr` provides RAII
- Clear ownership semantics

### 4. Why Timestamp-Based Progress?

**Decision**: Track timestamp progression, not just flow count

**Alternatives Considered**:
- Flow count percentage: Misleading for time-based generation
- Byte count: Doesn't reflect time progression

**Rationale**:
- More accurate for time-range generation
- Better ETA estimation
- Meaningful for users ("50% through time range")

### 5. Why Separate Progress Thread?

**Decision**: Dedicated thread for progress display

**Alternatives Considered**:
- Update from main thread: Blocks collection
- Update from worker threads: Contention on display

**Rationale**:
- Non-blocking display updates
- Consistent update interval (1 second)
- Clean separation of concerns

### 6. Why .h Instead of .hpp?

**Decision**: Use `.h` for all header files

**Rationale**: User requirement for consistency with existing codebase

### 7. Why m_ Prefix?

**Decision**: Member variables use `m_` prefix

**Rationale**: User requirement, improves readability

---

## Future Extensions

### New Subcommands (Examples)

#### 1. FlowStatsFilter

**Purpose**: Filter flows by criteria

**Result Type**:
```cpp
struct FilterResult {
    std::vector<EnhancedFlowRecord> m_filtered_flows;
    uint64_t m_total_examined;
    uint64_t m_total_matched;
};
```

**Options**:
- `--protocol tcp|udp|icmp`
- `--port PORT`
- `--src-subnet CIDR`
- `--dst-subnet CIDR`
- `--time-range START-END`

#### 2. FlowStatsAggregate

**Purpose**: Aggregate flows by key

**Result Type**:
```cpp
struct AggregateResult {
    std::map<AggregateKey, AggregateStats> m_aggregated;
};

struct AggregateKey {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t dst_port;
    uint8_t protocol;
};

struct AggregateStats {
    uint64_t flow_count;
    uint64_t total_packets;
    uint64_t total_bytes;
    uint64_t first_seen;
    uint64_t last_seen;
};
```

**Options**:
- `--by 5tuple|srcip|dstip|proto`
- `--window SECONDS`

#### 3. FlowStatsCompare

**Purpose**: Compare two flow datasets

**Result Type**:
```cpp
struct CompareResult {
    std::vector<EnhancedFlowRecord> m_only_in_first;
    std::vector<EnhancedFlowRecord> m_only_in_second;
    std::vector<std::pair<EnhancedFlowRecord, EnhancedFlowRecord>> m_different;
};
```

### Progress Enhancements

1. **Per-Thread Display**:
```
Thread 1: [=====>     ] 50%
Thread 2: [======>    ] 60%
Thread 3: [====>      ] 40%
Overall:  [=====>     ] 50%
```

2. **Custom Format Strings**:
```bash
flowstats collect --progress-format "{pct}% | {flows} flows | {eta}"
```

3. **Progress Hooks**:
```cpp
class ProgressTracker {
    std::function<void(double pct)> m_progress_callback;
};
```

### Output Enhancements

1. **Binary Formats**:
```cpp
class ProtobufFormatter : public OutputFormatter<CollectResult> { ... };
class MessagePackFormatter : public OutputFormatter<CollectResult> { ... };
```

2. **Compression**:
```cpp
class GzipOutputStream : public std::ostream { ... };
flowstats collect -o csv --compress gzip > flows.csv.gz
```

3. **Incremental Output** (streaming):
```cpp
// Output flows as they're ready, don't wait for all
void output_results_streaming(const ResultType& partial_results);
```

### Performance Enhancements

1. **Parallel Sorting**:
```cpp
// Use TBB or std::execution::par
std::sort(std::execution::par, flows.begin(), flows.end(), ...);
```

2. **Memory Pools**:
```cpp
// Pre-allocate flow records to reduce allocations
class FlowRecordPool {
    std::vector<EnhancedFlowRecord> m_pool;
    std::atomic<size_t> m_next_free;
};
```

3. **SIMD Optimizations**:
```cpp
// Vectorize statistics aggregation
// Use AVX2/AVX512 for bulk operations
```

---

## Key Takeaways

### For Future Developers

1. **Adding Subcommands**:
   - Inherit from `FlowStatsCommand<YourResultType>`
   - Implement 5 virtual functions
   - Add entry point to `main.cpp`
   - Done!

2. **Thread Safety**:
   - Workers write to own buffers (no sharing)
   - Use atomics for counters/flags
   - Main thread collects after threads finish
   - No mutexes needed

3. **Progress Tracking**:
   - Call `update_progress()` from workers
   - Automatic display in separate thread
   - Zero performance impact

4. **Output Formats**:
   - Create new `OutputFormatter<ResultType>`
   - Register in `create_formatter()`
   - Works automatically

### Design Patterns Used

1. **Template Method**: Base class workflow
2. **Strategy**: Output formatters
3. **RAII**: unique_ptr ownership
4. **Zero-Copy**: Move semantics
5. **Lock-Free**: Atomic operations

### Performance Characteristics

- **Scalability**: Linear with thread count (up to core count)
- **Memory**: O(total_flows) with minimal overhead
- **Latency**: Near-zero synchronization overhead
- **Throughput**: Measured at 100K+ flows/second

---

## References

### Related Documentation

- `FLOWSTATS_IMPLEMENTATION.md` - Implementation details
- `CLAUDE.md` - FlowGen project context
- `FLOWDUMP_*.md` - Similar tool design (single-threaded)

### C++ Resources

- C++17 Standard: Atomic operations
- Herb Sutter: Lock-Free Programming
- Scott Meyers: Effective Modern C++

---

**Last Updated**: 2025-11-07
**Status**: Complete Design Reference
**Next Steps**: Ready for new subcommands and enhancements
