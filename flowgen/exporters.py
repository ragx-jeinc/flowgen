"""
Export flow records to various formats.
"""

import json
from typing import List, Iterator, Union
from pathlib import Path

from .models import FlowRecord


class FlowExporter:
    """Export flow records to different formats"""

    @staticmethod
    def export_to_csv(flows: Union[List[FlowRecord], Iterator[FlowRecord]],
                      filename: str,
                      include_header: bool = True) -> int:
        """
        Export flows to CSV file.

        Args:
            flows: List or iterator of FlowRecord objects
            filename: Output CSV filename
            include_header: Whether to include CSV header row

        Returns:
            Number of flows written
        """
        count = 0
        with open(filename, 'w') as f:
            if include_header:
                # Handle both Python and C++ FlowRecord
                try:
                    header = FlowRecord.csv_header()
                except:
                    # Try C++ FlowRecord
                    from . import _flowgen_core
                    header = _flowgen_core.FlowRecord.csv_header()
                f.write(header + '\n')

            for flow in flows:
                # Handle both Python FlowRecord.to_csv_row() and C++ FlowRecord.to_csv()
                if hasattr(flow, 'to_csv'):
                    f.write(flow.to_csv() + '\n')
                elif hasattr(flow, 'to_csv_row'):
                    f.write(flow.to_csv_row() + '\n')
                else:
                    raise ValueError("FlowRecord must have to_csv() or to_csv_row() method")
                count += 1

        return count

    @staticmethod
    def export_to_json(flows: Union[List[FlowRecord], Iterator[FlowRecord]],
                       filename: str,
                       pretty: bool = True) -> int:
        """
        Export flows to JSON file.

        Args:
            flows: List or iterator of FlowRecord objects
            filename: Output JSON filename
            pretty: Whether to use pretty formatting

        Returns:
            Number of flows written
        """
        flow_list = []

        # Convert to list of dicts
        for flow in flows:
            flow_list.append(flow.to_dict())

        # Write to file
        with open(filename, 'w') as f:
            if pretty:
                json.dump(flow_list, f, indent=2)
            else:
                json.dump(flow_list, f)

        return len(flow_list)

    @staticmethod
    def export_to_jsonlines(flows: Union[List[FlowRecord], Iterator[FlowRecord]],
                           filename: str) -> int:
        """
        Export flows to JSON Lines format (one JSON object per line).
        More efficient for streaming large datasets.

        Args:
            flows: List or iterator of FlowRecord objects
            filename: Output file name

        Returns:
            Number of flows written
        """
        count = 0
        with open(filename, 'w') as f:
            for flow in flows:
                f.write(json.dumps(flow.to_dict()) + '\n')
                count += 1

        return count

    @staticmethod
    def stream_to_csv(flows: Iterator[FlowRecord],
                     filename: str,
                     batch_size: int = 1000,
                     include_header: bool = True) -> int:
        """
        Stream flows to CSV file in batches (memory efficient).

        Args:
            flows: Iterator of FlowRecord objects
            filename: Output CSV filename
            batch_size: Number of flows to buffer before writing
            include_header: Whether to include CSV header row

        Returns:
            Number of flows written
        """
        count = 0
        buffer = []

        with open(filename, 'w') as f:
            if include_header:
                f.write(FlowRecord.csv_header() + '\n')

            for flow in flows:
                buffer.append(flow.to_csv_row())
                count += 1

                if len(buffer) >= batch_size:
                    f.write('\n'.join(buffer) + '\n')
                    buffer.clear()

            # Write remaining buffered flows
            if buffer:
                f.write('\n'.join(buffer) + '\n')

        return count


def export_csv(flows: Union[List[FlowRecord], Iterator[FlowRecord]],
               filename: str,
               include_header: bool = True) -> int:
    """
    Convenience function to export flows to CSV.

    Args:
        flows: List or iterator of FlowRecord objects
        filename: Output CSV filename
        include_header: Whether to include CSV header row

    Returns:
        Number of flows written
    """
    return FlowExporter.export_to_csv(flows, filename, include_header)


def export_json(flows: Union[List[FlowRecord], Iterator[FlowRecord]],
                filename: str,
                pretty: bool = True) -> int:
    """
    Convenience function to export flows to JSON.

    Args:
        flows: List or iterator of FlowRecord objects
        filename: Output JSON filename
        pretty: Whether to use pretty formatting

    Returns:
        Number of flows written
    """
    return FlowExporter.export_to_json(flows, filename, pretty)


def export_jsonlines(flows: Union[List[FlowRecord], Iterator[FlowRecord]],
                    filename: str) -> int:
    """
    Convenience function to export flows to JSON Lines format.

    Args:
        flows: List or iterator of FlowRecord objects
        filename: Output filename

    Returns:
        Number of flows written
    """
    return FlowExporter.export_to_jsonlines(flows, filename)
