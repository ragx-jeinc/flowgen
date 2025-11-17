# FlowGen - High-Performance Network Flow Generator

FlowGen is a high-performance library for generating realistic network flow records. It features a C++ core for maximum performance with Python bindings for ease of use, making it suitable for both rapid prototyping and production network testing tools.

## Features

- **High Performance**: C++ core capable of generating millions of flows per second
- **Dual API**: Use from C++ applications or Python scripts
- **Bandwidth-based Generation**: Specify target bandwidth (up to 40+ Gbps) with realistic timestamps
- **Flexible Traffic Patterns**: Web, DNS, SSH, Database, SMTP, FTP, and custom patterns
- **Iterator Pattern**: Memory-efficient one-at-a-time flow generation
- **Config-Driven**: YAML/JSON configuration for reproducible test scenarios
- **Multiple Export Formats**: CSV, JSON, JSON Lines

## Architecture

```
┌─────────────────────────────────────────┐
│         Python Application              │
│                                         │
│  ┌───────────────────────────────────┐ │
│  │   Python API (flowgen module)     │ │
│  │  - Config parsing (YAML/JSON)     │ │
│  │  - Export functions (CSV/JSON)    │ │
│  └───────────────┬───────────────────┘ │
│                  │ pybind11             │
│  ┌───────────────▼───────────────────┐ │
│  │   C++ Core (libflowgen.so)       │ │
│  │  - FlowGenerator                  │ │
│  │  - Pattern generators             │ │
│  │  - High-performance generation    │ │
│  └───────────────────────────────────┘ │
└─────────────────────────────────────────┘

        OR

┌─────────────────────────────────────────┐
│         C++ Application                 │
│                                         │
│  #include <flowgen/generator.hpp>      │
│                                         │
│  Direct C++ API access                  │
│  - Maximum performance                  │
│  - No Python overhead                   │
└─────────────────────────────────────────┘
```

## Installation

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+
- Python 3.8+ (for Python bindings)

### Building from Source

```bash
# Clone or navigate to the repository
cd flowgen

# Install Python dependencies
pip install -r requirements.txt

# Build and install (includes C++ library and Python bindings)
pip install -e .
```

### C++ Only Build

```bash
mkdir build && cd build
cmake .. -DBUILD_PYTHON_BINDINGS=OFF
cmake --build . -j
sudo cmake --install .
```

## Quick Start

### Python Usage

```python
from flowgen import FlowGenerator, export_csv

# Create and initialize generator
generator = FlowGenerator()
generator.initialize("configs/example_config.yaml")

# Generate flows using iterator pattern
flows = []
for flow in generator:
    flows.append(flow)
    # Process flow in real-time if needed

# Export to CSV
export_csv(flows, "output.csv")

# Print statistics
stats = generator.get_stats()
print(f"Generated {stats.flows_generated:,} flows")
print(f"Rate: {stats.flows_per_second:,.0f} flows/sec")
```

### C++ Usage

```cpp
#include <flowgen/generator.hpp>
#include <iostream>

int main() {
    flowgen::GeneratorConfig config;

    // Configure generator
    config.bandwidth_gbps = 10.0;
    config.max_flows = 100000;
    config.source_subnets = {"192.168.1.0/24"};
    config.destination_subnets = {"10.0.0.0/8"};
    config.traffic_patterns = {
        {"web_traffic", 60.0},
        {"dns_traffic", 40.0}
    };

    // Initialize and generate
    flowgen::FlowGenerator generator;
    generator.initialize(config);

    flowgen::FlowRecord flow;
    while (generator.next(flow)) {
        // Process flow
        std::cout << flow.to_csv() << "\n";
    }

    return 0;
}
```

### Compile C++ Application

```bash
g++ -std=c++17 my_app.cpp -lflowgen -o my_app
```

## Configuration

### Example Config (YAML)

```yaml
generation:
  max_flows: 1000000

  rate:
    bandwidth_gbps: 10.0      # Target 10 Gbps bandwidth

  start_timestamp: 1704067200 # Unix epoch (optional)

traffic_patterns:
  - type: web_traffic
    percentage: 40
  - type: dns_traffic
    percentage: 20
  - type: database_traffic
    percentage: 15
  - type: ssh_traffic
    percentage: 10
  - type: random
    percentage: 15

network:
  source_subnets:
    - 192.168.1.0/24
    - 192.168.2.0/24
  source_weights: [70, 30]

  destination_subnets:
    - 10.0.0.0/8

packets:
  min_size: 64
  max_size: 1500
  average_size: 800
```

## Performance

The C++ core is optimized for high-throughput generation:

| Bandwidth | Avg Packet Size | Flows/Second | Performance |
|-----------|----------------|--------------|-------------|
| 1 Gbps    | 800 bytes      | 156K/sec     | ✓ Easy      |
| 10 Gbps   | 800 bytes      | 1.56M/sec    | ✓ Good      |
| 40 Gbps   | 800 bytes      | 6.25M/sec    | ✓ Achievable|

**Note**: Generation is fast-forward (no real-time delays). Timestamps are calculated based on bandwidth config, but actual generation is much faster.

Example: Generating 1M flows might take 10 seconds, but timestamps span only 0.16 seconds (for 40 Gbps bandwidth).

## Supported Traffic Patterns

- **web_traffic**: HTTP/HTTPS traffic (ports 80, 443)
- **dns_traffic**: DNS queries (port 53, UDP)
- **ssh_traffic**: SSH sessions (port 22, TCP)
- **database_traffic**: MySQL, PostgreSQL, MongoDB, Redis
- **smtp_traffic**: Email traffic (ports 25, 587, 465)
- **ftp_traffic**: FTP data and control (ports 20, 21)
- **random**: Completely random flows

Each pattern generates realistic:
- Port numbers
- Packet sizes
- Protocol types (TCP/UDP)

## Export Formats

### CSV
```csv
timestamp,src_ip,dst_ip,src_port,dst_port,protocol,length
1704067200.000000,192.168.1.45,10.5.23.100,54321,443,6,1200
```

### JSON
```json
{
  "src_ip": "192.168.1.45",
  "dst_ip": "10.5.23.100",
  "src_port": 54321,
  "dst_port": 443,
  "protocol": 6,
  "timestamp": 1704067200.0,
  "length": 1200
}
```

### JSON Lines (streaming)
```
{"src_ip":"192.168.1.45",...}
{"src_ip":"192.168.1.46",...}
```

## Examples

See `examples/` directory:
- `basic_usage.py` - Simple Python example
- `streaming_usage.py` - Memory-efficient streaming
- `cpp/basic_usage.cpp` - C++ usage example

Run examples:
```bash
# Python
python examples/basic_usage.py

# C++ (after building)
./build/examples/basic_usage
```

## API Reference

### Python API

#### `FlowGenerator`
- `initialize(config_path: str) -> bool`: Initialize from config file
- `__iter__()`: Returns iterator
- `__next__() -> FlowRecord`: Generate next flow
- `get_stats() -> Stats`: Get generation statistics
- `reset()`: Reset to initial state

#### Export Functions
- `export_csv(flows, filename, include_header=True)`
- `export_json(flows, filename, pretty=True)`
- `export_jsonlines(flows, filename)`

### C++ API

#### `flowgen::FlowGenerator`
- `bool initialize(const GeneratorConfig& config)`
- `bool next(FlowRecord& flow)`
- `bool is_done() const`
- `Stats get_stats() const`
- `void reset()`

#### `flowgen::FlowRecord`
- 5-tuple fields: `source_ip`, `destination_ip`, `source_port`, `destination_port`, `protocol`
- Metadata: `timestamp`, `packet_length`
- Methods: `to_csv()`, `csv_header()`

## Development

### Building Tests

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON
cmake --build .
ctest
```

### Project Structure

```
flowgen/
├── cpp/                    # C++ source code
│   ├── include/flowgen/   # Public headers
│   ├── src/               # Implementation
│   └── bindings/          # Python bindings (pybind11)
├── flowgen/               # Python package
│   ├── config.py          # Config parser
│   ├── exporters.py       # Export functions
│   └── __init__.py        # Python API
├── examples/              # Usage examples
├── configs/               # Example configurations
├── CMakeLists.txt         # Build system
└── setup.py               # Python packaging
```

## Design Decisions

### Hybrid Architecture
- **C++ Core**: Maximum performance for flow generation loop
- **Python Layer**: Config parsing, export, orchestration
- **Best of Both**: Performance where it matters, flexibility where it helps

### Fast-Forward Generation
- No real-time delays (no `sleep()`)
- Timestamps calculated based on bandwidth
- Generate test data as fast as possible
- Realistic timing relationships preserved

### Iterator Pattern
- Memory-efficient one-at-a-time generation
- Process flows without storing all in memory
- Natural C++ and Python idioms

## Known Limitations

### UNIMPLEMENTED_SCENARIOS

1. **State Persistence**: No checkpoint/resume capability yet
2. **Bidirectional Flows**: Only one-way flows (no request-response pairs)
3. **NetFlow Export**: Only CSV/JSON export (no binary NetFlow/IPFIX)
4. **Protocol-Specific Sizes**: Simple packet size distributions (no per-protocol statistical models)

## License

[Specify your license here]

## Contributing

[Contribution guidelines]

## Authors

FlowGen Team
