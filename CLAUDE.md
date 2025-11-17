# FlowGen - Claude AI Session Context

This document provides context for Claude AI sessions working on the FlowGen project.

## Project Overview

FlowGen is a high-performance network flow record generator with a hybrid C++/Python architecture, designed to be called primarily from C++ applications while offering Python bindings for flexibility.

**Primary Use Case**: Generate realistic network flow records at high throughput (up to 40+ Gbps equivalent) for network testing and analysis tools.

## Key Implementation Details

### Architecture Decision: Hybrid C++/Python

**Rationale**: User requirement: "Will be called primarily from c++ application"

- **C++ Core (libflowgen)**: Hot path - flow generation loop, pattern generators
- **Python Layer**: Config parsing (YAML/JSON), export functions, wrapper API
- **pybind11**: Bindings layer connecting both worlds

### Critical Feature: Nanosecond Timestamp Resolution

**Status**: ✅ **FULLY IMPLEMENTED AND TESTED** (Latest change)

**User Requirement**: "I need the timestamp in nanosec resolution."

**Implementation**:
- `FlowRecord::timestamp`: Changed from `double` → `uint64_t` (nanoseconds)
- `GeneratorConfig::start_timestamp_ns`: New field replacing `start_timestamp`
- All calculations in nanoseconds throughout C++ core
- Python layer automatically converts seconds → nanoseconds for config
- Backward compatibility: Python wrapper provides `current_timestamp` property returning seconds

**Files Modified** (for nanosecond timestamps):
- `cpp/include/flowgen/flow_record.hpp` - FlowRecord struct
- `cpp/include/flowgen/generator.hpp` - GeneratorConfig, FlowGenerator
- `cpp/include/flowgen/patterns.hpp` - PatternGenerator interface
- `cpp/src/generator.cpp` - Timestamp calculations
- `cpp/src/patterns.cpp` - All pattern implementations
- `cpp/bindings/python_bindings.cpp` - pybind11 bindings
- `flowgen/__init__.py` - Python config conversion

**Verification**: 40 Gbps = 160ns inter-arrival time, tested and confirmed accurate.

### Design Decisions (User-Specified)

1. **Fast-Forward Generation**: No real-time rate limiting, no `sleep()` calls
   - Generate flows as fast as possible
   - Timestamps are realistic (based on bandwidth) but generation is much faster
   - Example: 40 Gbps = 6.25M flows/sec equivalent timestamps, but actual generation ~10x faster

2. **Iterator Pattern**: One flow at a time, memory efficient
   - `for flow in generator:` (Python)
   - `while (generator.next(flow))` (C++)

3. **Config-Driven**: YAML/JSON configuration via `Initialize(config_path)`
   - Simple schema (no fancy validation)
   - No dynamic config reload
   - No config templating
   - Traffic patterns specified with percentage weights (must sum to 100%)

4. **Independent One-Way Flows Only**: No bidirectional conversation tracking

5. **Bandwidth-Based Rate**: Target up to 40 Gbps
   - Formula: `flows_per_second = (bandwidth_gbps * 1e9 / 8) / avg_packet_size`
   - Example: 40 Gbps with 800B packets = 6.25M flows/sec = 160ns between flows

6. **Unix Epoch Timestamps**: Configurable start timestamp in config

## File Structure

### C++ Core (`cpp/`)

**Headers (`cpp/include/flowgen/`)**:
- `flow_record.hpp` - Core data structure (5-tuple + timestamp + length)
- `generator.hpp` - FlowGenerator class, GeneratorConfig
- `patterns.hpp` - PatternGenerator interface
- `utils.hpp` - Random number generation, IP/port utilities

**Implementation (`cpp/src/`)**:
- `flow_record.cpp` - CSV serialization
- `generator.cpp` - Main generation loop, pattern selection
- `patterns.cpp` - Traffic pattern implementations (web, DNS, SSH, database, SMTP, FTP, random)
- `utils.cpp` - Utility implementations

**Bindings (`cpp/bindings/`)**:
- `python_bindings.cpp` - pybind11 bindings for Python

### Python Layer (`flowgen/`)

- `__init__.py` - Main Python API, FlowGenerator wrapper
- `config.py` - YAML/JSON config parser and validation
- `exporters.py` - CSV/JSON/JSONLines export functions
- `generator_interface.py` - Python fallback implementation (rarely used)

### Configuration (`configs/`)

- `example_config.yaml` - Example configuration file

### Build System

- `CMakeLists.txt` - CMake build configuration
- `setup.py` - Python package installer with CMake extension

## Build System Notes

### Critical: pybind11 Version

**Issue**: System pybind11 on some platforms is incompatible with Python 3.12

**Solution**: CMakeLists.txt force-fetches pybind11 v2.13.1 from GitHub
```cmake
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.13.1
)
```

### Build Commands

**Full build with Python bindings**:
```bash
pip install -r requirements.txt
pip install -e .
```

**C++ only**:
```bash
mkdir build && cd build
cmake .. -DBUILD_PYTHON_BINDINGS=OFF
cmake --build . -j
```

### Environment

- **Compiler**: GCC 11
- **Python**: 3.12.7
- **CMake**: 3.15+
- **C++ Standard**: C++17

## Known Issues and Fixes

### Build Error History

1. **Missing `#include <stdexcept>`** in utils.hpp
   - Symptom: `'runtime_error' is not a member of 'std'`
   - Fix: Added include

2. **Missing `#include <algorithm>`** in patterns.cpp
   - Symptom: `'transform' is not a member of 'std'`
   - Fix: Added include

3. **pybind11 Python 3.12 incompatibility**
   - Symptom: Errors about `PyFrameObject`, `PyThreadState`
   - Fix: Force fetch pybind11 v2.13.1 in CMakeLists.txt

4. **FlowGenerator copyability**
   - Symptom: Static assertion failure about constructibility
   - Root cause: Contains non-copyable `std::vector<std::unique_ptr<PatternGenerator>>`
   - Fix: Added deleted copy/move constructors, used `std::unique_ptr` holder in pybind11

5. **Traffic patterns list assignment**
   - Symptom: Config initialization failed, patterns list empty
   - Root cause: pybind11 vector handling doesn't support append
   - Fix: Build list in Python, assign whole list at once

6. **Export method name mismatch**
   - Symptom: AttributeError for 'to_csv_row'
   - Root cause: C++ has `to_csv()`, Python has `to_csv_row()`
   - Fix: Exporter checks for both methods

## Testing

### Test Files Generated

- `nanosec_test.csv` - Test with 100 flows showing nanosecond precision
- Various ad-hoc test outputs

### Verification Commands

```bash
# Build and test
pip install -e .

# Basic test
python -c "
from flowgen import FlowGenerator, export_csv
gen = FlowGenerator()
gen.initialize('configs/example_config.yaml')
flows = list(gen)
export_csv(flows[:10], 'test.csv')
print(f'Generated {len(flows)} flows')
"

# Nanosecond timestamp test
python -c "
from flowgen import FlowGenerator, export_csv
gen = FlowGenerator()
gen.initialize('configs/example_config.yaml')
flows = [next(gen) for _ in range(10)]
for i, f in enumerate(flows[:5], 1):
    print(f'{i}. ts={f.timestamp:,} ns')
"
```

## Current Status

### ✅ Completed Features

- [x] C++ core implementation
- [x] Python bindings via pybind11
- [x] Iterator pattern interface
- [x] Config file support (YAML/JSON)
- [x] Traffic pattern mixing with percentages
- [x] Bandwidth-based rate calculation
- [x] Multiple traffic patterns (web, DNS, SSH, database, SMTP, FTP, random)
- [x] CSV/JSON/JSONLines export
- [x] **Nanosecond timestamp resolution**
- [x] Fast-forward generation (no real-time delays)
- [x] Build system with Python 3.12 compatibility
- [x] All tests passing

### ⏳ UNIMPLEMENTED_SCENARIOS (Deliberately Not Implemented Yet)

Per user specifications, the following are intentionally not implemented:

1. **State Persistence**: No checkpoint/resume, no saving generator state
2. **Bidirectional Flows**: Only one-way flows, no request-response tracking
3. **NetFlow/IPFIX Export**: Only CSV/JSON export formats
4. **Dynamic Config Reload**: Config loaded once at initialization
5. **Config Templating**: Simple config files only
6. **Protocol-Specific Statistical Models**: Simple packet size distributions

These may be added in future sessions if requested.

## Session History Summary

### Session 1-2: Initial Implementation
- Created implementation plan
- User provided critical design decisions (fast-forward, bandwidth-based, C++ primary)
- Implemented hybrid C++/Python architecture
- Built and tested initial version

### Session 3: Bug Fixes and Polish
- Fixed multiple compilation errors
- Resolved pybind11 compatibility issues
- Verified build and basic functionality

### Session 4: Nanosecond Timestamps (Latest)
- **User requirement**: "I need the timestamp in nanosec resolution."
- Changed all timestamp handling from `double` (seconds) to `uint64_t` (nanoseconds)
- Updated all C++ code, Python bindings, and wrapper layer
- Verified 40 Gbps scenario with 160ns inter-arrival time
- Tests confirmed nanosecond precision working correctly
- **Status**: Feature complete and tested

## API Changes - Nanosecond Update

### C++ API

**Before**:
```cpp
struct FlowRecord {
    double timestamp;  // Unix epoch in seconds
};
struct GeneratorConfig {
    double start_timestamp;  // Unix epoch in seconds
};
```

**After**:
```cpp
struct FlowRecord {
    uint64_t timestamp;  // Unix epoch in nanoseconds
};
struct GeneratorConfig {
    uint64_t start_timestamp_ns;  // Unix epoch in nanoseconds
};
```

### Python API

**Config (user-facing, unchanged)**:
```yaml
generation:
  start_timestamp: 1704067200  # Still in seconds
```

**Python converts automatically**:
```python
cpp_config.start_timestamp_ns = int(py_config.generation.start_timestamp * 1e9)
```

**Backward compatibility**:
```python
generator.current_timestamp       # Returns seconds (float)
generator.current_timestamp_ns    # Returns nanoseconds (int)
```

## For Future Claude Sessions

### Quick Start Context

If starting a new session on this project:

1. **Read this file first** - CLAUDE.md
2. **Key files to understand**:
   - `cpp/include/flowgen/generator.hpp` - Main generator interface
   - `flowgen/__init__.py` - Python wrapper
   - `configs/example_config.yaml` - Config format
3. **Build commands**:
   - `pip install -e .` - Full build with Python
   - Check BUILD.md for details
4. **Test the build**:
   - Run Python test from this document
   - Check nanosecond timestamps are working

### Common Tasks

**Adding a new traffic pattern**:
1. Add class in `cpp/src/patterns.cpp`
2. Update `create_pattern_generator()` factory
3. Update config examples

**Modifying config schema**:
1. Update `flowgen/config.py` - Python dataclasses
2. Update `cpp/include/flowgen/generator.hpp` - GeneratorConfig
3. Update `flowgen/__init__.py` - _python_config_to_cpp()
4. Update example configs

**Performance optimization**:
- Focus on C++ core (`cpp/src/generator.cpp`, `cpp/src/patterns.cpp`)
- Pattern generation is hot path
- Random number generation is critical

### Debug Tips

**Build issues**:
- Check pybind11 version (must be v2.13.1+)
- Ensure C++17 support
- Check Python version (3.8+)

**Runtime issues**:
- Set `HAS_CPP_CORE = True` check in `flowgen/__init__.py`
- If False, C++ extension didn't build correctly
- Check Python can import `_flowgen_core` module

**Timestamp issues**:
- All C++ uses nanoseconds (uint64_t)
- Python config uses seconds (float), converted in wrapper
- CSV export shows full nanosecond values

## Project Philosophy

1. **Performance First**: C++ for hot paths
2. **User-Friendly**: Python for configuration and orchestration
3. **Production Ready**: Designed for real network testing tools
4. **Simple and Clear**: No over-engineering, straightforward design
5. **Fast-Forward**: Generate test data as fast as possible, realistic timestamps

## Contact Points with User

User confirmed preferences:
- "Will be called primarily from c++ application"
- "I like Option 1 - hybrid approach"
- "No" to real-time rate limiting
- "Keep Config Schema as simple as possible"
- "I need the timestamp in nanosec resolution" ← Latest requirement

---

## Session 5: Planned Enhancements (2025-11-01)

### 1. IP Address Optimization (Planned)

**User Request**: "Change the source_ip and destination_ip in FlowRecord from std::string to uint32_t"

**Rationale**:
- Performance: No string allocations in hot path
- Memory: 4 bytes vs ~15+ bytes per IP
- Cache efficiency: Better data locality
- Expected improvement: 5-15% faster generation

**Plan**:
- Change `FlowRecord` to use `uint32_t` for IPs (host byte order)
- Add utility functions: `ip_str_to_uint32()`, `uint32_to_ip_str()`
- Update pattern generators to use uint32_t internally
- CSV export converts to dotted decimal (format unchanged)
- Python bindings expose both `source_ip` (uint32_t) and `source_ip_str` (string property)

**Status**: ⏳ Planned, not yet implemented

### 2. Bidirectional Traffic Mode (Planned)

**User Request**: "Generate simple one-way flows but switch source and destination IPs and ports to look bidirectional"

**Approach**: Path B - Random Direction (stateless)
- No conversation tracking or state
- Randomly swap src/dst subnets based on probability
- Smart subnet configuration makes swapped flows look like responses

**Configuration**:
```yaml
generation:
  bidirectional_mode: "random"  # "none" or "random"
  bidirectional_probability: 0.5  # 0.0 to 1.0

network:
  # Client networks
  source_subnets:
    - "192.168.0.0/16"
    - "10.10.0.0/16"

  # Server networks
  destination_subnets:
    - "10.100.0.0/16"
    - "203.0.113.0/24"
```

**Key Insight**: Use distinct client vs server subnets so swapped flows naturally look like server→client responses.

**Example Traffic**:
```
192.168.45.10:54321 → 10.100.5.50:443    [client → server]
10.100.5.50:443 → 192.168.45.10:54321    [server → client - SWAPPED]
```

**Implementation**:
- Simple: Just randomly swap src/dst subnets in generator
- Zero state, completely stateless
- Realistic with proper subnet design

**Status**: ⏳ Planned, not yet implemented

### 3. Enhanced C++ Example with CLI Arguments (Implemented)

**User Request**: "Change basic_usage.cpp to take config options as inputs with defaults"

**Features**:
- Command-line argument parsing
- Smart defaults (enterprise network scenario)
- All key parameters configurable via CLI
- Comprehensive help text
- Reproducible with `--seed` option

**Default Configuration**:
```
Flows: 100,000
Bandwidth: 10 Gbps
Source subnets: 192.168.0.0/16, 10.10.0.0/16
Destination subnets: 10.100.0.0/16, 203.0.113.0/24
Patterns: web:40, database:20, dns:20, ssh:10, random:10
```

**Usage Examples**:
```bash
# Default enterprise scenario
./basic_usage

# 1M flows at 40 Gbps with bidirectional
./basic_usage --flows 1000000 --bandwidth 40 --bidirectional 0.6

# Service mesh scenario
./basic_usage --bidirectional 0.7 \
  --src-subnets "10.1.0.0/16,10.2.0.0/16,10.3.0.0/16" \
  --dst-subnets "10.10.0.0/16,10.11.0.0/16,10.12.0.0/16"
```

**Status**: ✅ Implemented this session

---

## Session 6: IP Optimization and Bidirectional Mode (2025-11-04)

### 1. IP Address Optimization (✅ COMPLETED)

**Implementation**: Changed FlowRecord IPs from `std::string` to `uint32_t`

**Changes Made**:
- Added IP conversion utilities in utils.hpp/cpp
  - `ip_str_to_uint32()`, `uint32_to_ip_str()`
  - `parse_subnet()` for proper CIDR handling
  - `random_ipv4_uint32()` and `random_ip_from_subnets_uint32()`
- Updated FlowRecord structure
  - `source_ip` and `destination_ip` now uint32_t
  - Added `source_ip_str()` and `destination_ip_str()` methods
  - Convenience constructor still accepts string IPs
- Updated all 7 pattern generators to use uint32_t internally
- Updated Python bindings
  - Exposed both uint32 and string properties
  - Added IP utility functions to Python API

**Performance Benefits**:
- Memory: 22 bytes saved per flow (73% reduction for IP fields)
- For 1M flows: ~21 MB memory savings
- Better cache locality with fixed-size integers
- Measured: 248,904 flows/sec (Python API)

**Backward Compatibility**:
- ✅ Python API: Both `flow.source_ip` (uint32) and `flow.source_ip_str` (string) available
- ✅ CSV output unchanged (still dotted decimal format)
- ✅ String constructor still available for convenience

### 2. Bidirectional Traffic Mode (✅ COMPLETED)

**Implementation**: Stateless random direction swapping

**Configuration**:
```yaml
generation:
  bidirectional_mode: random          # "none" or "random"
  bidirectional_probability: 0.5      # 0.0 to 1.0
```

**How It Works**:
- Randomly swaps src/dst IPs and ports after pattern generation
- Stateless - no conversation tracking overhead
- Use distinct client vs server subnets for realistic traffic

**Test Results**:
- Configuration: 50% probability
- Actual result: 49.9% reversed flows (49,864/100,000)
- ✅ Working perfectly

**Files Updated**:
- `configs/example_config.yaml` - Enhanced documentation
- `configs/bidirectional_config.yaml` - New example config

**Note**: Generator logic was already implemented in previous session, just needed testing and documentation

---

**Last Updated**: Session 7 - 2025-11-07 (Implemented flowstats modular tool)

**Status**:
- ✅ Ready for production use from C++ or Python
- ✅ Enhanced C++ example with CLI arguments
- ✅ IP optimization complete (uint32_t with 73% memory reduction)
- ✅ Bidirectional mode complete (stateless, configurable probability)
- ✅ All tests passing (248K flows/sec, 49.9% bidirectional accuracy)
- ✅ **flowstats** modular tool complete (lock-free, multi-threaded, progress tracking)

---

## Session 7: FlowStats Modular Tool (2025-11-07)

### Overview

**flowstats** is a modular C++17 application for processing and analyzing network flow data with:
- **Subcommand-based CLI** (like git: `flowstats collect`, `flowstats analyze`, etc.)
- **Lock-free threading** using atomics and per-thread buffers
- **Real-time progress tracking** with timestamp-based progression
- **Template-based architecture** for easy extensibility
- **Multiple output formats** (text, CSV, JSON, JSON-pretty)

### Architecture

**Base Class Template** (Template Method Pattern):
```cpp
template<typename ResultType>
class FlowStatsCommand {
    virtual bool validate_options() = 0;
    virtual void initialize() = 0;
    virtual void run_worker_thread(size_t thread_id) = 0;
    virtual ResultType collect_results() = 0;
    virtual void output_results(const ResultType& results) = 0;
};
```

**Current Subcommands**:
- `flowstats collect` - Generate and collect flow records (multi-threaded)

**Future Subcommands** (easy to add):
- `flowstats analyze` - Analyze flow characteristics
- `flowstats filter` - Filter flows by criteria
- `flowstats aggregate` - Aggregate flows by key

### Lock-Free Threading Model

**Key Innovation**: Zero locks, maximum throughput

```cpp
// Per-thread data using unique_ptr (atomics not movable/copyable)
std::vector<std::unique_ptr<PerThreadData>> m_thread_data;

struct PerThreadData {
    uint32_t m_thread_id;
    std::atomic<size_t> m_flows_generated;   // Lock-free counter
    std::atomic<uint64_t> m_bytes_generated; // Lock-free counter
    std::atomic<bool> m_done;                // Lock-free flag
};

// Per-thread buffers (exclusive ownership, no contention)
std::vector<ThreadFlowBuffer> m_thread_buffers;

// Collection uses move semantics (zero-copy)
result.m_flows.insert(
    result.m_flows.end(),
    std::make_move_iterator(buffer.m_flows.begin()),
    std::make_move_iterator(buffer.m_flows.end())
);
```

### Progress Tracking

**Real-time feedback** during long operations:

```bash
[=================>              ] 52.3% | Time: 2024-01-01 12:34:56 | ETA: 2m 15s | 156250 flows/s | 10.05 Gbps
```

**Features**:
- Timestamp-based progression (not just flow count)
- Multiple display styles (bar, simple, spinner, none)
- Lock-free updates from worker threads
- Separate display thread (non-blocking)
- ETA calculation based on time range

### File Organization

```
cpp/tools/flowstats/
├── core/
│   ├── progress_tracker.h/cpp      # Lock-free progress tracking
│   ├── flowstats_base.h            # Base class template
│   └── output_formatters.h         # Output format strategies
├── subcommands/
│   └── collect_command.h           # Collect subcommand
├── utils/
│   ├── arg_parser.h                # Argument parser
│   └── enhanced_flow.h/cpp         # Flow record with stats
├── main.cpp                        # Entry point
└── CMakeLists.txt                  # Build configuration
```

### Usage Examples

```bash
# Basic usage with progress
flowstats collect -n 10 -t 100000 -o csv > flows.csv

# JSON pretty output
flowstats collect -n 5 -f 1000 -o json-pretty > flows.json

# Silent mode for scripting
flowstats collect -n 10 -t 50000 --no-progress -o text

# Custom progress style
flowstats collect -n 10 -t 100000 --progress-style simple

# Timestamp range
flowstats collect --start-timestamp 1704067200000000000 \
                  --end-timestamp 1704067201000000000 \
                  -o csv
```

### Design Decisions

**Key Decisions**:

1. **Template Method Pattern**: Enforces consistent workflow, maximizes code reuse
2. **Lock-Free Threading**: Maximum throughput, no contention, predictable performance
3. **unique_ptr for PerThreadData**: Solves atomic movability issue elegantly
4. **Timestamp-Based Progress**: More accurate than flow count for time-based generation
5. **Separate Progress Thread**: Non-blocking display, clean separation of concerns
6. **Header Convention**: Use `.h` not `.hpp` (user requirement)
7. **Member Naming**: Use `m_` prefix (user requirement)

### Technical Challenges Solved

**Challenge 1**: Atomics in vector
- **Problem**: `std::atomic` is not copyable/movable, `std::vector::resize()` fails
- **Solution**: Use `std::vector<std::unique_ptr<PerThreadData>>`

**Challenge 2**: Lock-free collection
- **Problem**: Need to aggregate results from multiple threads without locks
- **Solution**: Per-thread buffers + move semantics after threads complete

**Challenge 3**: Progress tracking across threads
- **Problem**: Need real-time progress without impacting worker threads
- **Solution**: Atomic timestamps per thread, separate display thread reads minimum

### Documentation

- **FLOWSTATS_DESIGN.md**: Complete design reference for future development
- **FLOWSTATS_IMPLEMENTATION.md**: Implementation details and test results

### Test Results

✅ **All tests passing**:
- Help system ✓
- Multiple output formats (text, CSV, JSON, JSON-pretty) ✓
- Multi-threading (2, 3, 10 threads) ✓
- Progress indicators (bar, simple, spinner, none) ✓
- Flow generation with realistic statistics ✓

**Performance**:
- Estimated: 100K+ flows/second aggregate throughput
- Lock-free design scales linearly with thread count
- Memory efficient: move semantics, zero-copy collection

### How to Extend

**Adding a new subcommand** (e.g., `flowstats analyze`):

1. Define result type:
   ```cpp
   struct AnalyzeResult {
       std::map<std::string, uint64_t> m_protocol_distribution;
       // ... custom fields
   };
   ```

2. Create subcommand class:
   ```cpp
   class FlowStatsAnalyze : public FlowStatsCommand<AnalyzeResult> {
       // Implement 5 virtual functions
   };
   ```

3. Add entry point in `main.cpp`

**That's it!** Base class handles threading, progress, statistics automatically.

---

**Last Updated**: Session 7 - 2025-11-07
