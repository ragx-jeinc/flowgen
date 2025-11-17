#include "flowgen/flow_record.hpp"
#include "flowgen/utils.hpp"
#include <sstream>
#include <iomanip>

namespace flowgen {

// Convenience constructor with string IPs
FlowRecord::FlowRecord(const std::string& src_ip, const std::string& dst_ip,
                       uint16_t src_port, uint16_t dst_port,
                       uint8_t proto, uint64_t ts, uint32_t pkt_len)
    : source_ip(utils::ip_str_to_uint32(src_ip)),
      destination_ip(utils::ip_str_to_uint32(dst_ip)),
      source_port(src_port),
      destination_port(dst_port),
      protocol(proto),
      timestamp(ts),
      packet_length(pkt_len) {}

std::string FlowRecord::source_ip_str() const {
    return utils::uint32_to_ip_str(source_ip);
}

std::string FlowRecord::destination_ip_str() const {
    return utils::uint32_to_ip_str(destination_ip);
}

std::string FlowRecord::to_csv() const {
    std::ostringstream oss;
    oss << timestamp << ","  // nanoseconds as integer
        << utils::uint32_to_ip_str(source_ip) << ","
        << utils::uint32_to_ip_str(destination_ip) << ","
        << source_port << ","
        << destination_port << ","
        << static_cast<int>(protocol) << ","
        << packet_length;
    return oss.str();
}

std::string FlowRecord::csv_header() {
    return "timestamp,src_ip,dst_ip,src_port,dst_port,protocol,length";
}

} // namespace flowgen
