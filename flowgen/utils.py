"""
Utility functions for generating network flow data.
"""

import random
import ipaddress
from typing import Tuple, List, Optional
import numpy as np


def random_ipv4(subnet: Optional[str] = None) -> str:
    """
    Generate a random IPv4 address.

    Args:
        subnet: CIDR notation subnet (e.g., '192.168.1.0/24')
                If None, generates completely random IP

    Returns:
        IPv4 address string
    """
    if subnet:
        network = ipaddress.IPv4Network(subnet, strict=False)
        # Get random address from subnet
        num_addresses = network.num_addresses
        # Avoid network and broadcast addresses for most subnets
        if num_addresses > 2:
            random_offset = random.randint(1, num_addresses - 2)
        else:
            random_offset = random.randint(0, num_addresses - 1)
        return str(network.network_address + random_offset)
    else:
        # Generate completely random IP
        # Avoid reserved ranges in simple implementation
        return f"{random.randint(1, 223)}.{random.randint(0, 255)}." \
               f"{random.randint(0, 255)}.{random.randint(1, 254)}"


def random_ipv6(subnet: Optional[str] = None) -> str:
    """
    Generate a random IPv6 address.

    Args:
        subnet: CIDR notation subnet (e.g., '2001:db8::/32')
                If None, generates completely random IP

    Returns:
        IPv6 address string
    """
    if subnet:
        network = ipaddress.IPv6Network(subnet, strict=False)
        # Get random address from subnet
        num_addresses = network.num_addresses
        random_offset = random.randint(0, min(num_addresses - 1, 2**64))  # Limit for performance
        return str(network.network_address + random_offset)
    else:
        # Generate random IPv6
        return ':'.join(f'{random.randint(0, 65535):x}' for _ in range(8))


def random_port(port_range: str = 'dynamic') -> int:
    """
    Generate a random port number.

    Args:
        port_range: Port range type - 'well_known', 'registered', 'dynamic', or 'any'

    Returns:
        Port number
    """
    ranges = {
        'well_known': (0, 1023),
        'registered': (1024, 49151),
        'dynamic': (49152, 65535),
        'any': (1, 65535),
    }

    start, end = ranges.get(port_range, ranges['dynamic'])
    return random.randint(start, end)


def random_packet_size(min_size: int = 64, max_size: int = 1500) -> int:
    """
    Generate a random packet size.

    Args:
        min_size: Minimum packet size in bytes
        max_size: Maximum packet size in bytes

    Returns:
        Packet size in bytes
    """
    return random.randint(min_size, max_size)


def random_packet_size_realistic(distribution: str = 'bimodal') -> int:
    """
    Generate realistic packet size based on distribution.

    Args:
        distribution: Distribution type - 'bimodal', 'uniform', 'normal'

    Returns:
        Packet size in bytes
    """
    if distribution == 'bimodal':
        # 60% small packets, 40% large packets (realistic internet traffic)
        if random.random() < 0.6:
            return random.randint(40, 100)  # Small packets (ACKs, control)
        else:
            return random.randint(500, 1500)  # Large packets (data)

    elif distribution == 'normal':
        # Normal distribution around 800 bytes
        size = int(np.random.normal(800, 300))
        return max(64, min(1500, size))  # Clamp to valid range

    else:  # uniform
        return random.randint(64, 1500)


def weighted_random_choice(items: List, weights: List[float]):
    """
    Select random item based on weights.

    Args:
        items: List of items to choose from
        weights: List of weights (should sum to ~1.0 or ~100)

    Returns:
        Randomly selected item
    """
    return random.choices(items, weights=weights, k=1)[0]


def subnet_random_ip(subnets: List[str], weights: Optional[List[float]] = None) -> str:
    """
    Generate random IP from list of subnets with optional weights.

    Args:
        subnets: List of subnet CIDR strings
        weights: Optional weights for each subnet

    Returns:
        Random IP address string
    """
    if weights:
        subnet = weighted_random_choice(subnets, weights)
    else:
        subnet = random.choice(subnets)

    # Detect IPv4 vs IPv6
    if ':' in subnet:
        return random_ipv6(subnet)
    else:
        return random_ipv4(subnet)


def parse_subnet(subnet_str: str) -> ipaddress.IPv4Network | ipaddress.IPv6Network:
    """
    Parse subnet string into network object.

    Args:
        subnet_str: CIDR notation subnet string

    Returns:
        IPv4Network or IPv6Network object

    Raises:
        ValueError: If subnet string is invalid
    """
    try:
        return ipaddress.ip_network(subnet_str, strict=False)
    except ValueError as e:
        raise ValueError(f"Invalid subnet '{subnet_str}': {e}")


def calculate_flows_per_second(bandwidth_gbps: float, avg_packet_size: int) -> float:
    """
    Calculate flows per second based on bandwidth and average packet size.

    Args:
        bandwidth_gbps: Target bandwidth in Gbps
        avg_packet_size: Average packet size in bytes

    Returns:
        Flows per second
    """
    bandwidth_bps = bandwidth_gbps * 1_000_000_000  # Convert to bits per second
    bandwidth_Bps = bandwidth_bps / 8  # Convert to bytes per second
    flows_per_second = bandwidth_Bps / avg_packet_size
    return flows_per_second


def generate_timestamps(start_time: float, count: int, rate: float,
                       distribution: str = 'constant') -> np.ndarray:
    """
    Generate array of timestamps based on flow rate.

    Args:
        start_time: Starting timestamp (Unix epoch)
        count: Number of timestamps to generate
        rate: Flows per second
        distribution: 'constant', 'poisson', or 'uniform'

    Returns:
        NumPy array of timestamps
    """
    if distribution == 'poisson':
        # Exponential inter-arrival times (Poisson process)
        inter_arrivals = np.random.exponential(1.0 / rate, count)
        timestamps = start_time + np.cumsum(inter_arrivals)

    elif distribution == 'uniform':
        # Uniform random inter-arrival times
        inter_arrivals = np.random.uniform(0, 2.0 / rate, count)
        timestamps = start_time + np.cumsum(inter_arrivals)

    else:  # constant
        # Constant inter-arrival time
        inter_arrival = 1.0 / rate
        timestamps = start_time + np.arange(count) * inter_arrival

    return timestamps
