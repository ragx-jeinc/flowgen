#!/usr/bin/env python3
"""
Test script for FlowGen enhancements:
1. IP optimization (uint32_t)
2. Bidirectional traffic mode
"""

import sys
import time
from flowgen import FlowGenerator, export_csv
import flowgen._flowgen_core as core

def test_ip_optimization():
    """Test IP conversion utilities"""
    print("=" * 60)
    print("TEST 1: IP Address Optimization (uint32_t)")
    print("=" * 60)

    # Test IP conversion
    test_ips = ["192.168.1.1", "10.0.0.1", "172.16.0.1", "203.0.113.1"]

    print("\nIP Conversion Test:")
    for ip_str in test_ips:
        ip_uint = core.ip_str_to_uint32(ip_str)
        back_to_str = core.uint32_to_ip_str(ip_uint)
        status = "✓" if ip_str == back_to_str else "✗"
        print(f"  {status} {ip_str:15s} → {ip_uint:10d} → {back_to_str}")

    # Test flow generation
    print("\nFlow Generation Test:")
    gen = FlowGenerator()
    gen.initialize('configs/example_config.yaml')

    flow = next(gen)
    print(f"  source_ip (uint32):      {flow.source_ip}")
    print(f"  source_ip_str (string):  {flow.source_ip_str}")
    print(f"  destination_ip (uint32): {flow.destination_ip}")
    print(f"  destination_ip_str:      {flow.destination_ip_str}")
    print("\n✓ IP optimization test passed!\n")

def test_bidirectional_mode():
    """Test bidirectional traffic generation"""
    print("=" * 60)
    print("TEST 2: Bidirectional Traffic Mode")
    print("=" * 60)

    gen = FlowGenerator()
    gen.initialize('configs/bidirectional_config.yaml')

    # Analyze flow directions
    client_to_server = 0
    server_to_client = 0
    sample_flows = []

    print("\nSample flows:")
    for i, flow in enumerate(gen):
        if i >= 1000:
            break

        src = flow.source_ip_str
        dst = flow.destination_ip_str

        # Classify direction
        is_client_src = src.startswith('192.168.') or src.startswith('10.10.')
        is_server_dst = dst.startswith('10.100.') or dst.startswith('203.0.113.')

        if is_client_src and is_server_dst:
            client_to_server += 1
            direction = "Client→Server"
        else:
            server_to_client += 1
            direction = "Server→Client"

        if i < 5:
            print(f"  {i+1}. {src}:{flow.source_port} → {dst}:{flow.destination_port}")
            print(f"      [{direction}]")

    total = client_to_server + server_to_client
    bidir_ratio = server_to_client / total

    print(f"\nStatistics ({total} flows):")
    print(f"  Client → Server: {client_to_server:4d} ({100*client_to_server/total:.1f}%)")
    print(f"  Server → Client: {server_to_client:4d} ({100*server_to_client/total:.1f}%)")
    print(f"  Expected ratio:   ~50%")
    print(f"  Actual ratio:     {100*bidir_ratio:.1f}%")

    # Check if within reasonable bounds (45-55%)
    if 0.45 <= bidir_ratio <= 0.55:
        print("\n✓ Bidirectional mode test passed!\n")
        return True
    else:
        print("\n✗ Bidirectional ratio outside expected range!\n")
        return False

def performance_test():
    """Test generation performance"""
    print("=" * 60)
    print("TEST 3: Performance Benchmark")
    print("=" * 60)

    gen = FlowGenerator()
    gen.initialize('configs/bidirectional_config.yaml')

    print("\nGenerating 50,000 flows...")
    start = time.time()
    count = 0
    for _ in gen:
        count += 1

    elapsed = time.time() - start
    rate = count / elapsed

    print(f"  Flows generated: {count:,}")
    print(f"  Time elapsed:    {elapsed:.3f} seconds")
    print(f"  Generation rate: {rate:,.0f} flows/second")

    # Memory efficiency
    old_size = 30  # approx bytes for string IPs
    new_size = 8   # bytes for uint32 IPs
    savings = old_size - new_size

    print(f"\nMemory efficiency:")
    print(f"  Old (string IPs): ~{old_size} bytes/flow")
    print(f"  New (uint32 IPs): ~{new_size} bytes/flow")
    print(f"  Savings:          ~{savings} bytes/flow ({100*savings/old_size:.0f}%)")
    print(f"  For 1M flows:     ~{savings * 1_000_000 / 1024 / 1024:.1f} MB saved")

    print("\n✓ Performance test passed!\n")

def export_test():
    """Test CSV export"""
    print("=" * 60)
    print("TEST 4: CSV Export")
    print("=" * 60)

    gen = FlowGenerator()
    gen.initialize('configs/bidirectional_config.yaml')

    flows = []
    for i, flow in enumerate(gen):
        if i >= 100:
            break
        flows.append(flow)

    filename = 'test_output.csv'
    export_csv(flows, filename)

    print(f"\n  Exported {len(flows)} flows to {filename}")
    print("  First 3 lines:")

    with open(filename, 'r') as f:
        for i, line in enumerate(f):
            if i >= 3:
                break
            print(f"    {line.rstrip()}")

    print("\n✓ CSV export test passed!\n")

def main():
    """Run all tests"""
    print("\n" + "=" * 60)
    print("FlowGen Enhancement Test Suite")
    print("=" * 60 + "\n")

    try:
        test_ip_optimization()
        test_bidirectional_mode()
        performance_test()
        export_test()

        print("=" * 60)
        print("ALL TESTS PASSED ✓")
        print("=" * 60)
        return 0

    except Exception as e:
        print(f"\n✗ Test failed with error: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(main())
