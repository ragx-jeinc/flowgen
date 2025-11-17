#ifndef FLOWGEN_PATTERNS_HPP
#define FLOWGEN_PATTERNS_HPP

#include "flow_record.hpp"
#include <string>
#include <vector>
#include <memory>

namespace flowgen {

/**
 * Base class for traffic pattern generators
 */
class PatternGenerator {
public:
    virtual ~PatternGenerator() = default;

    /**
     * Generate a single flow record
     */
    virtual FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) = 0;

    /**
     * Get pattern type name
     */
    virtual std::string type() const = 0;
};

/**
 * Random traffic generator
 */
class RandomPattern : public PatternGenerator {
public:
    FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) override;

    std::string type() const override { return "random"; }
};

/**
 * Web (HTTP/HTTPS) traffic generator
 */
class WebPattern : public PatternGenerator {
public:
    FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) override;

    std::string type() const override { return "web_traffic"; }
};

/**
 * DNS traffic generator
 */
class DnsPattern : public PatternGenerator {
public:
    FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) override;

    std::string type() const override { return "dns_traffic"; }
};

/**
 * SSH traffic generator
 */
class SshPattern : public PatternGenerator {
public:
    FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) override;

    std::string type() const override { return "ssh_traffic"; }
};

/**
 * Database traffic generator
 */
class DatabasePattern : public PatternGenerator {
public:
    FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) override;

    std::string type() const override { return "database_traffic"; }
};

/**
 * SMTP traffic generator
 */
class SmtpPattern : public PatternGenerator {
public:
    FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) override;

    std::string type() const override { return "smtp_traffic"; }
};

/**
 * FTP traffic generator
 */
class FtpPattern : public PatternGenerator {
public:
    FlowRecord generate(
        uint64_t timestamp_ns,
        const std::vector<std::string>& src_subnets,
        const std::vector<std::string>& dst_subnets,
        const std::vector<double>& src_weights,
        uint32_t min_pkt_size,
        uint32_t max_pkt_size
    ) override;

    std::string type() const override { return "ftp_traffic"; }
};

/**
 * Factory function to create pattern generators
 */
std::unique_ptr<PatternGenerator> create_pattern_generator(const std::string& pattern_type);

} // namespace flowgen

#endif // FLOWGEN_PATTERNS_HPP
