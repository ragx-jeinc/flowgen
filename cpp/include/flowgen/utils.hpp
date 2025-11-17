#ifndef FLOWGEN_UTILS_HPP
#define FLOWGEN_UTILS_HPP

#include <string>
#include <vector>
#include <random>
#include <cstdint>
#include <stdexcept>

namespace flowgen {
namespace utils {

/**
 * Random number generator singleton
 */
class Random {
public:
    static Random& instance();

    void seed(uint64_t seed_value);

    int randint(int min, int max);
    double uniform(double min = 0.0, double max = 1.0);
    uint32_t rand32();

private:
    Random();
    std::mt19937_64 gen_;
};

/**
 * Convert IPv4 string to uint32_t (host byte order)
 *
 * @param ip_str IPv4 address string (e.g., "192.168.1.1")
 * @return IPv4 address as uint32_t
 */
uint32_t ip_str_to_uint32(const std::string& ip_str);

/**
 * Convert uint32_t to IPv4 string (host byte order)
 *
 * @param ip IPv4 address as uint32_t
 * @return IPv4 address string
 */
std::string uint32_to_ip_str(uint32_t ip);

/**
 * Parse subnet and return base IP and host count
 *
 * @param subnet CIDR subnet (e.g., "192.168.1.0/24")
 * @return Pair of (base_ip, host_count)
 */
std::pair<uint32_t, uint32_t> parse_subnet(const std::string& subnet);

/**
 * Generate random IPv4 address as uint32_t
 *
 * @param subnet CIDR subnet (e.g., "192.168.1.0/24"), empty for random
 * @return IPv4 address as uint32_t
 */
uint32_t random_ipv4_uint32(const std::string& subnet = "");

/**
 * Generate random IPv4 address
 *
 * @param subnet CIDR subnet (e.g., "192.168.1.0/24"), empty for random
 * @return IPv4 address string
 */
std::string random_ipv4(const std::string& subnet = "");

/**
 * Generate random IPv6 address
 *
 * @param subnet CIDR subnet, empty for random
 * @return IPv6 address string
 */
std::string random_ipv6(const std::string& subnet = "");

/**
 * Generate random IP from list of subnets (uint32_t)
 *
 * @param subnets List of CIDR subnets
 * @param weights Optional weights (must sum to ~100 or ~1.0)
 * @return Random IPv4 address as uint32_t
 */
uint32_t random_ip_from_subnets_uint32(const std::vector<std::string>& subnets,
                                        const std::vector<double>& weights = {});

/**
 * Generate random IP from list of subnets
 *
 * @param subnets List of CIDR subnets
 * @param weights Optional weights (must sum to ~100 or ~1.0)
 * @return Random IP address
 */
std::string random_ip_from_subnets(const std::vector<std::string>& subnets,
                                    const std::vector<double>& weights = {});

/**
 * Generate random port number
 *
 * @param min Minimum port
 * @param max Maximum port
 * @return Port number
 */
uint16_t random_port(uint16_t min = 1024, uint16_t max = 65535);

/**
 * Generate random packet size
 *
 * @param min Minimum size in bytes
 * @param max Maximum size in bytes
 * @return Packet size
 */
uint32_t random_packet_size(uint32_t min, uint32_t max);

/**
 * Calculate flows per second from bandwidth
 *
 * @param bandwidth_gbps Bandwidth in Gbps
 * @param avg_packet_size Average packet size in bytes
 * @return Flows per second
 */
double calculate_flows_per_second(double bandwidth_gbps, uint32_t avg_packet_size);

/**
 * Weighted random selection
 */
template<typename T>
T weighted_choice(const std::vector<T>& items, const std::vector<double>& weights) {
    if (items.empty()) {
        throw std::runtime_error("Cannot choose from empty items");
    }

    if (!weights.empty() && weights.size() != items.size()) {
        throw std::runtime_error("Weights size must match items size");
    }

    if (weights.empty()) {
        // Uniform choice
        int idx = Random::instance().randint(0, items.size() - 1);
        return items[idx];
    }

    // Weighted choice
    double total = 0.0;
    for (double w : weights) {
        total += w;
    }

    double r = Random::instance().uniform(0.0, total);
    double cumsum = 0.0;

    for (size_t i = 0; i < items.size(); ++i) {
        cumsum += weights[i];
        if (r <= cumsum) {
            return items[i];
        }
    }

    return items.back();
}

} // namespace utils
} // namespace flowgen

#endif // FLOWGEN_UTILS_HPP
