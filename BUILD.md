# Building FlowGen

**Last Updated**: 2025-11-02
**Tested Environment**: GCC 11.4.1, Python 3.9.18/3.12.7, pybind11 v2.13.1

## Quick Start

### Python (Virtual Environment)
```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
python setup.py build_ext --inplace
cp build/lib.*/flowgen/libflowgen.so flowgen/
source activate_venv.sh
python examples/basic_usage.py
```

### C++ (No Installation)
```bash
mkdir -p build && cd build
cmake .. -DBUILD_PYTHON_BINDINGS=OFF -DBUILD_EXAMPLES=ON
cmake --build . -j
LD_LIBRARY_PATH=. ./basic_usage
```

## Build Options

FlowGen uses CMake for building and supports multiple configurations:

1. **Python Package** (recommended for Python users)
2. **Python Virtual Environment** (isolated development)
3. **C++ Library Only** (for C++ applications)
4. **C++ Examples** (standalone applications)

## Prerequisites

### All Platforms

- **CMake** 3.15 or later
- **C++17 compiler**:
  - GCC 7+ (Linux)
  - Clang 5+ (macOS/Linux)
  - MSVC 2017+ (Windows)

### For Python Bindings

- **Python** 3.8 or later
- **pip** for package installation

### Dependencies

The build system will automatically fetch:
- **pybind11** (if not found on system)

## Quick Build

### Python Package (C++ + Python)

```bash
# Install in development mode
pip install -e .

# Or install normally
pip install .
```

This will:
1. Build the C++ library (`libflowgen.so` or `flowgen.dll`)
2. Build the Python extension module (`_flowgen_core`)
3. Install the Python package

### Python Virtual Environment (Recommended for Development)

**Note**: Due to shared library dependencies, using a virtual environment requires setting `LD_LIBRARY_PATH`.

```bash
# Create and activate virtual environment
python3 -m venv venv
source venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Build the C++ extension (creates libflowgen.so and _flowgen_core.*.so)
python setup.py build_ext --inplace

# Copy libflowgen.so to flowgen directory
cp build/lib.*/flowgen/libflowgen.so flowgen/

# Use the activation script (sets PYTHONPATH and LD_LIBRARY_PATH)
source activate_venv.sh

# Verify installation
python -c "from flowgen import HAS_CPP_CORE; print(f'C++ Core: {HAS_CPP_CORE}')"
# Should print: C++ Core: True

# Run examples
python examples/basic_usage.py
python examples/streaming_usage.py
```

**Quick Reference** (after initial setup):
```bash
source activate_venv.sh
python examples/basic_usage.py
```

### C++ Library Only

```bash
mkdir build && cd build
cmake .. -DBUILD_PYTHON_BINDINGS=OFF
cmake --build . -j$(nproc)
sudo cmake --install .
```

### C++ Examples (Without Installation)

Build and run C++ examples without installing the library system-wide:

```bash
# Create build directory
mkdir -p build
cd build

# Configure for C++ only with examples
cmake .. -DBUILD_PYTHON_BINDINGS=OFF -DBUILD_EXAMPLES=ON

# Build library and example (both are built automatically)
cmake --build . -j

# Run the example (libflowgen.so is automatically copied to build root)
LD_LIBRARY_PATH=. ./basic_usage

# Or with options
LD_LIBRARY_PATH=. ./basic_usage --flows 10000 --bandwidth 40 --output test.csv

# Output: cpp_output_flows.csv with nanosecond timestamps
```

**Note**: When BUILD_EXAMPLES=ON, both `libflowgen.so` and `basic_usage` are placed in the build root directory for convenient execution.

**Performance**: Pure C++ achieves ~370K flows/sec (vs ~450K flows/sec with Python API)

## Platform-Specific Instructions

### Linux

```bash
# Install build dependencies (Ubuntu/Debian)
sudo apt-get install build-essential cmake python3-dev

# Build
pip install -e .

# Or C++ only
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### macOS

```bash
# Install dependencies
brew install cmake python@3.11

# Build
pip install -e .

# Or C++ only
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
sudo make install
```

### Windows

```bash
# Using Visual Studio 2019/2022
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release

# Install
cmake --install . --config Release
```

## CMake Options

Configure the build with CMake options:

```bash
cmake .. \
  -DBUILD_PYTHON_BINDINGS=ON \    # Build Python module (default: OFF)
  -DBUILD_EXAMPLES=ON \           # Build C++ examples (default: ON)
  -DBUILD_SHARED_LIBS=ON \        # Build shared library (default: ON)
  -DCMAKE_BUILD_TYPE=Release      # Release or Debug
```

### BUILD_EXAMPLES Option

When `BUILD_EXAMPLES=ON` (default):
- Builds `basic_usage` executable
- Automatically copies `libflowgen.so` to build root
- Both binaries in same directory for easy execution

When `BUILD_EXAMPLES=OFF`:
- Only builds `libflowgen.so`
- No C++ examples compiled

**Directory structure with BUILD_EXAMPLES=ON**:
```
build/
├── libflowgen.so       # C++ library (copied here)
├── basic_usage         # C++ example executable
└── ...
```

## Development Build

Build everything including examples:

```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_PYTHON_BINDINGS=ON

cmake --build . -j
```

This creates:
- `libflowgen.so` - C++ library (in build root)
- `_flowgen_core.so` - Python extension (in project root under flowgen/)
- `basic_usage` - C++ example (in build root)

**Note**: With BUILD_EXAMPLES=ON, `libflowgen.so` is automatically copied to the build root so `basic_usage` can find it.

## Testing the Build

### Python (Virtual Environment)

```bash
# Activate environment
source activate_venv.sh

# Quick test
python -c "
from flowgen import FlowGenerator, HAS_CPP_CORE
print(f'C++ Core available: {HAS_CPP_CORE}')

# Generate a few flows
gen = FlowGenerator()
gen.initialize('configs/example_config.yaml')
flows = [next(gen) for _ in range(5)]
for i, f in enumerate(flows, 1):
    print(f'{i}. ts={f.timestamp} ns')
"

# Run full example
python examples/basic_usage.py
```

**Expected Output**:
```
C++ Core available: True
1. ts=1704067200000000000 ns
2. ts=1704067200000000640 ns
3. ts=1704067200000001280 ns
...
Generation complete!
Total flows: 100,000
Elapsed time: 0.22 seconds
Actual generation rate: 449,367 flows/sec
```

### Python (System Install)

```python
import flowgen
flowgen.print_info()

# Should print:
# FlowGen v1.0.0
# C++ Core: Available
# Using high-performance C++ backend
```

### C++

```bash
# From build directory
LD_LIBRARY_PATH=. ./basic_usage

# Or with custom options
LD_LIBRARY_PATH=. ./basic_usage --flows 10000 --bandwidth 10 --output test.csv

# Expected output:
# FlowGen C++ Example
# ===================
#
# Configuration:
#   Flows: 100000
#   Bandwidth: 10 Gbps
#   Output: cpp_output_flows.csv
#   ...
#
# Initializing generator...
# Generator initialized successfully
# Target rate: 1.5625e+06 flows/sec
#
# Generating flows...
#
# Generation complete!
#   Total flows: 100000
#   Elapsed time: 0.27 seconds
#   Generation rate: 371,034 flows/sec
#   Timestamp range: 6.4e-05 seconds
#
# Output written to: cpp_output_flows.csv
```

**Verify nanosecond timestamps**:
```bash
head -5 cpp_output_flows.csv
# Should show timestamps like: 1704067200000000000, 1704067200000000640, ...
# Delta of 640ns = 10 Gbps bandwidth
```

## Linking Against FlowGen (C++ Applications)

### Using CMake

```cmake
find_package(flowgen REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE flowgen::flowgen)
```

### Using g++ Directly

```bash
g++ -std=c++17 my_app.cpp -lflowgen -o my_app
```

### pkg-config

```bash
g++ -std=c++17 my_app.cpp $(pkg-config --cflags --libs flowgen) -o my_app
```

## Troubleshooting

### pybind11 Not Found

The build system will automatically fetch pybind11. If you prefer using system pybind11:

```bash
# Linux
sudo apt-get install pybind11-dev

# macOS
brew install pybind11

# Or install via pip
pip install pybind11
```

### Python Extension Not Found

After building, the extension should be in `flowgen/_flowgen_core.*.so`. If Python can't find it:

```bash
# Reinstall in development mode
pip install -e . --force-reinstall
```

### "libflowgen.so: cannot open shared object file"

**Symptom**: Python imports fail with `ImportError: libflowgen.so: cannot open shared object file`

**Cause**: The Python extension `_flowgen_core.*.so` depends on `libflowgen.so`, but the loader can't find it.

**Solution 1** (Virtual Environment - Recommended):
```bash
# Copy libflowgen.so to the same directory as the Python module
cp build/lib.*/flowgen/libflowgen.so flowgen/

# Set LD_LIBRARY_PATH when running
export LD_LIBRARY_PATH=/path/to/flowgen/flowgen:$LD_LIBRARY_PATH

# Or use the activation script
source activate_venv.sh
```

**Solution 2** (System Install):
```bash
# Install the library system-wide
cd build
sudo cmake --install .
sudo ldconfig  # Update library cache
```

**Verify**:
```bash
ldd flowgen/_flowgen_core.*.so
# Should show: libflowgen.so => /path/to/libflowgen.so (found)
```

### C++ Standard Issues

Ensure your compiler supports C++17:

```bash
# Check GCC version
g++ --version  # Should be 7+

# Check Clang version
clang++ --version  # Should be 5+
```

### Missing numpy/yaml

```bash
pip install numpy pyyaml
```

## Performance Benchmarks

### Tested Performance (GCC 11, 10 Gbps config, 100K flows)

| API Type | Throughput | Timestamp Precision | Notes |
|----------|-----------|-------------------|-------|
| **Pure C++** | ~370K flows/sec | 640ns intervals | No Python overhead |
| **Python (pybind11)** | ~450K flows/sec | 640ns intervals | Includes iterator crossing |

**Timestamp Accuracy**: Both APIs produce identical nanosecond-precision timestamps
- 10 Gbps config → 640ns between flows
- 40 Gbps config → 160ns between flows
- Full uint64_t nanosecond resolution (no floating-point errors)

### Output Verification

```bash
# Check CSV timestamps
head -3 output_flows.csv
# timestamp,src_ip,dst_ip,src_port,dst_port,protocol,length
# 1704067200000000000,192.168.1.198,10.110.20.33,51913,6117,6,545
# 1704067200000000640,192.168.2.221,10.235.84.82,55199,443,6,817
#                 ^^^  640 nanosecond delta = 10 Gbps
```

## Performance Optimization

### Release Build

Always use Release mode for performance:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```

### Compiler Optimizations

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native"
```

### Link-Time Optimization (LTO)

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

**Expected Performance Improvement**: Up to 2-3x faster than default build for pure C++ usage

## Clean Build

```bash
# Remove build directory
rm -rf build

# Uninstall Python package
pip uninstall flowgen

# Rebuild
pip install -e .
```

## Installation Paths

Default installation locations:

### Linux/macOS
- Headers: `/usr/local/include/flowgen/`
- Library: `/usr/local/lib/libflowgen.so`
- CMake config: `/usr/local/lib/cmake/flowgen/`

### Windows
- Headers: `C:/Program Files/flowgen/include/`
- Library: `C:/Program Files/flowgen/lib/flowgen.lib`

### Python Module
- Installed in site-packages: `site-packages/flowgen/`

## Virtual Environment Helper Script

The `activate_venv.sh` script automates environment setup:

```bash
#!/bin/bash
# Activate virtual environment
source venv/bin/activate

# Set library paths
export PYTHONPATH=/pxstore/claude/projects/flowgen/flowgen:$PYTHONPATH
export LD_LIBRARY_PATH=/pxstore/claude/projects/flowgen/flowgen/flowgen:$LD_LIBRARY_PATH
```

**Usage**:
```bash
source activate_venv.sh
python examples/basic_usage.py
```

**Why is this needed?**
- `PYTHONPATH`: Tells Python where to find the `flowgen` module
- `LD_LIBRARY_PATH`: Tells the dynamic linker where to find `libflowgen.so`

Without these settings, you'll get import errors even though the build succeeded.

## Next Steps

After building, see:
- [README.md](README.md) for usage examples
- [examples/](examples/) for sample code
- [configs/](configs/) for configuration examples
- [CLAUDE.md](CLAUDE.md) for implementation context (AI sessions)
- [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) for technical documentation
