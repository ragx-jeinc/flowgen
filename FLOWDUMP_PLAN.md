# FlowDump - Multi-threaded Flow Generator Plan

## Overview

**flowdump** is a high-performance CLI application that generates network flows using multiple concurrent threads and aggregates them into a single ordered output stream.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Main Thread                              │
│  - Parse CLI arguments                                           │
│  - Initialize configuration                                      │
│  - Launch generator threads                                      │
│  - Launch collector thread                                       │
│  - Wait for completion                                          │
└─────────────────────────────────────────────────────────────────┘
                             │
         ┌───────────────────┴───────────────────┐
         ▼                                       ▼
┌──────────────────────┐              ┌──────────────────────┐
│  Generator Threads   │              │  Collector Thread    │
│  (10 concurrent)     │              │                      │
│                      │              │  - Receives flows    │
│  Thread 1 (ID: 0x01) │─────────────▶│  - Timestamp chunk   │
│  Thread 2 (ID: 0x02) │─────────────▶│  - Sort within chunk │
│  Thread 3 (ID: 0x03) │─────────────▶│  - Format output     │
│  ...                 │─────────────▶│  - Write to stdout   │
│  Thread 10 (ID: 0x0A)│─────────────▶│                      │
│                      │              │                      │
│  Each generates:     │              │  Outputs:            │
│  - FlowRecords       │              │  - Merged stream     │
│  - Unique stream_id  │              │  - Sorted by time    │
└──────────────────────┘              └──────────────────────┘
```

## Data Structures

### 1. Enhanced FlowRecord

Extend the existing FlowRecord with aggregation fields:

```cpp
struct EnhancedFlowRecord {
    uint32_t stream_id;        // Generator thread ID
    uint64_t timestamp;        // Nanoseconds since epoch
    uint32_t source_ip;        // IPv4 as uint32_t
    uint32_t destination_ip;   // IPv4 as uint32_t
    uint16_t source_port;
    uint16_t destination_port;
    uint8_t  protocol;
    uint32_t packet_count;     // Number of packets in flow
    uint64_t byte_count;       // Total bytes in flow

    // Conversion methods
    std::string to_plain_text() const;
    std::string to_csv() const;
    std::string to_json() const;
    static std::string csv_header();
};
```

### 2. Thread-Safe Flow Queue

```cpp
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool done_;

public:
    void push(T item);
    bool try_pop(T& item, std::chrono::milliseconds timeout);
    void set_done();
    bool is_done() const;
};
```

### 3. Timestamp Chunker

```cpp
class TimestampChunker {
private:
    uint64_t chunk_duration_ns_;  // e.g., 1ms = 1,000,000 ns
    std::map<uint64_t, std::vector<EnhancedFlowRecord>> chunks_;
    uint64_t oldest_chunk_;

public:
    TimestampChunker(uint64_t chunk_duration_ns);
    void add_flow(const EnhancedFlowRecord& flow);
    bool has_complete_chunk() const;
    std::vector<EnhancedFlowRecord> get_complete_chunk();
};
```

## Components

### 1. Generator Thread Worker

```cpp
class GeneratorWorker {
private:
    uint32_t stream_id_;
    GeneratorConfig config_;
    ThreadSafeQueue<EnhancedFlowRecord>& output_queue_;
    uint64_t flows_to_generate_;

public:
    GeneratorWorker(uint32_t stream_id,
                    const GeneratorConfig& config,
                    ThreadSafeQueue<EnhancedFlowRecord>& queue,
                    uint64_t flows_per_thread);

    void run();

private:
    EnhancedFlowRecord create_enhanced_flow(const FlowRecord& basic_flow);
};
```

**Implementation**:
- Each worker creates its own FlowGenerator instance
- Generates flows using the existing FlowGen API
- Enhances each flow with stream_id, packet_count, byte_count
- Pushes enhanced flows to shared queue
- Terminates when flow quota reached

### 2. Collector Thread

```cpp
class FlowCollector {
private:
    ThreadSafeQueue<EnhancedFlowRecord>& input_queue_;
    TimestampChunker chunker_;
    OutputFormat format_;
    SortField sort_field_;
    std::ostream& output_;
    size_t num_generators_;
    std::atomic<size_t> generators_done_;

public:
    FlowCollector(ThreadSafeQueue<EnhancedFlowRecord>& queue,
                  uint64_t chunk_duration_ns,
                  OutputFormat format,
                  SortField sort_field,
                  std::ostream& output,
                  size_t num_generators);

    void run();
    void generator_done();

private:
    void process_complete_chunks();
    void sort_flows(std::vector<EnhancedFlowRecord>& flows);
    void output_flows(const std::vector<EnhancedFlowRecord>& flows);
};
```

**Implementation**:
- Continuously reads from input queue
- Adds flows to timestamp chunker
- When chunk is complete (all data within time window received):
  - Sort flows within chunk by specified field
  - Output formatted flows
- Continues until all generators done and queue empty

### 3. Output Formatters

```cpp
enum class OutputFormat {
    PLAIN_TEXT,
    CSV,
    JSON
};

enum class SortField {
    TIMESTAMP,
    STREAM_ID,
    SOURCE_IP,
    DESTINATION_IP,
    BYTE_COUNT,
    PACKET_COUNT
};

class FlowFormatter {
public:
    static std::string format(const EnhancedFlowRecord& flow,
                             OutputFormat format);
    static std::string header(OutputFormat format);
    static std::string footer(OutputFormat format);
};
```

## CLI Interface

### Command Line Options

```bash
flowdump [OPTIONS]

Options:
  -c, --config FILE          Config file path (required)
  -n, --num-threads NUM      Number of generator threads (default: 10)
  -f, --flows-per-thread NUM Number of flows per thread (default: 10000)
  -t, --total-flows NUM      Total flows to generate (overrides --flows-per-thread)
  -o, --output-format FMT    Output format: text, csv, json (default: text)
  -s, --sort-by FIELD        Sort by: timestamp, stream_id, src_ip, dst_ip,
                             bytes, packets (default: timestamp)
  -w, --time-window MS       Time window for chunking in milliseconds (default: 10)
  --no-header                Suppress header in CSV/text output
  --pretty                   Pretty-print JSON output
  -h, --help                 Show this help message

Examples:
  # Generate 100K flows using 10 threads, output as CSV
  flowdump -c config.yaml -n 10 -t 100000 -o csv

  # Generate flows sorted by byte count, JSON format
  flowdump -c config.yaml -n 5 -f 20000 -o json -s bytes --pretty

  # Plain text output sorted by stream ID
  flowdump -c config.yaml -n 10 -f 10000 -o text -s stream_id
```

### Output Format Examples

#### Plain Text Format
```
STREAM    TIMESTAMP               SRC_IP           SRC_PORT  DST_IP           DST_PORT  PROTO  PACKETS  BYTES
0x000001  1704067200.000000000    192.168.1.10     54321     10.0.0.1         443       6      15       12000
0x000001  1704067200.000000640    192.168.1.11     54322     10.0.0.2         80        6      8        6400
0x000002  1704067200.000000320    192.168.2.20     49152     10.0.1.1         53        17     2        512
0x000003  1704067200.000000480    192.168.1.30     60000     10.0.0.5         22        6      25       20000
...
```

#### CSV Format
```csv
stream_id,timestamp,src_ip,dst_ip,src_port,dst_port,protocol,packet_count,byte_count
1,1704067200000000000,192.168.1.10,10.0.0.1,54321,443,6,15,12000
1,1704067200000000640,192.168.1.11,10.0.0.2,54322,80,6,8,6400
2,1704067200000000320,192.168.2.20,10.0.1.1,49152,53,17,2,512
3,1704067200000000480,192.168.1.30,10.0.0.5,60000,22,6,25,20000
```

#### JSON Format
```json
[
  {
    "stream_id": 1,
    "timestamp": 1704067200000000000,
    "src_ip": "192.168.1.10",
    "dst_ip": "10.0.0.1",
    "src_port": 54321,
    "dst_port": 443,
    "protocol": 6,
    "packet_count": 15,
    "byte_count": 12000
  },
  {
    "stream_id": 1,
    "timestamp": 1704067200000000640,
    "src_ip": "192.168.1.11",
    "dst_ip": "10.0.0.2",
    "src_port": 54322,
    "dst_port": 80,
    "protocol": 6,
    "packet_count": 8,
    "byte_count": 6400
  }
]
```

## Implementation Details

### 1. Packet Count and Byte Count Generation

Since FlowGen generates individual flow records (not packet traces), we need to simulate realistic packet/byte counts:

```cpp
struct FlowStats {
    uint32_t packet_count;
    uint64_t byte_count;
};

FlowStats generate_flow_stats(uint32_t packet_length,
                               uint8_t protocol,
                               uint16_t dst_port) {
    FlowStats stats;

    // Simulate multiple packets in a flow
    // Different patterns based on protocol/port
    if (protocol == 6) {  // TCP
        if (dst_port == 80 || dst_port == 443) {  // HTTP/HTTPS
            // Typical web flow: 10-50 packets
            stats.packet_count = utils::Random::instance().randint(10, 50);
        } else if (dst_port == 22) {  // SSH
            // SSH sessions: longer flows
            stats.packet_count = utils::Random::instance().randint(100, 500);
        } else if (dst_port == 3306 || dst_port == 5432) {  // Database
            // Database queries: variable
            stats.packet_count = utils::Random::instance().randint(5, 100);
        } else {
            // Generic TCP
            stats.packet_count = utils::Random::instance().randint(5, 100);
        }
    } else if (protocol == 17) {  // UDP
        if (dst_port == 53) {  // DNS
            // DNS: typically 2 packets (query + response)
            stats.packet_count = 2;
        } else {
            // Generic UDP
            stats.packet_count = utils::Random::instance().randint(1, 20);
        }
    } else {
        stats.packet_count = utils::Random::instance().randint(1, 10);
    }

    // Calculate byte count based on packet count and average size
    // Add some variance
    uint32_t avg_pkt_size = packet_length;
    stats.byte_count = 0;
    for (uint32_t i = 0; i < stats.packet_count; ++i) {
        // Vary packet size ±20%
        uint32_t variance = avg_pkt_size / 5;  // 20%
        int32_t offset = utils::Random::instance().randint(-variance, variance);
        uint32_t pkt_size = avg_pkt_size + offset;
        pkt_size = std::max(64u, std::min(1500u, pkt_size));  // Clamp to valid range
        stats.byte_count += pkt_size;
    }

    return stats;
}
```

### 2. Timestamp Chunking Algorithm

```cpp
void TimestampChunker::add_flow(const EnhancedFlowRecord& flow) {
    uint64_t chunk_id = flow.timestamp / chunk_duration_ns_;
    chunks_[chunk_id].push_back(flow);

    if (oldest_chunk_ == 0) {
        oldest_chunk_ = chunk_id;
    }
}

bool TimestampChunker::has_complete_chunk() const {
    if (chunks_.empty()) return false;

    // A chunk is complete when we have data from a later chunk
    // This ensures all flows in the chunk have arrived
    uint64_t latest_chunk = chunks_.rbegin()->first;
    return (latest_chunk > oldest_chunk_);
}

std::vector<EnhancedFlowRecord> TimestampChunker::get_complete_chunk() {
    auto it = chunks_.find(oldest_chunk_);
    if (it == chunks_.end()) {
        oldest_chunk_++;
        return {};
    }

    std::vector<EnhancedFlowRecord> result = std::move(it->second);
    chunks_.erase(it);
    oldest_chunk_++;
    return result;
}
```

### 3. Sorting Implementation

```cpp
void FlowCollector::sort_flows(std::vector<EnhancedFlowRecord>& flows) {
    switch (sort_field_) {
    case SortField::TIMESTAMP:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      return a.timestamp < b.timestamp;
                  });
        break;
    case SortField::STREAM_ID:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.stream_id != b.stream_id)
                          return a.stream_id < b.stream_id;
                      return a.timestamp < b.timestamp;
                  });
        break;
    case SortField::SOURCE_IP:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.source_ip != b.source_ip)
                          return a.source_ip < b.source_ip;
                      return a.timestamp < b.timestamp;
                  });
        break;
    case SortField::BYTE_COUNT:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.byte_count != b.byte_count)
                          return a.byte_count > b.byte_count;  // Descending
                      return a.timestamp < b.timestamp;
                  });
        break;
    // ... other fields
    }
}
```

## File Structure

```
flowgen/
├── cpp/
│   ├── tools/
│   │   └── flowdump/
│   │       ├── main.cpp               # Entry point, CLI parsing
│   │       ├── enhanced_flow.hpp      # EnhancedFlowRecord definition
│   │       ├── enhanced_flow.cpp      # Implementation
│   │       ├── thread_safe_queue.hpp  # Thread-safe queue
│   │       ├── timestamp_chunker.hpp  # Timestamp chunking
│   │       ├── timestamp_chunker.cpp  # Implementation
│   │       ├── generator_worker.hpp   # Generator thread worker
│   │       ├── generator_worker.cpp   # Implementation
│   │       ├── flow_collector.hpp     # Collector thread
│   │       ├── flow_collector.cpp     # Implementation
│   │       ├── flow_formatter.hpp     # Output formatting
│   │       ├── flow_formatter.cpp     # Implementation
│   │       └── CMakeLists.txt         # Build configuration
│   └── CMakeLists.txt                 # Update to include tools
└── examples/
    └── flowdump_example.sh            # Example usage script
```

## Build Integration

Update `cpp/CMakeLists.txt`:

```cmake
# ... existing configuration ...

# Tools
option(BUILD_TOOLS "Build tools (flowdump, etc.)" ON)

if(BUILD_TOOLS)
    add_subdirectory(tools/flowdump)
endif()
```

`cpp/tools/flowdump/CMakeLists.txt`:

```cmake
# FlowDump tool
add_executable(flowdump
    main.cpp
    enhanced_flow.cpp
    timestamp_chunker.cpp
    generator_worker.cpp
    flow_collector.cpp
    flow_formatter.cpp
)

target_link_libraries(flowdump
    PRIVATE
        flowgen
        Threads::Threads
)

target_include_directories(flowdump
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/cpp/include
)

target_compile_features(flowdump PRIVATE cxx_std_17)

# Install
install(TARGETS flowdump
        RUNTIME DESTINATION bin)
```

## Testing Strategy

### Unit Tests

1. **ThreadSafeQueue Test**
   - Concurrent push/pop
   - Blocking behavior
   - Done flag handling

2. **TimestampChunker Test**
   - Chunk boundary detection
   - Out-of-order flow handling
   - Chunk retrieval

3. **FlowStats Generation Test**
   - Realistic packet counts per protocol
   - Byte count consistency
   - Protocol-specific patterns

### Integration Tests

1. **Single Thread Test**
   - Verify basic flow generation
   - Check enhanced fields populated
   - Validate output formats

2. **Multi-Thread Test**
   - Run with 2, 5, 10 threads
   - Verify all flows accounted for
   - Check for data races

3. **Sorting Test**
   - Test each sort field
   - Verify stable sorting
   - Check secondary sort (timestamp)

4. **Format Test**
   - Plain text parsing
   - CSV validity
   - JSON validity

### Performance Tests

1. **Throughput Test**
   - Measure flows/second with different thread counts
   - Compare: 1, 2, 5, 10, 20 threads
   - Measure overhead of collection/formatting

2. **Scalability Test**
   - Generate 1M, 10M, 100M flows
   - Monitor memory usage
   - Check for bottlenecks

3. **Latency Test**
   - Measure time from generation to output
   - Test with different chunk sizes
   - Analyze queueing delays

## Performance Considerations

### 1. Lock Contention
- Use per-thread buffers, flush periodically to shared queue
- Reduce lock granularity in queue operations
- Consider lock-free queue for higher performance

### 2. Memory Usage
- Limit queue size (backpressure)
- Use move semantics for flow records
- Configurable chunk buffer size

### 3. Sorting Overhead
- Sort only within chunks (already time-bounded)
- Use stable sort when secondary key needed
- Consider parallel sort for large chunks

### 4. Output Buffering
- Buffer output before writing to stdout
- Use bulk writes instead of per-flow writes
- Consider separate I/O thread if needed

## Usage Examples

### Example 1: Basic Usage
```bash
# Generate 100K flows using 10 threads, CSV output
flowdump -c configs/example_config.yaml -n 10 -t 100000 -o csv > flows.csv
```

### Example 2: High-Throughput Scenario
```bash
# Generate 10M flows with 20 threads, minimal overhead
flowdump -c configs/example_config.yaml \
         -n 20 \
         -t 10000000 \
         -o csv \
         --no-header > flows_10m.csv
```

### Example 3: Analysis-Friendly Output
```bash
# Generate flows sorted by byte count for top talkers analysis
flowdump -c configs/bidirectional_config.yaml \
         -n 10 \
         -f 50000 \
         -o json \
         -s bytes \
         --pretty > top_talkers.json
```

### Example 4: Stream Analysis
```bash
# Output sorted by stream ID to analyze per-thread behavior
flowdump -c configs/example_config.yaml \
         -n 5 \
         -f 10000 \
         -o text \
         -s stream_id
```

### Example 5: Pipeline Usage
```bash
# Generate flows and pipe to analysis tool
flowdump -c config.yaml -n 10 -t 100000 -o csv --no-header | \
    awk -F, '{bytes[$3]+=$9} END {for(ip in bytes) print ip, bytes[ip]}' | \
    sort -k2 -rn | \
    head -20
```

## Future Enhancements

### Phase 2 Enhancements
1. **Output to file** (in addition to stdout)
2. **Progress reporting** (e.g., every 10K flows)
3. **Statistics summary** at the end
4. **Configurable queue sizes** and buffer limits
5. **PCAP output format** (in addition to text/CSV/JSON)

### Phase 3 Enhancements
1. **Flow aggregation** (combine multiple flows to same 5-tuple)
2. **Sampling** (output only 1 in N flows)
3. **Filtering** (e.g., only TCP flows, only specific ports)
4. **Real-time output** mode (minimize latency)
5. **Distributed generation** (coordinate across multiple machines)

## Implementation Checklist

### Phase 1: Core Implementation
- [ ] Create directory structure
- [ ] Implement EnhancedFlowRecord
- [ ] Implement ThreadSafeQueue
- [ ] Implement TimestampChunker
- [ ] Implement GeneratorWorker
- [ ] Implement FlowCollector
- [ ] Implement FlowFormatter (text, CSV, JSON)
- [ ] Implement CLI argument parsing
- [ ] Implement main() orchestration
- [ ] Add CMake build configuration
- [ ] Write unit tests
- [ ] Write integration tests

### Phase 2: Testing & Validation
- [ ] Test single-threaded operation
- [ ] Test multi-threaded operation (2, 5, 10, 20 threads)
- [ ] Verify output correctness (all flows present, sorted correctly)
- [ ] Test all output formats
- [ ] Test all sort options
- [ ] Performance benchmarking
- [ ] Memory profiling

### Phase 3: Documentation & Polish
- [ ] Update README with flowdump usage
- [ ] Create man page
- [ ] Add example scripts
- [ ] Performance tuning
- [ ] Error handling improvements
- [ ] Add progress indicators

## Success Criteria

1. **Correctness**
   - ✓ All generated flows appear in output
   - ✓ Flows sorted correctly by chosen field
   - ✓ Stream IDs unique and consistent
   - ✓ Timestamps monotonically increasing within chunks
   - ✓ No data races or corruption

2. **Performance**
   - ✓ Generate at least 500K flows/second (all threads combined)
   - ✓ Scale linearly up to 10 threads
   - ✓ Memory usage < 1GB for 10M flows
   - ✓ Latency < 100ms from generation to output

3. **Usability**
   - ✓ Clear CLI interface
   - ✓ Intuitive output formats
   - ✓ Good error messages
   - ✓ Comprehensive documentation

---

**Estimated Implementation Time**: 2-3 days for Phase 1 core implementation
