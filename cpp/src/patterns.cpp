#include "flowgen/patterns.hpp"
#include "flowgen/utils.hpp"
#include <stdexcept>
#include <algorithm>

namespace flowgen {

// Protocol constants
constexpr uint8_t PROTO_ICMP = 1;
constexpr uint8_t PROTO_TCP = 6;
constexpr uint8_t PROTO_UDP = 17;

// Random pattern
FlowRecord RandomPattern::generate(
    uint64_t timestamp_ns,
    const std::vector<std::string>& src_subnets,
    const std::vector<std::string>& dst_subnets,
    const std::vector<double>& src_weights,
    uint32_t min_pkt_size,
    uint32_t max_pkt_size) {

    uint32_t src_ip = utils::random_ip_from_subnets_uint32(src_subnets, src_weights);
    uint32_t dst_ip = utils::random_ip_from_subnets_uint32(dst_subnets);

    uint8_t proto = (utils::Random::instance().uniform() < 0.7) ? PROTO_TCP : PROTO_UDP;
    uint16_t src_port = utils::random_port(49152, 65535);
    uint16_t dst_port = utils::random_port(1, 65535);
    uint32_t pkt_len = utils::random_packet_size(min_pkt_size, max_pkt_size);

    return FlowRecord(src_ip, dst_ip, src_port, dst_port, proto, timestamp_ns, pkt_len);
}

// Web pattern
FlowRecord WebPattern::generate(
    uint64_t timestamp_ns,
    const std::vector<std::string>& src_subnets,
    const std::vector<std::string>& dst_subnets,
    const std::vector<double>& src_weights,
    uint32_t min_pkt_size,
    uint32_t max_pkt_size) {

    uint32_t src_ip = utils::random_ip_from_subnets_uint32(src_subnets, src_weights);
    uint32_t dst_ip = utils::random_ip_from_subnets_uint32(dst_subnets);

    // 70% HTTPS, 30% HTTP
    uint16_t dst_port = (utils::Random::instance().uniform() < 0.7) ? 443 : 80;
    uint16_t src_port = utils::random_port(49152, 65535);
    uint8_t proto = PROTO_TCP;

    // Bimodal distribution - 40% small packets, 60% large
    uint32_t pkt_len;
    if (utils::Random::instance().uniform() < 0.4) {
        pkt_len = utils::random_packet_size(64, 200);
    } else {
        pkt_len = utils::random_packet_size(500, max_pkt_size);
    }

    return FlowRecord(src_ip, dst_ip, src_port, dst_port, proto, timestamp_ns, pkt_len);
}

// DNS pattern
FlowRecord DnsPattern::generate(
    uint64_t timestamp_ns,
    const std::vector<std::string>& src_subnets,
    const std::vector<std::string>& dst_subnets,
    const std::vector<double>& src_weights,
    uint32_t min_pkt_size,
    uint32_t max_pkt_size) {

    uint32_t src_ip = utils::random_ip_from_subnets_uint32(src_subnets, src_weights);
    uint32_t dst_ip = utils::random_ip_from_subnets_uint32(dst_subnets);

    uint16_t dst_port = 53;
    uint16_t src_port = utils::random_port(49152, 65535);
    uint8_t proto = PROTO_UDP;

    // DNS packets are typically small
    uint32_t pkt_len = utils::random_packet_size(64, 512);

    return FlowRecord(src_ip, dst_ip, src_port, dst_port, proto, timestamp_ns, pkt_len);
}

// SSH pattern
FlowRecord SshPattern::generate(
    uint64_t timestamp_ns,
    const std::vector<std::string>& src_subnets,
    const std::vector<std::string>& dst_subnets,
    const std::vector<double>& src_weights,
    uint32_t min_pkt_size,
    uint32_t max_pkt_size) {

    uint32_t src_ip = utils::random_ip_from_subnets_uint32(src_subnets, src_weights);
    uint32_t dst_ip = utils::random_ip_from_subnets_uint32(dst_subnets);

    uint16_t dst_port = 22;
    uint16_t src_port = utils::random_port(49152, 65535);
    uint8_t proto = PROTO_TCP;

    // SSH packets are small and consistent
    uint32_t pkt_len = utils::random_packet_size(100, 400);

    return FlowRecord(src_ip, dst_ip, src_port, dst_port, proto, timestamp_ns, pkt_len);
}

// Database pattern
FlowRecord DatabasePattern::generate(
    uint64_t timestamp_ns,
    const std::vector<std::string>& src_subnets,
    const std::vector<std::string>& dst_subnets,
    const std::vector<double>& src_weights,
    uint32_t min_pkt_size,
    uint32_t max_pkt_size) {

    uint32_t src_ip = utils::random_ip_from_subnets_uint32(src_subnets, src_weights);
    uint32_t dst_ip = utils::random_ip_from_subnets_uint32(dst_subnets);

    // Random database port
    const uint16_t db_ports[] = {3306, 5432, 27017, 6379};
    int idx = utils::Random::instance().randint(0, 3);
    uint16_t dst_port = db_ports[idx];

    uint16_t src_port = utils::random_port(49152, 65535);
    uint8_t proto = PROTO_TCP;

    // 30% small queries, 70% large result sets
    uint32_t pkt_len;
    if (utils::Random::instance().uniform() < 0.3) {
        pkt_len = utils::random_packet_size(64, 300);
    } else {
        pkt_len = utils::random_packet_size(500, max_pkt_size);
    }

    return FlowRecord(src_ip, dst_ip, src_port, dst_port, proto, timestamp_ns, pkt_len);
}

// SMTP pattern
FlowRecord SmtpPattern::generate(
    uint64_t timestamp_ns,
    const std::vector<std::string>& src_subnets,
    const std::vector<std::string>& dst_subnets,
    const std::vector<double>& src_weights,
    uint32_t min_pkt_size,
    uint32_t max_pkt_size) {

    uint32_t src_ip = utils::random_ip_from_subnets_uint32(src_subnets, src_weights);
    uint32_t dst_ip = utils::random_ip_from_subnets_uint32(dst_subnets);

    const uint16_t smtp_ports[] = {25, 587, 465};
    int idx = utils::Random::instance().randint(0, 2);
    uint16_t dst_port = smtp_ports[idx];

    uint16_t src_port = utils::random_port(49152, 65535);
    uint8_t proto = PROTO_TCP;

    uint32_t pkt_len = utils::random_packet_size(200, max_pkt_size);

    return FlowRecord(src_ip, dst_ip, src_port, dst_port, proto, timestamp_ns, pkt_len);
}

// FTP pattern
FlowRecord FtpPattern::generate(
    uint64_t timestamp_ns,
    const std::vector<std::string>& src_subnets,
    const std::vector<std::string>& dst_subnets,
    const std::vector<double>& src_weights,
    uint32_t min_pkt_size,
    uint32_t max_pkt_size) {

    uint32_t src_ip = utils::random_ip_from_subnets_uint32(src_subnets, src_weights);
    uint32_t dst_ip = utils::random_ip_from_subnets_uint32(dst_subnets);

    uint16_t dst_port = (utils::Random::instance().uniform() < 0.5) ? 20 : 21;
    uint16_t src_port = utils::random_port(49152, 65535);
    uint8_t proto = PROTO_TCP;

    // Port 20 (data) - large packets, port 21 (control) - small packets
    uint32_t pkt_len;
    if (dst_port == 20) {
        pkt_len = utils::random_packet_size(1000, max_pkt_size);
    } else {
        pkt_len = utils::random_packet_size(64, 500);
    }

    return FlowRecord(src_ip, dst_ip, src_port, dst_port, proto, timestamp_ns, pkt_len);
}

// Factory function
std::unique_ptr<PatternGenerator> create_pattern_generator(const std::string& pattern_type) {
    std::string type_lower = pattern_type;
    std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(), ::tolower);

    if (type_lower == "random") {
        return std::make_unique<RandomPattern>();
    } else if (type_lower == "web_traffic" || type_lower == "http_traffic" || type_lower == "https_traffic") {
        return std::make_unique<WebPattern>();
    } else if (type_lower == "dns_traffic") {
        return std::make_unique<DnsPattern>();
    } else if (type_lower == "ssh_traffic") {
        return std::make_unique<SshPattern>();
    } else if (type_lower == "database_traffic") {
        return std::make_unique<DatabasePattern>();
    } else if (type_lower == "smtp_traffic" || type_lower == "email_traffic") {
        return std::make_unique<SmtpPattern>();
    } else if (type_lower == "ftp_traffic") {
        return std::make_unique<FtpPattern>();
    } else {
        throw std::runtime_error("Unknown pattern type: " + pattern_type);
    }
}

} // namespace flowgen
