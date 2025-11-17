#pragma once

#include <cstdint>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace flowstats {

// Port statistics structure
struct PortStat {
    uint16_t m_port;              // Port number
    uint64_t m_flow_count;        // Total flows involving this port
    uint64_t m_tx_bytes;          // Bytes transmitted FROM this port (as source)
    uint64_t m_rx_bytes;          // Bytes received TO this port (as destination)
    uint64_t m_tx_packets;        // Packets transmitted FROM this port
    uint64_t m_rx_packets;        // Packets received TO this port

    PortStat()
        : m_port(0)
        , m_flow_count(0)
        , m_tx_bytes(0)
        , m_rx_bytes(0)
        , m_tx_packets(0)
        , m_rx_packets(0)
    {}

    explicit PortStat(uint16_t port)
        : m_port(port)
        , m_flow_count(0)
        , m_tx_bytes(0)
        , m_rx_bytes(0)
        , m_tx_packets(0)
        , m_rx_packets(0)
    {}

    // Helper to get total bytes (tx + rx)
    uint64_t total_bytes() const {
        return m_tx_bytes + m_rx_bytes;
    }

    // Helper to get total packets (tx + rx)
    uint64_t total_packets() const {
        return m_tx_packets + m_rx_packets;
    }
};

// Sort field enumeration
enum class PortSortField {
    PORT,
    FLOW_COUNT,
    TX_BYTES,
    RX_BYTES,
    TOTAL_BYTES,
    TX_PACKETS,
    RX_PACKETS,
    TOTAL_PACKETS
};

// Port statistics result
struct PortResult {
    std::map<uint16_t, PortStat> m_port_stats;
    uint64_t m_total_flows;
    uint64_t m_total_bytes;
    uint64_t m_start_ts;
    uint64_t m_end_ts;

    PortResult()
        : m_total_flows(0)
        , m_total_bytes(0)
        , m_start_ts(0)
        , m_end_ts(0)
    {}

    // Get sorted list of port statistics
    std::vector<PortStat> get_sorted(PortSortField field, bool descending = true, size_t top_n = 0) const {
        // Convert map to vector
        std::vector<PortStat> sorted_stats;
        sorted_stats.reserve(m_port_stats.size());
        for (const auto& [port, stat] : m_port_stats) {
            sorted_stats.push_back(stat);
        }

        // Sort based on field
        switch (field) {
            case PortSortField::PORT:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.m_port > b.m_port) : (a.m_port < b.m_port);
                    });
                break;

            case PortSortField::FLOW_COUNT:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.m_flow_count > b.m_flow_count) : (a.m_flow_count < b.m_flow_count);
                    });
                break;

            case PortSortField::TX_BYTES:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.m_tx_bytes > b.m_tx_bytes) : (a.m_tx_bytes < b.m_tx_bytes);
                    });
                break;

            case PortSortField::RX_BYTES:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.m_rx_bytes > b.m_rx_bytes) : (a.m_rx_bytes < b.m_rx_bytes);
                    });
                break;

            case PortSortField::TOTAL_BYTES:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.total_bytes() > b.total_bytes()) : (a.total_bytes() < b.total_bytes());
                    });
                break;

            case PortSortField::TX_PACKETS:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.m_tx_packets > b.m_tx_packets) : (a.m_tx_packets < b.m_tx_packets);
                    });
                break;

            case PortSortField::RX_PACKETS:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.m_rx_packets > b.m_rx_packets) : (a.m_rx_packets < b.m_rx_packets);
                    });
                break;

            case PortSortField::TOTAL_PACKETS:
                std::sort(sorted_stats.begin(), sorted_stats.end(),
                    [descending](const PortStat& a, const PortStat& b) {
                        return descending ? (a.total_packets() > b.total_packets()) : (a.total_packets() < b.total_packets());
                    });
                break;
        }

        // Apply top-N filter if specified
        if (top_n > 0 && sorted_stats.size() > top_n) {
            sorted_stats.resize(top_n);
        }

        return sorted_stats;
    }
};

// Helper function to parse sort field from string
inline PortSortField parse_sort_field(const std::string& field_str) {
    std::string field = field_str;
    std::transform(field.begin(), field.end(), field.begin(), ::tolower);

    if (field == "port") {
        return PortSortField::PORT;
    } else if (field == "flows" || field == "flow_count") {
        return PortSortField::FLOW_COUNT;
    } else if (field == "tx_bytes") {
        return PortSortField::TX_BYTES;
    } else if (field == "rx_bytes") {
        return PortSortField::RX_BYTES;
    } else if (field == "total_bytes" || field == "bytes") {
        return PortSortField::TOTAL_BYTES;
    } else if (field == "tx_packets") {
        return PortSortField::TX_PACKETS;
    } else if (field == "rx_packets") {
        return PortSortField::RX_PACKETS;
    } else if (field == "total_packets" || field == "packets") {
        return PortSortField::TOTAL_PACKETS;
    } else {
        throw std::runtime_error("Invalid sort field: " + field_str +
                                " (valid: port, flows, tx_bytes, rx_bytes, total_bytes, tx_packets, rx_packets, total_packets)");
    }
}

} // namespace flowstats
