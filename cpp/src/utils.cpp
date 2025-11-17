#include "flowgen/utils.hpp"
#include <stdexcept>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace flowgen {
namespace utils {

// Random singleton implementation
Random& Random::instance() {
    static Random inst;
    return inst;
}

Random::Random() {
    auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    gen_.seed(seed);
}

void Random::seed(uint64_t seed_value) {
    gen_.seed(seed_value);
}

int Random::randint(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen_);
}

double Random::uniform(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(gen_);
}

uint32_t Random::rand32() {
    return static_cast<uint32_t>(gen_());
}

// Convert IPv4 string to uint32_t (host byte order)
uint32_t ip_str_to_uint32(const std::string& ip_str) {
    std::istringstream iss(ip_str);
    std::string token;
    std::vector<int> octets;

    while (std::getline(iss, token, '.')) {
        octets.push_back(std::stoi(token));
    }

    if (octets.size() != 4) {
        throw std::runtime_error("Invalid IPv4 address: " + ip_str);
    }

    // Convert to uint32_t (host byte order: big-endian)
    return (static_cast<uint32_t>(octets[0]) << 24) |
           (static_cast<uint32_t>(octets[1]) << 16) |
           (static_cast<uint32_t>(octets[2]) << 8) |
           static_cast<uint32_t>(octets[3]);
}

// Convert uint32_t to IPv4 string (host byte order)
std::string uint32_to_ip_str(uint32_t ip) {
    std::ostringstream oss;
    oss << ((ip >> 24) & 0xFF) << "."
        << ((ip >> 16) & 0xFF) << "."
        << ((ip >> 8) & 0xFF) << "."
        << (ip & 0xFF);
    return oss.str();
}

// Parse subnet and return base IP and host count
std::pair<uint32_t, uint32_t> parse_subnet(const std::string& subnet) {
    size_t slash_pos = subnet.find('/');
    if (slash_pos == std::string::npos) {
        // No prefix length, treat as single host
        uint32_t ip = ip_str_to_uint32(subnet);
        return {ip, 1};
    }

    std::string ip_part = subnet.substr(0, slash_pos);
    int prefix_len = std::stoi(subnet.substr(slash_pos + 1));

    if (prefix_len < 0 || prefix_len > 32) {
        throw std::runtime_error("Invalid prefix length: " + std::to_string(prefix_len));
    }

    uint32_t base_ip = ip_str_to_uint32(ip_part);
    uint32_t host_bits = 32 - prefix_len;
    uint32_t host_count = (host_bits >= 32) ? 0xFFFFFFFF : (1u << host_bits);

    // Mask off host bits to get network address
    uint32_t mask = (prefix_len == 0) ? 0 : (0xFFFFFFFF << host_bits);
    base_ip &= mask;

    return {base_ip, host_count};
}

// Parse CIDR subnet (simplified - just extracts prefix)
static std::pair<std::string, int> parse_cidr(const std::string& subnet) {
    size_t slash_pos = subnet.find('/');
    if (slash_pos == std::string::npos) {
        return {subnet, -1};
    }

    std::string prefix = subnet.substr(0, slash_pos);
    int prefix_len = std::stoi(subnet.substr(slash_pos + 1));
    return {prefix, prefix_len};
}

// Generate random IPv4 as uint32_t from subnet
uint32_t random_ipv4_uint32(const std::string& subnet) {
    auto& rng = Random::instance();

    if (subnet.empty()) {
        // Generate random IP (avoid 0.x.x.x, 224+.x.x.x, x.x.x.0, x.x.x.255)
        uint32_t octet1 = rng.randint(1, 223);
        uint32_t octet2 = rng.randint(0, 255);
        uint32_t octet3 = rng.randint(0, 255);
        uint32_t octet4 = rng.randint(1, 254);
        return (octet1 << 24) | (octet2 << 16) | (octet3 << 8) | octet4;
    }

    // Parse subnet to get base IP and host count
    auto [base_ip, host_count] = parse_subnet(subnet);

    if (host_count <= 2) {
        // Very small subnet, just return base + 1
        return base_ip + 1;
    }

    // Generate random host offset (avoid network address and broadcast)
    uint32_t offset = rng.randint(1, host_count - 2);
    return base_ip + offset;
}

// Simple IPv4 generation from subnet (string version for backward compatibility)
std::string random_ipv4(const std::string& subnet) {
    auto& rng = Random::instance();

    if (subnet.empty()) {
        // Generate random IP
        return std::to_string(rng.randint(1, 223)) + "." +
               std::to_string(rng.randint(0, 255)) + "." +
               std::to_string(rng.randint(0, 255)) + "." +
               std::to_string(rng.randint(1, 254));
    }

    // Parse subnet (simplified implementation)
    auto [prefix, prefix_len] = parse_cidr(subnet);

    // Extract octets from prefix
    std::istringstream iss(prefix);
    std::string token;
    std::vector<int> octets;
    while (std::getline(iss, token, '.')) {
        octets.push_back(std::stoi(token));
    }

    // Generate IP within subnet (simplified - just vary last octet for /24)
    if (prefix_len >= 24 && octets.size() >= 3) {
        return std::to_string(octets[0]) + "." +
               std::to_string(octets[1]) + "." +
               std::to_string(octets[2]) + "." +
               std::to_string(rng.randint(1, 254));
    } else if (prefix_len >= 16 && octets.size() >= 2) {
        return std::to_string(octets[0]) + "." +
               std::to_string(octets[1]) + "." +
               std::to_string(rng.randint(0, 255)) + "." +
               std::to_string(rng.randint(1, 254));
    } else {
        // /8 or less specific
        if (!octets.empty()) {
            return std::to_string(octets[0]) + "." +
                   std::to_string(rng.randint(0, 255)) + "." +
                   std::to_string(rng.randint(0, 255)) + "." +
                   std::to_string(rng.randint(1, 254));
        }
    }

    // Fallback
    return random_ipv4("");
}

std::string random_ipv6(const std::string& subnet) {
    (void)subnet;  // TODO: Implement subnet support for IPv6
    auto& rng = Random::instance();

    // Simplified IPv6 generation
    std::ostringstream oss;
    for (int i = 0; i < 8; ++i) {
        if (i > 0) oss << ":";
        oss << std::hex << rng.randint(0, 65535);
    }
    return oss.str();
}

uint32_t random_ip_from_subnets_uint32(const std::vector<std::string>& subnets,
                                        const std::vector<double>& weights) {
    if (subnets.empty()) {
        return random_ipv4_uint32("");
    }

    std::string subnet;
    if (weights.empty()) {
        // Uniform choice
        int idx = Random::instance().randint(0, subnets.size() - 1);
        subnet = subnets[idx];
    } else {
        // Weighted choice
        subnet = weighted_choice(subnets, weights);
    }

    return random_ipv4_uint32(subnet);
}

std::string random_ip_from_subnets(const std::vector<std::string>& subnets,
                                    const std::vector<double>& weights) {
    if (subnets.empty()) {
        return random_ipv4("");
    }

    std::string subnet;
    if (weights.empty()) {
        // Uniform choice
        int idx = Random::instance().randint(0, subnets.size() - 1);
        subnet = subnets[idx];
    } else {
        // Weighted choice
        subnet = weighted_choice(subnets, weights);
    }

    // Detect IPv4 vs IPv6
    if (subnet.find(':') != std::string::npos) {
        return random_ipv6(subnet);
    } else {
        return random_ipv4(subnet);
    }
}

uint16_t random_port(uint16_t min, uint16_t max) {
    return Random::instance().randint(min, max);
}

uint32_t random_packet_size(uint32_t min, uint32_t max) {
    return Random::instance().randint(min, max);
}

double calculate_flows_per_second(double bandwidth_gbps, uint32_t avg_packet_size) {
    double bandwidth_bps = bandwidth_gbps * 1e9;
    double bandwidth_Bps = bandwidth_bps / 8.0;
    return bandwidth_Bps / avg_packet_size;
}

} // namespace utils
} // namespace flowgen
