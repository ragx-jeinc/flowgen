"""
FlowGen - High-performance network flow record generator

This package provides both C++ and Python interfaces for generating
realistic network flow data.
"""

from .config import ConfigParser, FlowGenConfig
from .exporters import export_csv, export_json, export_jsonlines, FlowExporter

# Try to import C++ core, fallback to Python if not available
try:
    from . import _flowgen_core
    HAS_CPP_CORE = True
except ImportError:
    HAS_CPP_CORE = False
    import warnings
    warnings.warn(
        "C++ core not available, using Python implementation. "
        "For better performance, build the C++ extension with: pip install -e ."
    )

# Public API
__version__ = "1.0.0"
__all__ = [
    "FlowGenerator",
    "ConfigParser",
    "FlowGenConfig",
    "export_csv",
    "export_json",
    "export_jsonlines",
    "FlowExporter",
]

if HAS_CPP_CORE:
    # Use C++ backend
    class FlowGenerator:
        """
        High-performance flow generator using C++ backend.

        This generator can be used in two ways:
        1. Iterator pattern: for flow in generator: ...
        2. Direct calls from C++: Can be called from C++ applications

        Example:
            >>> gen = FlowGenerator()
            >>> gen.initialize("config.yaml")
            >>> for flow in gen:
            ...     process(flow)
        """

        def __init__(self):
            self._cpp_generator = _flowgen_core.FlowGenerator()
            self._config = None

        def initialize(self, config_path: str) -> bool:
            """
            Initialize generator from configuration file.

            Args:
                config_path: Path to YAML or JSON config file

            Returns:
                True if initialization successful

            Raises:
                Exception if config is invalid
            """
            # Parse config using Python (for YAML/JSON support)
            self._config = ConfigParser.load(config_path)

            # Convert to C++ config
            cpp_config = self._python_config_to_cpp(self._config)

            # Initialize C++ generator
            return self._cpp_generator.initialize(cpp_config)

        def _python_config_to_cpp(self, py_config: FlowGenConfig):
            """Convert Python config to C++ GeneratorConfig"""
            cpp_config = _flowgen_core.GeneratorConfig()

            # Rate configuration
            if py_config.generation.rate.bandwidth_gbps is not None:
                cpp_config.bandwidth_gbps = py_config.generation.rate.bandwidth_gbps
            if py_config.generation.rate.flows_per_second is not None:
                cpp_config.flows_per_second = py_config.generation.rate.flows_per_second

            # Stop conditions
            if py_config.generation.max_flows is not None:
                cpp_config.max_flows = py_config.generation.max_flows
            if py_config.generation.duration_seconds is not None:
                cpp_config.duration_seconds = py_config.generation.duration_seconds

            # Timestamp (convert seconds to nanoseconds)
            if py_config.generation.start_timestamp is not None:
                cpp_config.start_timestamp_ns = int(py_config.generation.start_timestamp * 1e9)

            # Network configuration
            cpp_config.source_subnets = py_config.network.source_subnets
            cpp_config.destination_subnets = py_config.network.destination_subnets
            if py_config.network.source_weights:
                cpp_config.source_weights = py_config.network.source_weights

            # Packet configuration
            cpp_config.min_packet_size = py_config.packets.min_size
            cpp_config.max_packet_size = py_config.packets.max_size
            if py_config.packets.average_size:
                cpp_config.average_packet_size = py_config.packets.average_size
            else:
                cpp_config.average_packet_size = (
                    py_config.packets.min_size + py_config.packets.max_size
                ) // 2

            # Bidirectional mode configuration
            cpp_config.bidirectional_mode = py_config.generation.bidirectional_mode
            cpp_config.bidirectional_probability = py_config.generation.bidirectional_probability

            # Traffic patterns - build list and assign all at once
            patterns_list = []
            for pattern in py_config.traffic_patterns:
                cpp_pattern = _flowgen_core.TrafficPattern()
                cpp_pattern.type = pattern.type
                cpp_pattern.percentage = pattern.percentage
                patterns_list.append(cpp_pattern)
            cpp_config.traffic_patterns = patterns_list

            return cpp_config

        def __iter__(self):
            """Return iterator for flow records"""
            return self._cpp_generator.__iter__()

        def __next__(self):
            """Generate next flow record"""
            return self._cpp_generator.__next__()

        def is_done(self) -> bool:
            """Check if generation is complete"""
            return self._cpp_generator.is_done()

        def reset(self):
            """Reset generator to initial state"""
            self._cpp_generator.reset()

        def get_stats(self):
            """Get generation statistics"""
            return self._cpp_generator.get_stats()

        @property
        def flow_count(self) -> int:
            """Get number of flows generated"""
            return self._cpp_generator.flow_count()

        @property
        def current_timestamp_ns(self) -> int:
            """Get current timestamp in nanoseconds"""
            return self._cpp_generator.current_timestamp_ns()

        @property
        def current_timestamp(self) -> float:
            """Get current timestamp in seconds (for backwards compatibility)"""
            return self._cpp_generator.current_timestamp_ns() / 1e9

else:
    # Fallback to Python implementation
    from .generator_interface import ConfigurableFlowGenerator as FlowGenerator


def print_info():
    """Print FlowGen library information"""
    print(f"FlowGen v{__version__}")
    print(f"C++ Core: {'Available' if HAS_CPP_CORE else 'Not available'}")
    if HAS_CPP_CORE:
        print("Using high-performance C++ backend")
    else:
        print("Using Python implementation")
