"""
Flow record data models for network traffic generation.
"""

from dataclasses import dataclass, asdict
from typing import Dict


@dataclass
class FlowRecord:
    """
    Network flow record containing 5-tuple and metadata.

    Attributes:
        source_ip: Source IP address (IPv4 or IPv6)
        destination_ip: Destination IP address (IPv4 or IPv6)
        source_port: Source port number (0-65535)
        destination_port: Destination port number (0-65535)
        protocol: IP protocol number (6=TCP, 17=UDP, 1=ICMP, etc.)
        timestamp: Unix epoch timestamp (float, seconds)
        packet_length: Packet size in bytes
    """
    source_ip: str
    destination_ip: str
    source_port: int
    destination_port: int
    protocol: int
    timestamp: float
    packet_length: int

    def __post_init__(self):
        """Validate field values"""
        # Validate ports
        if not (0 <= self.source_port <= 65535):
            raise ValueError(f"Invalid source_port: {self.source_port}")
        if not (0 <= self.destination_port <= 65535):
            raise ValueError(f"Invalid destination_port: {self.destination_port}")

        # Validate protocol
        if not (0 <= self.protocol <= 255):
            raise ValueError(f"Invalid protocol: {self.protocol}")

        # Validate packet length
        if self.packet_length < 0:
            raise ValueError(f"Invalid packet_length: {self.packet_length}")

        # Validate timestamp
        if self.timestamp < 0:
            raise ValueError(f"Invalid timestamp: {self.timestamp}")

    def to_dict(self) -> Dict:
        """
        Convert flow record to dictionary.

        Returns:
            Dictionary representation of the flow record
        """
        return {
            'src_ip': self.source_ip,
            'dst_ip': self.destination_ip,
            'src_port': self.source_port,
            'dst_port': self.destination_port,
            'protocol': self.protocol,
            'timestamp': self.timestamp,
            'length': self.packet_length
        }

    def to_csv_row(self) -> str:
        """
        Convert flow record to CSV format.

        Returns:
            Comma-separated string representation
        """
        return (f"{self.timestamp},{self.source_ip},{self.destination_ip},"
                f"{self.source_port},{self.destination_port},"
                f"{self.protocol},{self.packet_length}")

    @staticmethod
    def csv_header() -> str:
        """
        Get CSV header row.

        Returns:
            CSV header string
        """
        return "timestamp,src_ip,dst_ip,src_port,dst_port,protocol,length"

    def __repr__(self) -> str:
        """String representation of flow record"""
        return (f"FlowRecord({self.source_ip}:{self.source_port} -> "
                f"{self.destination_ip}:{self.destination_port}, "
                f"proto={self.protocol}, len={self.packet_length}B, "
                f"ts={self.timestamp})")
