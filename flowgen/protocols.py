"""
Protocol constants and well-known port mappings.
"""

from typing import Dict, List


# IP Protocol Numbers (IANA)
class Protocol:
    """IP protocol number constants"""
    ICMP = 1
    TCP = 6
    UDP = 17
    ICMPV6 = 58
    SCTP = 132


# Protocol name to number mapping
PROTOCOL_MAP: Dict[str, int] = {
    'icmp': Protocol.ICMP,
    'tcp': Protocol.TCP,
    'udp': Protocol.UDP,
    'icmpv6': Protocol.ICMPV6,
    'sctp': Protocol.SCTP,
}


# Well-known ports (0-1023)
class WellKnownPorts:
    """Common well-known port numbers"""
    FTP_DATA = 20
    FTP_CONTROL = 21
    SSH = 22
    TELNET = 23
    SMTP = 25
    DNS = 53
    HTTP = 80
    POP3 = 110
    IMAP = 143
    SNMP = 161
    HTTPS = 443
    SMTPS = 465
    IMAPS = 993
    POP3S = 995


# Service to port/protocol mapping
SERVICE_PORTS: Dict[str, Dict[str, any]] = {
    'http': {
        'port': 80,
        'protocol': Protocol.TCP,
        'alt_ports': [8080, 8000],
    },
    'https': {
        'port': 443,
        'protocol': Protocol.TCP,
        'alt_ports': [8443],
    },
    'dns': {
        'port': 53,
        'protocol': Protocol.UDP,
        'alt_protocols': [Protocol.TCP],
    },
    'ssh': {
        'port': 22,
        'protocol': Protocol.TCP,
    },
    'ftp': {
        'port': 21,
        'protocol': Protocol.TCP,
        'data_port': 20,
    },
    'smtp': {
        'port': 25,
        'protocol': Protocol.TCP,
        'alt_ports': [587, 465],
    },
    'mysql': {
        'port': 3306,
        'protocol': Protocol.TCP,
    },
    'postgresql': {
        'port': 5432,
        'protocol': Protocol.TCP,
    },
    'mongodb': {
        'port': 27017,
        'protocol': Protocol.TCP,
    },
    'redis': {
        'port': 6379,
        'protocol': Protocol.TCP,
    },
    'telnet': {
        'port': 23,
        'protocol': Protocol.TCP,
    },
    'pop3': {
        'port': 110,
        'protocol': Protocol.TCP,
    },
    'imap': {
        'port': 143,
        'protocol': Protocol.TCP,
    },
    'snmp': {
        'port': 161,
        'protocol': Protocol.UDP,
    },
    'ntp': {
        'port': 123,
        'protocol': Protocol.UDP,
    },
}


# Port ranges
PORT_RANGE_WELL_KNOWN = (0, 1023)
PORT_RANGE_REGISTERED = (1024, 49151)
PORT_RANGE_DYNAMIC = (49152, 65535)


def get_service_info(service_name: str) -> Dict:
    """
    Get port and protocol information for a service.

    Args:
        service_name: Name of the service (e.g., 'http', 'dns')

    Returns:
        Dictionary with service information

    Raises:
        KeyError: If service name is not found
    """
    return SERVICE_PORTS[service_name.lower()]


def get_protocol_number(protocol_name: str) -> int:
    """
    Get protocol number from protocol name.

    Args:
        protocol_name: Protocol name (e.g., 'tcp', 'udp')

    Returns:
        Protocol number

    Raises:
        KeyError: If protocol name is not found
    """
    return PROTOCOL_MAP[protocol_name.lower()]


def get_protocol_name(protocol_number: int) -> str:
    """
    Get protocol name from protocol number.

    Args:
        protocol_number: IP protocol number

    Returns:
        Protocol name or 'unknown'
    """
    for name, num in PROTOCOL_MAP.items():
        if num == protocol_number:
            return name.upper()
    return f'unknown({protocol_number})'
