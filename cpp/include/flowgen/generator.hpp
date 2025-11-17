#ifndef FLOWGEN_GENERATOR_HPP
#define FLOWGEN_GENERATOR_HPP

#include "flow_record.hpp"
#include "patterns.hpp"
#include <vector>
#include <memory>
#include <string>

namespace flowgen {

/**
 * Configuration for flow generation
 */
struct GeneratorConfig {
    // Rate configuration
    double bandwidth_gbps = 0.0;
    double flows_per_second = 0.0;

    // Stop conditions
    uint64_t max_flows = 0;
    double duration_seconds = 0.0;

    // Timestamp (nanoseconds since Unix epoch)
    uint64_t start_timestamp_ns = 0;

    // Network configuration
    std::vector<std::string> source_subnets;
    std::vector<std::string> destination_subnets;
    std::vector<double> source_weights;

    // Packet configuration
    uint32_t min_packet_size = 64;
    uint32_t max_packet_size = 1500;
    uint32_t average_packet_size = 800;

    // Bidirectional mode configuration
    std::string bidirectional_mode = "none";  // "none" or "random"
    double bidirectional_probability = 0.5;    // 0.0 to 1.0

    // Traffic patterns
    struct TrafficPattern {
        std::string type;
        double percentage;
    };
    std::vector<TrafficPattern> traffic_patterns;

    /**
     * Validate configuration
     */
    bool validate(std::string* error = nullptr) const;
};

/**
 * Main flow generator class
 */
class FlowGenerator {
public:
    FlowGenerator();
    ~FlowGenerator();

    // Non-copyable and non-movable (contains unique_ptr members)
    FlowGenerator(const FlowGenerator&) = delete;
    FlowGenerator& operator=(const FlowGenerator&) = delete;
    FlowGenerator(FlowGenerator&&) = delete;
    FlowGenerator& operator=(FlowGenerator&&) = delete;

    /**
     * Initialize generator with configuration
     */
    bool initialize(const GeneratorConfig& config);

    /**
     * Generate next flow record
     *
     * @return Flow record, or empty optional if generation complete
     */
    bool next(FlowRecord& flow);

    /**
     * Check if generation is complete
     */
    bool is_done() const;

    /**
     * Reset generator to initial state
     */
    void reset();

    /**
     * Get current statistics
     */
    struct Stats {
        uint64_t flows_generated;
        double elapsed_time_seconds;
        double flows_per_second;
        uint64_t current_timestamp_ns;
    };
    Stats get_stats() const;

    /**
     * Get flows generated count
     */
    uint64_t flow_count() const { return flow_count_; }

    /**
     * Get current timestamp in nanoseconds
     */
    uint64_t current_timestamp_ns() const { return current_timestamp_ns_; }

private:
    bool initialized_;
    GeneratorConfig config_;

    std::vector<std::unique_ptr<PatternGenerator>> pattern_generators_;
    std::vector<double> pattern_weights_;

    double flows_per_second_;
    uint64_t inter_arrival_time_ns_;  // nanoseconds between flows
    uint64_t start_timestamp_ns_;
    uint64_t current_timestamp_ns_;
    uint64_t flow_count_;

    bool should_stop() const;
    PatternGenerator* select_pattern();
};

} // namespace flowgen

#endif // FLOWGEN_GENERATOR_HPP
