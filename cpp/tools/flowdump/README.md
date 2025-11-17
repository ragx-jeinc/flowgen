# FlowDump - Multi-threaded Network Flow Generator

## Quick Start

### Build
```bash
cd flowgen
mkdir build && cd build
cmake .. -DBUILD_TOOLS=ON
cmake --build . -j
```

### Basic Usage
```bash
# Show help and all options
./flowdump --help

# Generate 10,000 flows using 5 threads, CSV output
./flowdump -c config.yaml -n 5 -f 10000 -o csv > flows.csv

# Generate 100K flows sorted by byte count (top talkers)
./flowdump -c config.yaml -n 10 -t 100000 -o text -s bytes

# JSON output with pretty printing
./flowdump -c config.yaml -n 10 -f 5000 -o json --pretty > flows.json
```

## Command Line Options

```
-c, --config FILE             Config file path (required*)
-n, --num-threads NUM         Number of generator threads (default: 10)
-f, --flows-per-thread NUM    Flows per thread (default: 10000)
-t, --total-flows NUM         Total flows (overrides --flows-per-thread)
-o, --output-format FMT       text|csv|json (default: text)
-s, --sort-by FIELD           timestamp|stream_id|src_ip|dst_ip|bytes|packets
-w, --time-window MS          Chunking window in ms (default: 10)
--start-timestamp NS          Start timestamp in nanoseconds (default: 1704067200000000000)
--end-timestamp NS            End timestamp in nanoseconds (0=auto-calculate)
--no-header                   Suppress header
--pretty                      Pretty-print JSON
-h, --help                    Show help
```

*Config file parameter is accepted but not yet used (defaults applied)

### Timestamp Options

**--start-timestamp**: Sets the starting timestamp for generated flows in nanoseconds since Unix epoch.
- Default: 1704067200000000000 (2024-01-01 00:00:00 UTC)
- Example: `--start-timestamp 1704067200000000000`

**--end-timestamp**: Sets the ending timestamp for generated flows.
- Default: 0 (auto-calculated based on flow count and bandwidth)
- When set to non-zero, the number of flows is calculated to fit within the time range
- Overrides `-f` and `-t` options when specified
- Example: `--end-timestamp 1704067260000000000` (60 seconds after default start)

## Output Formats

### Text (Human-Readable)
```
STREAM    FIRST_TIMESTAMP       LAST_TIMESTAMP        SRC_IP            SRC_PORT  DST_IP            DST_PORT  PROTO  PACKETS   BYTES
0x00000001    1704067200.000000000  1704067200.987654321  192.168.1.77      50847     172.21.205.3      443       6      20        28324
0x00000002    1704067200.000000000  1704067204.567890123  192.168.2.207     52790     172.25.169.215    5432      6      89        103322
```

**Note**: FIRST_TIMESTAMP is when the first packet of the flow arrived, LAST_TIMESTAMP is when the last packet arrived. The duration represents realistic inter-packet timing based on protocol and traffic pattern.

### CSV (Analysis Tools)
```csv
stream_id,first_timestamp,last_timestamp,src_ip,dst_ip,src_port,dst_port,protocol,packet_count,byte_count
1,1704067200000000000,1704067200987654321,192.168.1.91,10.188.229.245,57038,27017,6,64,51939
2,1704067200000000000,1704067204567890123,192.168.2.180,172.16.238.36,55509,6379,6,99,95785
```

### JSON (Programmatic)
```json
[
  {
    "stream_id": 1,
    "first_timestamp": 1704067200000000000,
    "last_timestamp": 1704067200987654321,
    "src_ip": "192.168.1.91",
    "dst_ip": "10.188.229.245",
    "packet_count": 64,
    "byte_count": 51939
  }
]
```

## Sort Options

- **timestamp** (default) - Chronological order
- **stream_id** - Group by thread
- **src_ip** / **dst_ip** - IP address order
- **bytes** - Descending by byte count (heaviest first)
- **packets** - Descending by packet count

## Performance

- **Throughput**: 190K+ flows/second (10 threads)
- **Scalability**: Linear up to ~10 threads
- **Memory**: Low overhead, move semantics used

## Examples

### Example 1: Top Talkers
```bash
# Find flows with highest byte count
flowdump -c config.yaml -n 10 -t 100000 -o csv -s bytes --no-header | head -20
```

### Example 2: Per-Thread Analysis
```bash
# See flows grouped by thread
flowdump -c config.yaml -n 5 -f 10000 -o text -s stream_id
```

### Example 3: Pipeline Processing
```bash
# Generate flows and analyze with awk
flowdump -c config.yaml -n 10 -t 50000 -o csv --no-header | \
    awk -F, '{bytes[$3]+=$9} END {for(ip in bytes) print ip, bytes[ip]}' | \
    sort -k2 -rn | head -10
```

### Example 4: High-Throughput Generation
```bash
# Generate 1M flows as fast as possible
time flowdump -c config.yaml -n 20 -t 1000000 -o csv --no-header > flows_1m.csv
```

### Example 5: Timestamp Range Generation
```bash
# Generate flows for a specific 60-second window
# Start: 2024-01-01 00:00:00 UTC (1704067200000000000)
# End:   2024-01-01 00:01:00 UTC (1704067260000000000)
flowdump -c config.yaml -n 10 \
  --start-timestamp 1704067200000000000 \
  --end-timestamp 1704067260000000000 \
  -o csv > flows_60s.csv

# Generate flows starting at a custom timestamp
# Number of flows determined by -t option
flowdump -c config.yaml -n 10 -t 50000 \
  --start-timestamp 1735689600000000000 \
  -o json --pretty > flows_custom_time.json
```

## Features

✅ Multi-threaded generation (configurable)
✅ Unique stream ID per thread
✅ Realistic packet/byte counts per flow
✅ Timestamp-based ordering across threads
✅ Multiple output formats (text, CSV, JSON)
✅ Flexible sorting (6 options)
✅ High performance (190K+ flows/sec)

## Architecture

Each thread independently generates flows with:
- Unique 32-bit stream ID
- Realistic packet counts (protocol-aware)
- Aggregated byte counts
- Nanosecond timestamps

A collector thread:
- Pulls from thread-safe queue
- Groups flows by time windows
- Sorts within windows
- Outputs ordered results

## Use Cases

1. **Performance Testing** - Load test network monitoring tools
2. **Dataset Generation** - Create realistic network traffic datasets
3. **Benchmarking** - Test flow processing pipelines
4. **Top Talkers** - Identify high-volume endpoints
5. **Multi-Stream Analysis** - Simulate multiple data sources

## Limitations

- Config file parsing not yet implemented (uses defaults)
- Single collector thread (can be bottleneck at extreme scale)
- No flow aggregation (each record is distinct)

## Future Enhancements

- YAML config file support
- Progress reporting during generation
- PCAP output format
- Flow aggregation by 5-tuple
- Filtering and sampling options

## See Also

- [FLOWDUMP_PLAN.md](../../../FLOWDUMP_PLAN.md) - Detailed design document
- [FLOWDUMP_IMPLEMENTATION.md](../../../FLOWDUMP_IMPLEMENTATION.md) - Implementation summary
- [FlowGen Documentation](../../../README.md) - Base library documentation
