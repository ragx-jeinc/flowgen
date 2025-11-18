# Multi-Generator Example

## Overview

This example demonstrates managing **multiple independent FlowGenerator instances**, each writing flows to separate subdirectories with automatic file rotation.

**Use Cases**:
- Simulating multiple network sensors/collectors
- Generating flows from different network segments
- Testing distributed flow processing systems
- Creating large-scale multi-source datasets

## Features

✅ **Multiple Generator Instances**: Create 1-100+ generators with unique IDs
✅ **Unique Directory per Generator**: `generator_0/`, `generator_1/`, etc.
✅ **Automatic File Rotation**: Split flows into batches (e.g., 1000 flows per file)
✅ **Different Traffic Profiles**: Each generator has varied patterns
✅ **Bidirectional Mode**: Even-numbered generators use bidirectional flows
✅ **Nanosecond Timestamps**: Full precision for high-bandwidth scenarios
✅ **Performance Metrics**: Reports generation rate and file counts

## Build

```bash
cd flowgen/build
cmake .. -DBUILD_EXAMPLES=ON
cmake --build . --target multi_generator_example -j
```

Binary location: `build/multi_generator_example`

## Usage

### Basic Usage

```bash
# Default: 12 generators, 10K flows each, 1K flows per file
LD_LIBRARY_PATH=. ./multi_generator_example

# Custom configuration
LD_LIBRARY_PATH=. ./multi_generator_example -n 5 -f 50000 -o /tmp/flowdata
```

### Command-Line Options

```
-n, --num-generators NUM      Number of generator instances (default: 12)
-f, --flows-per-generator NUM Flows per generator (default: 10000)
-b, --batch-size NUM          Flows per CSV file (default: 1000)
-o, --output-path PATH        Base output directory (default: ./output)
-w, --bandwidth GBPS          Bandwidth in Gbps (default: 10.0)
-v, --verbose                 Verbose output (progress per 10%)
-h, --help                    Show help message
```

### Examples

#### Example 1: Generate 12 generators with default settings
```bash
LD_LIBRARY_PATH=. ./multi_generator_example
```

**Output**:
```
output/
├── generator_0/
│   ├── flows_0000.csv  (1000 flows)
│   ├── flows_0001.csv  (1000 flows)
│   ├── ...
│   └── flows_0009.csv  (1000 flows)
├── generator_1/
│   └── ...
└── generator_11/
    └── ...
```

**Total**: 120,000 flows across 120 files (12 generators × 10 files each)

#### Example 2: High-volume data generation
```bash
LD_LIBRARY_PATH=. ./multi_generator_example \
  -n 20 \
  -f 100000 \
  -b 10000 \
  -o /data/network_flows
```

**Total**: 2,000,000 flows across 200 files (20 generators × 10 files each)

#### Example 3: Small test dataset
```bash
LD_LIBRARY_PATH=. ./multi_generator_example \
  -n 3 \
  -f 500 \
  -b 100 \
  -o /tmp/test_flows \
  -v
```

**Output**: 1,500 flows with verbose progress reporting

#### Example 4: Custom bandwidth scenario
```bash
LD_LIBRARY_PATH=. ./multi_generator_example \
  -n 10 \
  -f 50000 \
  -w 40.0 \
  -o /data/high_bandwidth
```

**Simulates**: 10 sources at 40 Gbps equivalent bandwidth

## Output Structure

### Directory Hierarchy

```
<output-path>/
├── generator_0/
│   ├── flows_0000.csv
│   ├── flows_0001.csv
│   ├── flows_0002.csv
│   └── ...
├── generator_1/
│   ├── flows_0000.csv
│   ├── flows_0001.csv
│   └── ...
└── generator_N/
    └── ...
```

### CSV Format

Each CSV file contains standard FlowGen flow records:

```csv
timestamp,src_ip,dst_ip,src_port,dst_port,protocol,length
1704067200000000000,192.168.1.45,10.200.5.100,54321,443,6,1200
1704067200000000640,192.168.1.46,10.200.5.101,54322,80,6,800
```

**Fields**:
- `timestamp`: Nanoseconds since Unix epoch
- `src_ip`, `dst_ip`: IPv4 addresses (dotted decimal)
- `src_port`, `dst_port`: Port numbers
- `protocol`: 6=TCP, 17=UDP, 1=ICMP
- `length`: Packet size in bytes

## Generator Configuration

Each generator instance has **unique characteristics**:

### Source Subnets (varies by generator ID)
- Generator 0: `192.168.0.0/16`
- Generator 1: `10.10.0.0/16`
- Generator 2: `172.16.0.0/12`
- Generator 3+: `10.20.0.0/16`, `10.30.0.0/16`, etc.

### Traffic Patterns (varies by generator ID % 3)

**Web-heavy** (ID % 3 == 0):
- 50% web traffic
- 20% DNS
- 15% database
- 10% SSH
- 5% random

**Database-heavy** (ID % 3 == 1):
- 40% database traffic
- 30% web
- 15% DNS
- 10% SSH
- 5% random

**Balanced** (ID % 3 == 2):
- 30% web
- 25% DNS
- 20% database
- 15% SSH
- 10% random

### Bidirectional Mode
- **Even-numbered generators** (0, 2, 4, ...): Bidirectional mode enabled (50% reversed)
- **Odd-numbered generators** (1, 3, 5, ...): Unidirectional only

### Timestamps
Each generator starts at a slightly different timestamp (+1ms per generator ID) to simulate real-world staggered startup.

## Performance

**Tested Performance** (12 generators, 10K flows each):
- **Total flows**: 120,000
- **Generation time**: ~0.5 seconds
- **Generation rate**: ~240K flows/second
- **Memory usage**: Minimal (sequential generation per instance)

**Scalability**:
- 100+ generators tested successfully
- Linear scaling with generator count
- Limited only by disk I/O for large batch sizes

## Data Analysis Examples

### Count total flows across all generators
```bash
wc -l output/generator_*/flows_*.csv
```

### View flows from a specific generator
```bash
head -20 output/generator_0/flows_0000.csv
```

### Combine all flows from all generators
```bash
# Combine with headers
head -1 output/generator_0/flows_0000.csv > all_flows.csv
tail -n +2 -q output/generator_*/flows_*.csv >> all_flows.csv

# Or without deduplicating headers
cat output/generator_*/flows_*.csv > all_flows_with_headers.csv
```

### Count flows by protocol
```bash
tail -n +2 -q output/generator_*/flows_*.csv | \
  awk -F',' '{count[$6]++} END {for(p in count) print "Protocol " p ": " count[p]}'
```

### Find top talkers (by source IP)
```bash
tail -n +2 -q output/generator_*/flows_*.csv | \
  awk -F',' '{count[$2]++} END {for(ip in count) print count[ip], ip}' | \
  sort -rn | head -20
```

### Analyze flows per generator
```bash
for dir in output/generator_*/; do
  gen_id=$(basename "$dir")
  flow_count=$(tail -n +2 -q "$dir"/*.csv | wc -l)
  echo "$gen_id: $flow_count flows"
done
```

## Implementation Details

### Generator Instance Class

Each `GeneratorInstance`:
- Creates its own output directory
- Manages its own FlowGenerator instance
- Handles file rotation when batch size reached
- Tracks flows generated and files written
- Provides progress reporting

### Sequential Generation

Generators run **sequentially** (not in parallel) in this example:
```cpp
for (auto& gen : generators) {
    gen->generate_all();  // Generate all flows for this generator
}
```

**Why sequential?**
- Simpler implementation
- Predictable resource usage
- Still achieves ~240K flows/second
- Easy to modify for parallel generation if needed

### File Rotation

Files are automatically created when:
- First flow is generated
- Batch size limit reached (e.g., 1000 flows)

File naming: `flows_<NNNN>.csv` with zero-padded 4-digit numbers.

## Extending the Example

### Parallel Generation

To generate flows in parallel across all generators:

```cpp
#include <thread>

std::vector<std::thread> threads;
for (auto& gen : generators) {
    threads.emplace_back([&gen]() {
        gen->generate_all();
    });
}

for (auto& t : threads) {
    t.join();
}
```

### Custom Configuration per Generator

Modify `create_config()` to customize each generator:

```cpp
flowgen::GeneratorConfig create_config(size_t generator_id, const MultiGenOptions& opts) {
    // Add custom logic here
    if (generator_id < 5) {
        // First 5 generators: web-heavy
    } else {
        // Remaining generators: database-heavy
    }
}
```

### Real-time Progress Reporting

Add callback for real-time progress:

```cpp
void GeneratorInstance::generate_all() {
    // ... existing code ...

    if (m_flows_generated % 1000 == 0) {
        std::cout << "[Generator " << m_id << "] "
                  << m_flows_generated << " flows generated\n";
    }
}
```

## Comparison to Other Examples

| Example | Generators | Concurrency | Output | Use Case |
|---------|-----------|-------------|--------|----------|
| **basic_usage** | 1 | Single-threaded | Single file | Simple testing |
| **multi_generator** | 1-100+ | Sequential | Multi-file/multi-dir | Multi-source simulation |
| **flowdump** | 1 (10 threads) | Multi-threaded | Single stream | High throughput |
| **flowstats** | 1 (10 threads) | Multi-threaded | Aggregated stats | Analysis |

## Troubleshooting

### Permission Denied on Output Directory
```bash
mkdir -p /tmp/flowdata
chmod 755 /tmp/flowdata
LD_LIBRARY_PATH=. ./multi_generator_example -o /tmp/flowdata
```

### libflowgen.so Not Found
```bash
# Run from build directory
cd build
LD_LIBRARY_PATH=. ./multi_generator_example

# Or install library system-wide
sudo cmake --install .
```

### Disk Space Issues
Monitor disk usage when generating large datasets:
```bash
# Check available space
df -h /tmp

# Estimate size: ~65 bytes per flow (CSV)
# 1M flows ≈ 65 MB
```

## Summary

The multi-generator example demonstrates:
- ✅ Managing multiple FlowGenerator instances
- ✅ Organizing output into separate directories
- ✅ Automatic file rotation for large datasets
- ✅ Diverse traffic profiles per generator
- ✅ Scalable to 100+ generators
- ✅ Real-world multi-source simulation

**Perfect for**: Testing distributed flow collectors, generating large-scale datasets, simulating multi-sensor networks.
