#!/usr/bin/env python3
"""
Streaming usage example - process flows one at a time without storing in memory.

This example demonstrates:
- Memory-efficient flow generation
- Real-time processing
- Streaming export to file
"""

from flowgen import FlowGenerator
import time

def process_flow(flow):
    """
    Process a single flow record.

    In a real application, this might:
    - Send flow to network analyzer
    - Write to database
    - Apply filters or transformations
    """
    # For this example, just accumulate statistics
    pass

def main():
    # Create generator
    generator = FlowGenerator()

    # Initialize
    config_path = "configs/high_bandwidth.yaml"
    print(f"Initializing generator from {config_path}...")

    if not generator.initialize(config_path):
        print("Failed to initialize generator")
        return

    stats = generator.get_stats()
    print(f"Generator initialized")
    print(f"Target rate: {stats.flows_per_second:,.0f} flows/sec")
    print(f"Target bandwidth: 40 Gbps")
    print()

    # Stream flows directly to file (memory efficient)
    output_file = "streaming_output.csv"
    print(f"Streaming flows to {output_file}...")

    start_time = time.time()
    count = 0

    with open(output_file, 'w') as f:
        # Write header
        f.write("timestamp,src_ip,dst_ip,src_port,dst_port,protocol,length\n")

        # Process flows one at a time
        for flow in generator:
            # Process the flow
            process_flow(flow)

            # Write to file
            f.write(flow.to_csv() + '\n')

            count += 1

            # Print progress
            if count % 100000 == 0:
                elapsed = time.time() - start_time
                rate = count / elapsed if elapsed > 0 else 0
                print(f"Processed {count:,} flows ({rate:,.0f} flows/sec)...")

    end_time = time.time()
    elapsed = end_time - start_time

    # Final statistics
    final_stats = generator.get_stats()
    print(f"\nStreaming complete!")
    print(f"Total flows: {final_stats.flows_generated:,}")
    print(f"Wall-clock time: {elapsed:.2f} seconds")
    print(f"Generation rate: {final_stats.flows_generated / elapsed:,.0f} flows/sec")
    print(f"Simulated time span: {final_stats.elapsed_time_seconds:.3f} seconds")
    print(f"Time compression: {final_stats.elapsed_time_seconds / elapsed:.2e}x")

if __name__ == "__main__":
    main()
