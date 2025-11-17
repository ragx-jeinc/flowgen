#include "flowgen/generator.hpp"
#include "flowgen/utils.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>

namespace flowgen {

// GeneratorConfig validation
bool GeneratorConfig::validate(std::string* error) const {
    // Check rate configuration
    if (bandwidth_gbps <= 0.0 && flows_per_second <= 0.0) {
        if (error) *error = "Must specify either bandwidth_gbps or flows_per_second";
        return false;
    }

    // Check stop conditions
    if (max_flows == 0 && duration_seconds <= 0.0) {
        if (error) *error = "Must specify at least one of: max_flows, duration_seconds";
        return false;
    }

    // Check traffic patterns
    if (traffic_patterns.empty()) {
        if (error) *error = "Must specify at least one traffic pattern";
        return false;
    }

    double total_percentage = 0.0;
    for (const auto& pattern : traffic_patterns) {
        total_percentage += pattern.percentage;
    }

    if (std::abs(total_percentage - 100.0) > 0.01) {
        if (error) {
            *error = "Traffic pattern percentages must sum to 100, got " +
                     std::to_string(total_percentage);
        }
        return false;
    }

    // Check network configuration
    if (source_subnets.empty()) {
        if (error) *error = "source_subnets cannot be empty";
        return false;
    }

    if (destination_subnets.empty()) {
        if (error) *error = "destination_subnets cannot be empty";
        return false;
    }

    // Check source weights if provided
    if (!source_weights.empty() && source_weights.size() != source_subnets.size()) {
        if (error) *error = "source_weights size must match source_subnets size";
        return false;
    }

    if (!source_weights.empty()) {
        double weight_sum = 0.0;
        for (double w : source_weights) {
            weight_sum += w;
        }
        if (std::abs(weight_sum - 100.0) > 0.01) {
            if (error) *error = "source_weights must sum to 100";
            return false;
        }
    }

    // Check packet configuration
    if (min_packet_size > max_packet_size) {
        if (error) *error = "min_packet_size cannot exceed max_packet_size";
        return false;
    }

    // Check bidirectional mode
    if (bidirectional_mode != "none" && bidirectional_mode != "random") {
        if (error) *error = "bidirectional_mode must be 'none' or 'random'";
        return false;
    }

    // Check bidirectional probability
    if (bidirectional_probability < 0.0 || bidirectional_probability > 1.0) {
        if (error) *error = "bidirectional_probability must be between 0.0 and 1.0";
        return false;
    }

    return true;
}

// FlowGenerator implementation
FlowGenerator::FlowGenerator()
    : initialized_(false),
      flows_per_second_(0.0),
      inter_arrival_time_ns_(0),
      start_timestamp_ns_(0),
      current_timestamp_ns_(0),
      flow_count_(0) {
}

FlowGenerator::~FlowGenerator() = default;

bool FlowGenerator::initialize(const GeneratorConfig& config) {
    // Validate configuration
    std::string error;
    if (!config.validate(&error)) {
        // In a real implementation, we'd log or throw the error
        return false;
    }

    config_ = config;

    // Calculate flow rate
    if (config_.bandwidth_gbps > 0.0) {
        flows_per_second_ = utils::calculate_flows_per_second(
            config_.bandwidth_gbps,
            config_.average_packet_size
        );
    } else {
        flows_per_second_ = config_.flows_per_second;
    }

    // Calculate inter-arrival time in nanoseconds
    double inter_arrival_sec = 1.0 / flows_per_second_;
    inter_arrival_time_ns_ = static_cast<uint64_t>(inter_arrival_sec * 1e9);

    // Set start timestamp in nanoseconds
    if (config_.start_timestamp_ns > 0) {
        start_timestamp_ns_ = config_.start_timestamp_ns;
    } else {
        // Get current time in nanoseconds
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        start_timestamp_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }

    current_timestamp_ns_ = start_timestamp_ns_;
    flow_count_ = 0;

    // Initialize pattern generators
    pattern_generators_.clear();
    pattern_weights_.clear();

    for (const auto& pattern_config : config_.traffic_patterns) {
        auto generator = create_pattern_generator(pattern_config.type);
        pattern_generators_.push_back(std::move(generator));
        pattern_weights_.push_back(pattern_config.percentage);
    }

    initialized_ = true;
    return true;
}

bool FlowGenerator::next(FlowRecord& flow) {
    if (!initialized_) {
        return false;
    }

    if (should_stop()) {
        return false;
    }

    // Select pattern based on weights
    PatternGenerator* pattern = select_pattern();
    if (!pattern) {
        return false;
    }

    // Generate flow record
    flow = pattern->generate(
        current_timestamp_ns_,
        config_.source_subnets,
        config_.destination_subnets,
        config_.source_weights,
        config_.min_packet_size,
        config_.max_packet_size
    );

    // Apply bidirectional mode - randomly swap source and destination
    if (config_.bidirectional_mode == "random") {
        double r = utils::Random::instance().uniform(0.0, 1.0);
        if (r < config_.bidirectional_probability) {
            // Swap source and destination IPs and ports
            std::swap(flow.source_ip, flow.destination_ip);
            std::swap(flow.source_port, flow.destination_port);
        }
    }

    // Update state
    flow_count_++;
    current_timestamp_ns_ += inter_arrival_time_ns_;

    return true;
}

bool FlowGenerator::is_done() const {
    if (!initialized_) {
        return true;
    }
    return should_stop();
}

void FlowGenerator::reset() {
    if (initialized_) {
        current_timestamp_ns_ = start_timestamp_ns_;
        flow_count_ = 0;
    }
}

FlowGenerator::Stats FlowGenerator::get_stats() const {
    Stats stats;
    stats.flows_generated = flow_count_;
    // Convert nanoseconds to seconds for elapsed time
    stats.elapsed_time_seconds = (current_timestamp_ns_ - start_timestamp_ns_) / 1e9;
    stats.flows_per_second = flows_per_second_;
    stats.current_timestamp_ns = current_timestamp_ns_;
    return stats;
}

bool FlowGenerator::should_stop() const {
    // Check flow count limit
    if (config_.max_flows > 0 && flow_count_ >= config_.max_flows) {
        return true;
    }

    // Check duration limit
    if (config_.duration_seconds > 0.0) {
        double elapsed = (current_timestamp_ns_ - start_timestamp_ns_) / 1e9;
        if (elapsed >= config_.duration_seconds) {
            return true;
        }
    }

    return false;
}

PatternGenerator* FlowGenerator::select_pattern() {
    if (pattern_generators_.empty()) {
        return nullptr;
    }

    // Use weighted choice from utils
    size_t idx = 0;
    double r = utils::Random::instance().uniform(0.0, 100.0);
    double cumsum = 0.0;

    for (size_t i = 0; i < pattern_weights_.size(); ++i) {
        cumsum += pattern_weights_[i];
        if (r <= cumsum) {
            idx = i;
            break;
        }
    }

    return pattern_generators_[idx].get();
}

} // namespace flowgen
