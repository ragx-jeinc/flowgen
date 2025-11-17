"""
Traffic pattern generators for different network services.
"""

import random
from abc import ABC, abstractmethod
from typing import Dict, Any

from .models import FlowRecord
from .protocols import Protocol, SERVICE_PORTS
from .utils import (
    subnet_random_ip, random_port, random_packet_size,
    random_ipv4
)


class PatternGenerator(ABC):
    """Base class for traffic pattern generators"""

    def __init__(self, config: Dict[str, Any]):
        """
        Initialize pattern generator.

        Args:
            config: Pattern-specific configuration dictionary
        """
        self.config = config

    @abstractmethod
    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        """
        Generate a single flow record for this pattern.

        Args:
            timestamp: Unix epoch timestamp for the flow
            src_subnets: List of source subnet CIDR strings
            dst_subnets: List of destination subnet CIDR strings
            src_weights: Weights for source subnet selection
            min_pkt_size: Minimum packet size in bytes
            max_pkt_size: Maximum packet size in bytes

        Returns:
            Generated FlowRecord
        """
        pass


class RandomTrafficGenerator(PatternGenerator):
    """Generate completely random traffic"""

    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        src_ip = subnet_random_ip(src_subnets, src_weights)
        dst_ip = subnet_random_ip(dst_subnets)

        # Random protocol
        protocol = random.choice([Protocol.TCP, Protocol.UDP])

        # Random ports
        src_port = random_port('dynamic')
        dst_port = random_port('any')

        # Random packet size
        pkt_len = random_packet_size(min_pkt_size, max_pkt_size)

        return FlowRecord(
            source_ip=src_ip,
            destination_ip=dst_ip,
            source_port=src_port,
            destination_port=dst_port,
            protocol=protocol,
            timestamp=timestamp,
            packet_length=pkt_len
        )


class WebTrafficGenerator(PatternGenerator):
    """Generate HTTP/HTTPS traffic"""

    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        src_ip = subnet_random_ip(src_subnets, src_weights)
        dst_ip = subnet_random_ip(dst_subnets)

        # HTTP or HTTPS
        if random.random() < 0.7:  # 70% HTTPS
            dst_port = 443
        else:
            dst_port = 80

        src_port = random_port('dynamic')

        # Web traffic typically uses TCP
        protocol = Protocol.TCP

        # Bimodal packet size (small requests, larger responses)
        # Since we're doing one-way flows, just use bimodal distribution
        if random.random() < 0.4:
            pkt_len = random.randint(64, 200)  # Small packets
        else:
            pkt_len = random.randint(500, max_pkt_size)  # Larger packets

        return FlowRecord(
            source_ip=src_ip,
            destination_ip=dst_ip,
            source_port=src_port,
            destination_port=dst_port,
            protocol=protocol,
            timestamp=timestamp,
            packet_length=pkt_len
        )


class DnsTrafficGenerator(PatternGenerator):
    """Generate DNS traffic"""

    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        src_ip = subnet_random_ip(src_subnets, src_weights)
        dst_ip = subnet_random_ip(dst_subnets)

        # DNS uses port 53
        dst_port = 53
        src_port = random_port('dynamic')

        # DNS primarily uses UDP
        protocol = Protocol.UDP

        # DNS packets are typically small
        pkt_len = random.randint(64, 512)

        return FlowRecord(
            source_ip=src_ip,
            destination_ip=dst_ip,
            source_port=src_port,
            destination_port=dst_port,
            protocol=protocol,
            timestamp=timestamp,
            packet_length=pkt_len
        )


class SshTrafficGenerator(PatternGenerator):
    """Generate SSH traffic"""

    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        src_ip = subnet_random_ip(src_subnets, src_weights)
        dst_ip = subnet_random_ip(dst_subnets)

        # SSH uses port 22
        dst_port = 22
        src_port = random_port('dynamic')

        # SSH uses TCP
        protocol = Protocol.TCP

        # SSH packets are typically small and consistent (encrypted)
        pkt_len = random.randint(100, 400)

        return FlowRecord(
            source_ip=src_ip,
            destination_ip=dst_ip,
            source_port=src_port,
            destination_port=dst_port,
            protocol=protocol,
            timestamp=timestamp,
            packet_length=pkt_len
        )


class DatabaseTrafficGenerator(PatternGenerator):
    """Generate database traffic (MySQL, PostgreSQL, MongoDB)"""

    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        src_ip = subnet_random_ip(src_subnets, src_weights)
        dst_ip = subnet_random_ip(dst_subnets)

        # Random database port
        db_ports = [3306, 5432, 27017, 6379]  # MySQL, PostgreSQL, MongoDB, Redis
        dst_port = random.choice(db_ports)
        src_port = random_port('dynamic')

        # Databases use TCP
        protocol = Protocol.TCP

        # Database packets vary - queries small, results can be large
        if random.random() < 0.3:
            pkt_len = random.randint(64, 300)  # Small queries
        else:
            pkt_len = random.randint(500, max_pkt_size)  # Large result sets

        return FlowRecord(
            source_ip=src_ip,
            destination_ip=dst_ip,
            source_port=src_port,
            destination_port=dst_port,
            protocol=protocol,
            timestamp=timestamp,
            packet_length=pkt_len
        )


class SmtpTrafficGenerator(PatternGenerator):
    """Generate SMTP email traffic"""

    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        src_ip = subnet_random_ip(src_subnets, src_weights)
        dst_ip = subnet_random_ip(dst_subnets)

        # SMTP ports
        dst_port = random.choice([25, 587, 465])
        src_port = random_port('dynamic')

        protocol = Protocol.TCP

        # Email varies in size
        pkt_len = random.randint(200, max_pkt_size)

        return FlowRecord(
            source_ip=src_ip,
            destination_ip=dst_ip,
            source_port=src_port,
            destination_port=dst_port,
            protocol=protocol,
            timestamp=timestamp,
            packet_length=pkt_len
        )


class FtpTrafficGenerator(PatternGenerator):
    """Generate FTP traffic"""

    def generate(self, timestamp: float, src_subnets: list, dst_subnets: list,
                 src_weights: list, min_pkt_size: int, max_pkt_size: int) -> FlowRecord:
        src_ip = subnet_random_ip(src_subnets, src_weights)
        dst_ip = subnet_random_ip(dst_subnets)

        # FTP control or data port
        dst_port = random.choice([20, 21])
        src_port = random_port('dynamic')

        protocol = Protocol.TCP

        # FTP can transfer large files
        if dst_port == 20:  # Data port - larger packets
            pkt_len = random.randint(1000, max_pkt_size)
        else:  # Control port - smaller packets
            pkt_len = random.randint(64, 500)

        return FlowRecord(
            source_ip=src_ip,
            destination_ip=dst_ip,
            source_port=src_port,
            destination_port=dst_port,
            protocol=protocol,
            timestamp=timestamp,
            packet_length=pkt_len
        )


# Pattern generator factory
PATTERN_GENERATORS = {
    'random': RandomTrafficGenerator,
    'web_traffic': WebTrafficGenerator,
    'http_traffic': WebTrafficGenerator,
    'https_traffic': WebTrafficGenerator,
    'dns_traffic': DnsTrafficGenerator,
    'ssh_traffic': SshTrafficGenerator,
    'database_traffic': DatabaseTrafficGenerator,
    'smtp_traffic': SmtpTrafficGenerator,
    'email_traffic': SmtpTrafficGenerator,
    'ftp_traffic': FtpTrafficGenerator,
}


def get_pattern_generator(pattern_type: str, config: Dict[str, Any]) -> PatternGenerator:
    """
    Get pattern generator instance for given pattern type.

    Args:
        pattern_type: Type of traffic pattern
        config: Pattern-specific configuration

    Returns:
        PatternGenerator instance

    Raises:
        ValueError: If pattern type is unknown
    """
    generator_class = PATTERN_GENERATORS.get(pattern_type.lower())
    if generator_class is None:
        raise ValueError(f"Unknown pattern type: {pattern_type}. "
                        f"Available types: {list(PATTERN_GENERATORS.keys())}")

    return generator_class(config)
