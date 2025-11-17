#!/usr/bin/env python3
"""
Basic usage example for FlowGen library.

This example demonstrates:
- Loading configuration from YAML file
- Generating flows using iterator pattern
- Exporting flows to CSV
"""

from flowgen import FlowGenerator, export_csv
import time

def main():
    # Create generator
    generator = FlowGenerator()

    # Initialize from config file
    config_path = "configs/example_config.yaml"
    print(f"Initializing generator from {config_path}...")

    if not generator.initialize(config_path):
        print("Failed to initialize generator")
        return

    print(f"Generator initialized successfully")
    print(f"Target rate: {generator.get_stats().flows_per_second:.2f} flows/sec")
    print()

    # Generate flows and export to CSV
    print("Generating flows...")
    start_time = time.time()

    flows = []
    count = 0

    for flow in generator:
        flows.append(flow)
        count += 1

        # Print progress every 10K flows
        if count % 10000 == 0:
            print(f"Generated {count:,} flows...")

    end_time = time.time()
    elapsed = end_time - start_time

    # Print statistics
    stats = generator.get_stats()
    print(f"\nGeneration complete!")
    print(f"Total flows: {stats.flows_generated:,}")
    print(f"Elapsed time: {elapsed:.2f} seconds")
    print(f"Actual generation rate: {stats.flows_generated / elapsed:.2f} flows/sec")
    print(f"Timestamp range: {stats.elapsed_time_seconds:.2f} seconds")
    print()

    # Export to CSV
    output_file = "output_flows.csv"
    print(f"Exporting to {output_file}...")
    export_csv(flows, output_file)
    print(f"Export complete!")

    # Show a few example flows
    print(f"\nFirst 5 flows:")
    for i, flow in enumerate(flows[:5]):
        print(f"  {i+1}. {flow}")

if __name__ == "__main__":
    main()
