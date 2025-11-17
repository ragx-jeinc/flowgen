#ifndef FLOWGEN_FLOW_RECORD_HPP
#define FLOWGEN_FLOW_RECORD_HPP

#include <string>
#include <cstdint>

namespace flowgen {

/**
 * Network flow record containing 5-tuple and metadata
 */
struct FlowRecord {
    uint32_t source_ip;      // IPv4 address in host byte order
    uint32_t destination_ip;  // IPv4 address in host byte order
    uint16_t source_port;
    uint16_t destination_port;
    uint8_t protocol;
    uint64_t timestamp;    // Unix epoch timestamp in nanoseconds
    uint32_t packet_length; // Packet size in bytes

    FlowRecord() = default;

    FlowRecord(uint32_t src_ip, uint32_t dst_ip,
               uint16_t src_port, uint16_t dst_port,
               uint8_t proto, uint64_t ts, uint32_t pkt_len)
        : source_ip(src_ip), destination_ip(dst_ip),
          source_port(src_port), destination_port(dst_port),
          protocol(proto), timestamp(ts), packet_length(pkt_len) {}

    // Convenience constructor with string IPs
    FlowRecord(const std::string& src_ip, const std::string& dst_ip,
               uint16_t src_port, uint16_t dst_port,
               uint8_t proto, uint64_t ts, uint32_t pkt_len);

    /**
     * Get source IP as string
     */
    std::string source_ip_str() const;

    /**
     * Get destination IP as string
     */
    std::string destination_ip_str() const;

    /**
     * Convert to CSV string representation
     */
    std::string to_csv() const;

    /**
     * Get CSV header
     */
    static std::string csv_header();
};

} // namespace flowgen

#endif // FLOWGEN_FLOW_RECORD_HPP
