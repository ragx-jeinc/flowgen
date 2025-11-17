#pragma once

#include "../core/flowstats_base.h"
#include "../core/output_formatters.h"
#include <flowgen/generator.hpp>
#include <flowgen/utils.hpp>
#include <algorithm>

namespace flowstats {

// Options for flows subcommand
struct FlowsOptions {
    std::string m_config_file;
    size_t m_num_threads;
    size_t m_flows_per_thread;
    uint64_t m_total_flows;
    uint64_t m_start_timestamp_ns;
    uint64_t m_end_timestamp_ns;
    OutputFormat m_output_format;
    bool m_no_header;
    bool m_show_progress;
    ProgressStyle m_progress_style;

    FlowsOptions()
        : m_num_threads(10)
        , m_flows_per_thread(10000)
        , m_total_flows(0)
        , m_start_timestamp_ns(1704067200000000000ULL)  // 2024-01-01
        , m_end_timestamp_ns(0)
        , m_output_format(OutputFormat::TEXT)
        , m_no_header(false)
        , m_show_progress(true)
        , m_progress_style(ProgressStyle::BAR)
    {}
};

// Per-thread buffer for collecting flows
struct ThreadFlowBuffer {
    std::vector<EnhancedFlowRecord> m_flows;

    ThreadFlowBuffer() {
        m_flows.reserve(10000);  // Pre-allocate for efficiency
    }
};

// Flows subcommand - generates and collects flows
class FlowStatsFlows : public FlowStatsCommand<CollectResult> {
private:
    FlowsOptions m_options;
    std::vector<ThreadFlowBuffer> m_thread_buffers;

public:
    explicit FlowStatsFlows(const FlowsOptions& opts)
        : m_options(opts)
    {
        // Copy options to base class members
        m_config_file = opts.m_config_file;
        m_num_threads = opts.m_num_threads;
        m_flows_per_thread = opts.m_flows_per_thread;
        m_show_progress = opts.m_show_progress;
        m_progress_style = opts.m_progress_style;
    }

    bool validate_options() override {
        if (m_options.m_config_file.empty()) {
            std::cerr << "Error: Config file required\n";
            return false;
        }

        if (m_options.m_num_threads == 0 || m_options.m_num_threads > 100) {
            std::cerr << "Error: Invalid thread count (must be 1-100)\n";
            return false;
        }

        if (m_options.m_end_timestamp_ns > 0 &&
            m_options.m_end_timestamp_ns <= m_options.m_start_timestamp_ns) {
            std::cerr << "Error: End timestamp must be greater than start timestamp\n";
            return false;
        }

        return true;
    }

    void initialize() override {
        // Initialize per-thread buffers
        m_thread_buffers.resize(m_num_threads);

        // If end timestamp is specified, calculate flow count
        if (m_options.m_end_timestamp_ns > 0) {
            // Calculate flows based on time range
            uint64_t duration_ns = m_options.m_end_timestamp_ns - m_options.m_start_timestamp_ns;
            double duration_sec = duration_ns / 1e9;

            // Assume 10 Gbps and 800 byte average packet size
            double bandwidth_gbps = 10.0;
            double avg_packet_size = 800.0;
            double flows_per_second = (bandwidth_gbps * 1e9 / 8.0) / avg_packet_size;

            m_options.m_total_flows = static_cast<uint64_t>(duration_sec * flows_per_second);
            m_flows_per_thread = m_options.m_total_flows / m_num_threads;

            std::cerr << "Generating flows for time range: "
                      << m_options.m_start_timestamp_ns << " - "
                      << m_options.m_end_timestamp_ns << " ns\n";
            std::cerr << "Calculated total flows: " << m_options.m_total_flows << "\n";
        } else if (m_options.m_total_flows > 0) {
            // Total flows specified
            m_flows_per_thread = m_options.m_total_flows / m_num_threads;
        }
    }

    void run_worker_thread(size_t thread_id) override {
        try {
            // Create flow generator
            flowgen::FlowGenerator gen;
            flowgen::GeneratorConfig config;

            // Configure generator
            config.max_flows = m_flows_per_thread;
            config.start_timestamp_ns = m_options.m_start_timestamp_ns;

            // Simple default configuration
            config.source_subnets = {"192.168.0.0/16", "10.10.0.0/16"};
            config.destination_subnets = {"10.100.0.0/16", "172.16.0.0/12"};
            config.min_packet_size = 64;
            config.max_packet_size = 1500;
            config.average_packet_size = 800;

            // Add default traffic patterns
            flowgen::GeneratorConfig::TrafficPattern web_pattern;
            web_pattern.type = "web_traffic";
            web_pattern.percentage = 40;
            config.traffic_patterns.push_back(web_pattern);

            flowgen::GeneratorConfig::TrafficPattern dns_pattern;
            dns_pattern.type = "dns_traffic";
            dns_pattern.percentage = 20;
            config.traffic_patterns.push_back(dns_pattern);

            flowgen::GeneratorConfig::TrafficPattern db_pattern;
            db_pattern.type = "database_traffic";
            db_pattern.percentage = 20;
            config.traffic_patterns.push_back(db_pattern);

            flowgen::GeneratorConfig::TrafficPattern random_pattern;
            random_pattern.type = "random";
            random_pattern.percentage = 20;
            config.traffic_patterns.push_back(random_pattern);

            // Bandwidth-based rate (10 Gbps default)
            config.bandwidth_gbps = 10.0;

            gen.initialize(config);

            // Generate flows
            auto& buffer = m_thread_buffers[thread_id];
            auto& thread_data = get_thread_data(thread_id);

            flowgen::FlowRecord flow;
            while (gen.next(flow)) {
                if (is_shutdown_requested()) {
                    break;
                }

                // Enhance flow with statistics
                EnhancedFlowRecord enhanced = enhance_flow(flow, thread_id);

                // Store in thread-local buffer
                buffer.m_flows.push_back(enhanced);

                // Update statistics
                thread_data.m_flows_generated.fetch_add(1, std::memory_order_relaxed);
                thread_data.m_bytes_generated.fetch_add(enhanced.byte_count,
                                                       std::memory_order_relaxed);

                // Update progress (lock-free)
                update_progress(thread_id, flow.timestamp, enhanced.byte_count);
                increment_flow_count(1);
                increment_byte_count(enhanced.byte_count);
            }

            // Signal completion
            thread_data.m_done.store(true, std::memory_order_release);

        } catch (const std::exception& e) {
            std::cerr << "Error in worker thread " << thread_id << ": " << e.what() << "\n";
            auto& thread_data = get_thread_data(thread_id);
            thread_data.m_done.store(true, std::memory_order_release);
        }
    }

    CollectResult collect_results() override {
        CollectResult result;

        // Wait for all threads to complete
        for (auto& data_ptr : m_thread_data) {
            while (!data_ptr->m_done.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Collect from all thread buffers (using move semantics - no copy, no lock)
        for (auto& buffer : m_thread_buffers) {
            result.m_flows.insert(
                result.m_flows.end(),
                std::make_move_iterator(buffer.m_flows.begin()),
                std::make_move_iterator(buffer.m_flows.end())
            );
        }

        // Sort by timestamp
        std::sort(result.m_flows.begin(), result.m_flows.end(),
            [](const EnhancedFlowRecord& a, const EnhancedFlowRecord& b) {
                return a.first_timestamp < b.first_timestamp;
            });

        // Calculate statistics
        result.m_total_flows = result.m_flows.size();
        result.m_total_bytes = 0;
        if (!result.m_flows.empty()) {
            result.m_start_ts = result.m_flows.front().first_timestamp;
            result.m_end_ts = result.m_flows.back().last_timestamp;

            for (const auto& flow : result.m_flows) {
                result.m_total_bytes += flow.byte_count;
            }
        }

        return result;
    }

    void output_results(const CollectResult& results) override {
        auto formatter = create_formatter<CollectResult>(m_options.m_output_format);
        formatter->format(results, std::cout, m_options.m_no_header);
    }

    TimestampRange get_timestamp_range() const override {
        uint64_t end_ts = m_options.m_end_timestamp_ns;
        if (end_ts == 0) {
            // Calculate based on flow count and bandwidth
            double bandwidth_gbps = 10.0;
            double avg_packet_size = 800.0;
            double flows_per_second = (bandwidth_gbps * 1e9 / 8.0) / avg_packet_size;

            uint64_t total_flows = m_options.m_total_flows > 0 ?
                m_options.m_total_flows :
                (m_flows_per_thread * m_num_threads);

            double duration_sec = total_flows / flows_per_second;
            uint64_t duration_ns = static_cast<uint64_t>(duration_sec * 1e9);

            end_ts = m_options.m_start_timestamp_ns + duration_ns;
        }

        return {m_options.m_start_timestamp_ns, end_ts};
    }

private:
    // Enhance basic flow record with statistics
    EnhancedFlowRecord enhance_flow(const flowgen::FlowRecord& basic_flow, size_t thread_id) {
        EnhancedFlowRecord enhanced;

        enhanced.stream_id = thread_id + 1;  // 1-indexed
        enhanced.timestamp = basic_flow.timestamp;
        enhanced.source_ip = basic_flow.source_ip;
        enhanced.destination_ip = basic_flow.destination_ip;
        enhanced.source_port = basic_flow.source_port;
        enhanced.destination_port = basic_flow.destination_port;
        enhanced.protocol = basic_flow.protocol;

        // Generate realistic flow statistics
        FlowStats stats = generate_flow_stats(basic_flow.packet_length,
                                              basic_flow.protocol,
                                              basic_flow.destination_port);

        enhanced.packet_count = stats.packet_count;
        enhanced.byte_count = stats.byte_count;

        // Set first and last packet timestamps
        enhanced.first_timestamp = basic_flow.timestamp;
        enhanced.last_timestamp = basic_flow.timestamp + stats.duration_ns;

        return enhanced;
    }
};

} // namespace flowstats
