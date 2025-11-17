"""
Configuration file parser and validator for FlowGen.
"""

import json
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Dict, Optional, Any, Tuple

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class RateConfig:
    """Flow generation rate configuration"""
    bandwidth_gbps: Optional[float] = None
    flows_per_second: Optional[float] = None

    def validate(self) -> Tuple[bool, Optional[str]]:
        """Validate rate configuration"""
        if self.bandwidth_gbps is None and self.flows_per_second is None:
            return False, "Must specify either bandwidth_gbps or flows_per_second"

        if self.bandwidth_gbps is not None and self.bandwidth_gbps <= 0:
            return False, f"bandwidth_gbps must be positive, got {self.bandwidth_gbps}"

        if self.flows_per_second is not None and self.flows_per_second <= 0:
            return False, f"flows_per_second must be positive, got {self.flows_per_second}"

        return True, None


@dataclass
class GenerationConfig:
    """Generation parameters"""
    max_flows: Optional[int] = None
    duration_seconds: Optional[float] = None
    rate: RateConfig = field(default_factory=RateConfig)
    start_timestamp: Optional[float] = None
    bidirectional_mode: str = "none"
    bidirectional_probability: float = 0.5

    def validate(self) -> Tuple[bool, Optional[str]]:
        """Validate generation configuration"""
        # Check at least one stop condition
        if self.max_flows is None and self.duration_seconds is None:
            return False, "Must specify at least one of: max_flows, duration_seconds"

        if self.max_flows is not None and self.max_flows <= 0:
            return False, f"max_flows must be positive, got {self.max_flows}"

        if self.duration_seconds is not None and self.duration_seconds <= 0:
            return False, f"duration_seconds must be positive, got {self.duration_seconds}"

        # Validate rate config
        rate_valid, rate_error = self.rate.validate()
        if not rate_valid:
            return False, rate_error

        # Validate start timestamp
        if self.start_timestamp is not None and self.start_timestamp < 0:
            return False, f"start_timestamp must be non-negative, got {self.start_timestamp}"

        # Validate bidirectional mode
        if self.bidirectional_mode not in ["none", "random"]:
            return False, f"bidirectional_mode must be 'none' or 'random', got '{self.bidirectional_mode}'"

        # Validate bidirectional probability
        if self.bidirectional_probability < 0.0 or self.bidirectional_probability > 1.0:
            return False, f"bidirectional_probability must be between 0.0 and 1.0, got {self.bidirectional_probability}"

        return True, None


@dataclass
class TrafficPatternConfig:
    """Individual traffic pattern configuration"""
    type: str
    percentage: float
    config: Dict[str, Any] = field(default_factory=dict)

    def validate(self) -> Tuple[bool, Optional[str]]:
        """Validate traffic pattern configuration"""
        if not self.type:
            return False, "Traffic pattern type cannot be empty"

        if self.percentage < 0 or self.percentage > 100:
            return False, f"Percentage must be between 0 and 100, got {self.percentage}"

        return True, None


@dataclass
class NetworkConfig:
    """Network topology configuration"""
    source_subnets: List[str] = field(default_factory=lambda: ["192.168.1.0/24"])
    destination_subnets: List[str] = field(default_factory=lambda: ["10.0.0.0/8"])
    source_weights: Optional[List[float]] = None

    def validate(self) -> Tuple[bool, Optional[str]]:
        """Validate network configuration"""
        if not self.source_subnets:
            return False, "source_subnets cannot be empty"

        if not self.destination_subnets:
            return False, "destination_subnets cannot be empty"

        # Validate weights if provided
        if self.source_weights is not None:
            if len(self.source_weights) != len(self.source_subnets):
                return False, "source_weights length must match source_subnets length"

            if abs(sum(self.source_weights) - 100.0) > 0.01:
                return False, f"source_weights must sum to 100, got {sum(self.source_weights)}"

        return True, None


@dataclass
class PacketConfig:
    """Packet size configuration"""
    min_size: int = 64
    max_size: int = 1500
    average_size: Optional[int] = None

    def validate(self) -> Tuple[bool, Optional[str]]:
        """Validate packet configuration"""
        if self.min_size < 0:
            return False, f"min_size cannot be negative, got {self.min_size}"

        if self.max_size < self.min_size:
            return False, f"max_size ({self.max_size}) must be >= min_size ({self.min_size})"

        if self.average_size is not None:
            if self.average_size < self.min_size or self.average_size > self.max_size:
                return False, f"average_size must be between min_size and max_size"

        return True, None


@dataclass
class FlowGenConfig:
    """Complete FlowGen configuration"""
    generation: GenerationConfig
    traffic_patterns: List[TrafficPatternConfig]
    network: NetworkConfig = field(default_factory=NetworkConfig)
    packets: PacketConfig = field(default_factory=PacketConfig)

    def validate(self) -> Tuple[bool, Optional[str]]:
        """
        Validate complete configuration.

        Returns:
            (is_valid, error_message)
        """
        # Validate generation config
        gen_valid, gen_error = self.generation.validate()
        if not gen_valid:
            return False, f"Generation config error: {gen_error}"

        # Validate traffic patterns
        if not self.traffic_patterns:
            return False, "Must specify at least one traffic pattern"

        total_percentage = 0.0
        for pattern in self.traffic_patterns:
            pattern_valid, pattern_error = pattern.validate()
            if not pattern_valid:
                return False, f"Traffic pattern error: {pattern_error}"
            total_percentage += pattern.percentage

        # Check percentages sum to 100
        if abs(total_percentage - 100.0) > 0.01:
            return False, f"Traffic pattern percentages sum to {total_percentage}, must be 100"

        # Validate network config
        net_valid, net_error = self.network.validate()
        if not net_valid:
            return False, f"Network config error: {net_error}"

        # Validate packet config
        pkt_valid, pkt_error = self.packets.validate()
        if not pkt_valid:
            return False, f"Packet config error: {pkt_error}"

        return True, None


class ConfigParser:
    """Parse and load configuration files"""

    @staticmethod
    def load(config_path: str) -> FlowGenConfig:
        """
        Load configuration from file.

        Supports: .yaml, .yml, .json

        Args:
            config_path: Path to configuration file

        Returns:
            Parsed FlowGenConfig object

        Raises:
            FileNotFoundError: If config file doesn't exist
            ValueError: If file format unsupported or config invalid
        """
        path = Path(config_path)

        if not path.exists():
            raise FileNotFoundError(f"Config file not found: {config_path}")

        suffix = path.suffix.lower()

        if suffix in ['.yaml', '.yml']:
            if not HAS_YAML:
                raise ValueError("PyYAML not installed. Install with: pip install pyyaml")
            return ConfigParser._load_yaml(path)
        elif suffix == '.json':
            return ConfigParser._load_json(path)
        else:
            raise ValueError(f"Unsupported config format: {suffix}. Use .yaml, .yml, or .json")

    @staticmethod
    def _load_yaml(path: Path) -> FlowGenConfig:
        """Load YAML configuration"""
        with open(path, 'r') as f:
            data = yaml.safe_load(f)
        return ConfigParser._parse_dict(data)

    @staticmethod
    def _load_json(path: Path) -> FlowGenConfig:
        """Load JSON configuration"""
        with open(path, 'r') as f:
            data = json.load(f)
        return ConfigParser._parse_dict(data)

    @staticmethod
    def _parse_dict(data: Dict) -> FlowGenConfig:
        """
        Parse dictionary into FlowGenConfig object.

        Args:
            data: Configuration dictionary

        Returns:
            FlowGenConfig object

        Raises:
            ValueError: If configuration is invalid
        """
        # Parse generation config
        gen_data = data.get('generation', {})
        rate_data = gen_data.get('rate', {})

        rate_config = RateConfig(
            bandwidth_gbps=rate_data.get('bandwidth_gbps'),
            flows_per_second=rate_data.get('flows_per_second')
        )

        generation_config = GenerationConfig(
            max_flows=gen_data.get('max_flows'),
            duration_seconds=gen_data.get('duration_seconds'),
            rate=rate_config,
            start_timestamp=gen_data.get('start_timestamp'),
            bidirectional_mode=gen_data.get('bidirectional_mode', 'none'),
            bidirectional_probability=gen_data.get('bidirectional_probability', 0.5)
        )

        # Parse traffic patterns
        patterns_data = data.get('traffic_patterns', [])
        traffic_patterns = [
            TrafficPatternConfig(
                type=p.get('type', 'random'),
                percentage=p.get('percentage', 0),
                config=p.get('config', {})
            )
            for p in patterns_data
        ]

        # Parse network config
        net_data = data.get('network', {})
        network_config = NetworkConfig(
            source_subnets=net_data.get('source_subnets', ["192.168.1.0/24"]),
            destination_subnets=net_data.get('destination_subnets', ["10.0.0.0/8"]),
            source_weights=net_data.get('source_weights')
        )

        # Parse packet config
        pkt_data = data.get('packets', {})
        packet_config = PacketConfig(
            min_size=pkt_data.get('min_size', 64),
            max_size=pkt_data.get('max_size', 1500),
            average_size=pkt_data.get('average_size')
        )

        # Create config object
        config = FlowGenConfig(
            generation=generation_config,
            traffic_patterns=traffic_patterns,
            network=network_config,
            packets=packet_config
        )

        # Validate configuration
        is_valid, error_msg = config.validate()
        if not is_valid:
            raise ValueError(f"Invalid configuration: {error_msg}")

        return config
