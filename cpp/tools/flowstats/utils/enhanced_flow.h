#pragma once

#include <cstdint>
#include <string>

namespace flowstats {

/**
 * Enhanced flow record with stream ID and aggregated statistics
 */
struct EnhancedFlowRecord {
    uint32_t stream_id;        // Generator thread ID
    uint64_t timestamp;        // Nanoseconds since Unix epoch (first packet) - for chunking/sorting
    uint64_t first_timestamp;  // First packet timestamp (ns since epoch)
    uint64_t last_timestamp;   // Last packet timestamp (ns since epoch)
    uint32_t source_ip;        // IPv4 in host byte order
    uint32_t destination_ip;   // IPv4 in host byte order
    uint16_t source_port;
    uint16_t destination_port;
    uint8_t  protocol;
    uint32_t packet_count;     // Number of packets in flow
    uint64_t byte_count;       // Total bytes in flow

    EnhancedFlowRecord() = default;

    /**
     * Convert IP uint32_t to string
     */
    static std::string ip_to_string(uint32_t ip);

    /**
     * Get source IP as string
     */
    std::string source_ip_str() const;

    /**
     * Get destination IP as string
     */
    std::string destination_ip_str() const;

    /**
     * Format as plain text
     */
    std::string to_plain_text(bool include_header = false) const;

    /**
     * Format as CSV
     */
    std::string to_csv() const;

    /**
     * Format as JSON
     */
    std::string to_json(bool pretty = false, bool last = false) const;

    /**
     * Get CSV header
     */
    static std::string csv_header();

    /**
     * Get plain text header
     */
    static std::string plain_text_header();
};

/**
 * Flow statistics for realistic packet/byte count generation
 */
struct FlowStats {
    uint32_t packet_count;
    uint64_t byte_count;
    uint64_t duration_ns;  // Flow duration in nanoseconds
};

/**
 * Generate realistic flow statistics based on protocol and port
 */
FlowStats generate_flow_stats(uint32_t avg_packet_size,
                              uint8_t protocol,
                              uint16_t dst_port);

} // namespace flowstats
