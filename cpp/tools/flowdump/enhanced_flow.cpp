#include "enhanced_flow.hpp"
#include <flowgen/utils.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace flowdump {

std::string EnhancedFlowRecord::ip_to_string(uint32_t ip) {
    return flowgen::utils::uint32_to_ip_str(ip);
}

std::string EnhancedFlowRecord::source_ip_str() const {
    return ip_to_string(source_ip);
}

std::string EnhancedFlowRecord::destination_ip_str() const {
    return ip_to_string(destination_ip);
}

std::string EnhancedFlowRecord::plain_text_header() {
    std::ostringstream oss;
    oss << std::left
        << std::setw(10) << "STREAM"
        << std::setw(22) << "FIRST_TIMESTAMP"
        << std::setw(22) << "LAST_TIMESTAMP"
        << std::setw(18) << "SRC_IP"
        << std::setw(10) << "SRC_PORT"
        << std::setw(18) << "DST_IP"
        << std::setw(10) << "DST_PORT"
        << std::setw(7) << "PROTO"
        << std::setw(10) << "PACKETS"
        << std::setw(12) << "BYTES";
    return oss.str();
}

std::string EnhancedFlowRecord::to_plain_text(bool include_header) const {
    std::ostringstream oss;

    if (include_header) {
        oss << plain_text_header() << "\n";
    }

    // Format first timestamp as seconds.nanoseconds
    uint64_t first_seconds = first_timestamp / 1000000000ULL;
    uint64_t first_nanoseconds = first_timestamp % 1000000000ULL;

    // Format last timestamp as seconds.nanoseconds
    uint64_t last_seconds = last_timestamp / 1000000000ULL;
    uint64_t last_nanoseconds = last_timestamp % 1000000000ULL;

    // Format stream ID
    oss << "0x" << std::setw(8) << std::setfill('0') << std::hex << stream_id << std::dec << "  ";

    // Format first timestamp
    oss << std::setfill(' ') << std::setw(12) << first_seconds << "."
        << std::setw(9) << std::setfill('0') << first_nanoseconds << "  ";

    // Format last timestamp
    oss << std::setfill(' ') << std::setw(12) << last_seconds << "."
        << std::setw(9) << std::setfill('0') << last_nanoseconds << "  ";

    // Format rest of fields
    oss << std::setfill(' ') << std::left
        << std::setw(18) << source_ip_str()
        << std::setw(10) << source_port
        << std::setw(18) << destination_ip_str()
        << std::setw(10) << destination_port
        << std::setw(7) << static_cast<int>(protocol)
        << std::setw(10) << packet_count
        << std::setw(12) << byte_count;

    return oss.str();
}

std::string EnhancedFlowRecord::csv_header() {
    return "stream_id,first_timestamp,last_timestamp,src_ip,dst_ip,src_port,dst_port,protocol,packet_count,byte_count";
}

std::string EnhancedFlowRecord::to_csv() const {
    std::ostringstream oss;
    oss << stream_id << ","
        << first_timestamp << ","
        << last_timestamp << ","
        << source_ip_str() << ","
        << destination_ip_str() << ","
        << source_port << ","
        << destination_port << ","
        << static_cast<int>(protocol) << ","
        << packet_count << ","
        << byte_count;
    return oss.str();
}

std::string EnhancedFlowRecord::to_json(bool pretty, bool last) const {
    std::ostringstream oss;

    if (pretty) {
        oss << "  {\n"
            << "    \"stream_id\": " << stream_id << ",\n"
            << "    \"first_timestamp\": " << first_timestamp << ",\n"
            << "    \"last_timestamp\": " << last_timestamp << ",\n"
            << "    \"src_ip\": \"" << source_ip_str() << "\",\n"
            << "    \"dst_ip\": \"" << destination_ip_str() << "\",\n"
            << "    \"src_port\": " << source_port << ",\n"
            << "    \"dst_port\": " << destination_port << ",\n"
            << "    \"protocol\": " << static_cast<int>(protocol) << ",\n"
            << "    \"packet_count\": " << packet_count << ",\n"
            << "    \"byte_count\": " << byte_count << "\n"
            << "  }" << (last ? "" : ",") << "\n";
    } else {
        oss << "{"
            << "\"stream_id\":" << stream_id << ","
            << "\"first_timestamp\":" << first_timestamp << ","
            << "\"last_timestamp\":" << last_timestamp << ","
            << "\"src_ip\":\"" << source_ip_str() << "\","
            << "\"dst_ip\":\"" << destination_ip_str() << "\","
            << "\"src_port\":" << source_port << ","
            << "\"dst_port\":" << destination_port << ","
            << "\"protocol\":" << static_cast<int>(protocol) << ","
            << "\"packet_count\":" << packet_count << ","
            << "\"byte_count\":" << byte_count
            << "}" << (last ? "" : ",");
    }

    return oss.str();
}

FlowStats generate_flow_stats(uint32_t avg_packet_size,
                               uint8_t protocol,
                               uint16_t dst_port) {
    FlowStats stats;
    auto& rng = flowgen::utils::Random::instance();

    // Generate realistic packet count based on protocol and port
    if (protocol == 6) {  // TCP
        if (dst_port == 80 || dst_port == 443) {  // HTTP/HTTPS
            // Typical web flow: 10-50 packets
            stats.packet_count = rng.randint(10, 50);
        } else if (dst_port == 22) {  // SSH
            // SSH sessions: longer flows
            stats.packet_count = rng.randint(100, 500);
        } else if (dst_port == 3306 || dst_port == 5432 ||
                   dst_port == 27017 || dst_port == 6379) {  // Databases
            // Database queries: variable
            stats.packet_count = rng.randint(5, 100);
        } else if (dst_port == 25 || dst_port == 587 || dst_port == 465) {  // SMTP
            // Email: moderate size
            stats.packet_count = rng.randint(10, 50);
        } else {
            // Generic TCP
            stats.packet_count = rng.randint(5, 100);
        }
    } else if (protocol == 17) {  // UDP
        if (dst_port == 53) {  // DNS
            // DNS: typically 2 packets (query + response)
            stats.packet_count = 2;
        } else {
            // Generic UDP
            stats.packet_count = rng.randint(1, 20);
        }
    } else {
        stats.packet_count = rng.randint(1, 10);
    }

    // Calculate byte count with variance
    stats.byte_count = 0;
    for (uint32_t i = 0; i < stats.packet_count; ++i) {
        // Vary packet size ±20%
        int32_t variance = static_cast<int32_t>(avg_packet_size) / 5;
        int32_t offset = rng.randint(-variance, variance);
        int32_t pkt_size = static_cast<int32_t>(avg_packet_size) + offset;
        pkt_size = std::max(64, std::min(1500, pkt_size));  // Clamp to valid range
        stats.byte_count += static_cast<uint64_t>(pkt_size);
    }

    // Calculate flow duration based on packet count and protocol
    // Inter-packet timing varies by protocol
    if (stats.packet_count == 1) {
        // Single packet flow, zero duration
        stats.duration_ns = 0;
    } else if (protocol == 6) {  // TCP
        if (dst_port == 80 || dst_port == 443) {  // HTTP/HTTPS
            // Web flows: 10-100ms per packet (RTT + processing)
            uint64_t inter_packet_time_us = rng.randint(10000, 100000);  // 10-100ms
            stats.duration_ns = (stats.packet_count - 1) * inter_packet_time_us * 1000;
        } else if (dst_port == 22) {  // SSH
            // SSH: Interactive, faster inter-packet (1-50ms)
            uint64_t inter_packet_time_us = rng.randint(1000, 50000);  // 1-50ms
            stats.duration_ns = (stats.packet_count - 1) * inter_packet_time_us * 1000;
        } else if (dst_port == 3306 || dst_port == 5432 ||
                   dst_port == 27017 || dst_port == 6379) {  // Databases
            // Database: Fast queries (1-20ms per packet)
            uint64_t inter_packet_time_us = rng.randint(1000, 20000);  // 1-20ms
            stats.duration_ns = (stats.packet_count - 1) * inter_packet_time_us * 1000;
        } else {
            // Generic TCP: 5-50ms per packet
            uint64_t inter_packet_time_us = rng.randint(5000, 50000);  // 5-50ms
            stats.duration_ns = (stats.packet_count - 1) * inter_packet_time_us * 1000;
        }
    } else if (protocol == 17) {  // UDP
        if (dst_port == 53) {  // DNS
            // DNS: Quick query/response (1-50ms total)
            stats.duration_ns = rng.randint(1000000, 50000000);  // 1-50ms
        } else {
            // Generic UDP: Fast (0.1-10ms per packet)
            uint64_t inter_packet_time_us = rng.randint(100, 10000);  // 0.1-10ms
            stats.duration_ns = (stats.packet_count - 1) * inter_packet_time_us * 1000;
        }
    } else {
        // Other protocols: Moderate timing (1-20ms per packet)
        uint64_t inter_packet_time_us = rng.randint(1000, 20000);  // 1-20ms
        stats.duration_ns = (stats.packet_count - 1) * inter_packet_time_us * 1000;
    }

    return stats;
}

} // namespace flowdump
