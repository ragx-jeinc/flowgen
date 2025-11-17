"""
Main flow generator interface with iterator pattern support.
"""

import time
from typing import Iterator, Optional, List
from abc import ABC, abstractmethod

from .models import FlowRecord
from .config import ConfigParser, FlowGenConfig
from .generators import get_pattern_generator, PatternGenerator
from .utils import weighted_random_choice, calculate_flows_per_second


class FlowGenerator(ABC):
    """Abstract base class for flow generators with iterator interface"""

    def __init__(self):
        self._initialized = False
        self._config = None

    @abstractmethod
    def initialize(self, config_path: str) -> bool:
        """
        Initialize generator from config file.

        Args:
            config_path: Path to configuration file (YAML/JSON)

        Returns:
            True if initialization successful, False otherwise
        """
        pass

    @abstractmethod
    def __iter__(self) -> Iterator[FlowRecord]:
        """Return iterator for flow records"""
        pass

    @abstractmethod
    def __next__(self) -> FlowRecord:
        """Generate and return next flow record"""
        pass

    def reset(self):
        """Reset generator to initial state"""
        pass

    def close(self):
        """Cleanup resources"""
        pass


class ConfigurableFlowGenerator(FlowGenerator):
    """
    Main generator implementation supporting:
    - One-at-a-time flow generation via iterator pattern
    - Config-driven traffic mix and patterns
    - Bandwidth-based or flow-rate-based generation
    """

    def __init__(self):
        super().__init__()
        self._config: Optional[FlowGenConfig] = None
        self._pattern_generators: List[PatternGenerator] = []
        self._pattern_weights: List[float] = []
        self._current_timestamp: float = 0.0
        self._flow_count: int = 0
        self._flows_per_second: float = 0.0
        self._inter_arrival_time: float = 0.0
        self._start_time: float = 0.0

    def initialize(self, config_path: str) -> bool:
        """
        Initialize from config file.

        Args:
            config_path: Path to YAML or JSON config file

        Returns:
            True if initialization successful, False otherwise
        """
        try:
            # Load and validate config
            self._config = ConfigParser.load(config_path)

            # Calculate flow rate
            if self._config.generation.rate.bandwidth_gbps is not None:
                # Calculate from bandwidth
                bandwidth_gbps = self._config.generation.rate.bandwidth_gbps

                # Get average packet size
                avg_size = self._config.packets.average_size
                if avg_size is None:
                    # Estimate from min/max
                    avg_size = (self._config.packets.min_size +
                               self._config.packets.max_size) / 2

                self._flows_per_second = calculate_flows_per_second(
                    bandwidth_gbps, avg_size
                )
            else:
                # Use explicit flow rate
                self._flows_per_second = self._config.generation.rate.flows_per_second

            # Calculate inter-arrival time (constant rate for now)
            self._inter_arrival_time = 1.0 / self._flows_per_second

            # Set start timestamp
            if self._config.generation.start_timestamp is not None:
                self._start_time = self._config.generation.start_timestamp
            else:
                self._start_time = time.time()

            self._current_timestamp = self._start_time

            # Initialize pattern generators
            self._pattern_generators = []
            self._pattern_weights = []

            for pattern_config in self._config.traffic_patterns:
                generator = get_pattern_generator(
                    pattern_config.type,
                    pattern_config.config
                )
                self._pattern_generators.append(generator)
                self._pattern_weights.append(pattern_config.percentage)

            self._initialized = True
            return True

        except Exception as e:
            print(f"Error initializing generator: {e}")
            return False

    def __iter__(self) -> Iterator[FlowRecord]:
        """Return iterator for flow records"""
        if not self._initialized:
            raise RuntimeError("Generator not initialized. Call initialize() first.")
        return self

    def __next__(self) -> FlowRecord:
        """
        Generate and return next flow record.

        Raises:
            StopIteration: When generation limits reached
        """
        if not self._initialized:
            raise RuntimeError("Generator not initialized. Call initialize() first.")

        # Check stop conditions
        if self._should_stop():
            raise StopIteration

        # Select pattern based on weights
        pattern = weighted_random_choice(
            self._pattern_generators,
            self._pattern_weights
        )

        # Generate flow record
        flow = pattern.generate(
            timestamp=self._current_timestamp,
            src_subnets=self._config.network.source_subnets,
            dst_subnets=self._config.network.destination_subnets,
            src_weights=self._config.network.source_weights,
            min_pkt_size=self._config.packets.min_size,
            max_pkt_size=self._config.packets.max_size
        )

        # Update state
        self._flow_count += 1
        self._current_timestamp += self._inter_arrival_time

        return flow

    def _should_stop(self) -> bool:
        """
        Check if generation should stop based on limits.

        Returns:
            True if limits reached, False otherwise
        """
        # Check flow count limit
        if self._config.generation.max_flows is not None:
            if self._flow_count >= self._config.generation.max_flows:
                return True

        # Check duration limit
        if self._config.generation.duration_seconds is not None:
            elapsed = self._current_timestamp - self._start_time
            if elapsed >= self._config.generation.duration_seconds:
                return True

        return False

    def reset(self):
        """Reset generator to initial state"""
        if self._initialized:
            self._current_timestamp = self._start_time
            self._flow_count = 0

    def close(self):
        """Cleanup resources"""
        self._initialized = False
        self._pattern_generators.clear()
        self._pattern_weights.clear()

    @property
    def flow_count(self) -> int:
        """Get current flow count"""
        return self._flow_count

    @property
    def current_timestamp(self) -> float:
        """Get current timestamp"""
        return self._current_timestamp

    @property
    def flows_per_second(self) -> float:
        """Get calculated flows per second rate"""
        return self._flows_per_second

    def get_stats(self) -> dict:
        """
        Get generator statistics.

        Returns:
            Dictionary with generator stats
        """
        if not self._initialized:
            return {}

        elapsed = self._current_timestamp - self._start_time

        return {
            'flows_generated': self._flow_count,
            'elapsed_time_seconds': elapsed,
            'flows_per_second': self._flows_per_second,
            'current_timestamp': self._current_timestamp,
            'start_timestamp': self._start_time,
        }
