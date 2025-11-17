#pragma once

#include "../core/flowstats_base.h"
#include "../utils/port_stat.h"
#include <flowgen/generator.hpp>
#include <flowgen/utils.hpp>
#include <algorithm>
#include <map>
#include <mutex>

namespace flowstats {

// Options for port subcommand
struct PortOptions {
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
    PortSortField m_sort_field;
    bool m_sort_descending;
    size_t m_top_n;

    PortOptions()
        : m_num_threads(10)
        , m_flows_per_thread(10000)
        , m_total_flows(0)
        , m_start_timestamp_ns(1704067200000000000ULL)  // 2024-01-01
        , m_end_timestamp_ns(0)
        , m_output_format(OutputFormat::TEXT)
        , m_no_header(false)
        , m_show_progress(true)
        , m_progress_style(ProgressStyle::BAR)
        , m_sort_field(PortSortField::TOTAL_BYTES)
        , m_sort_descending(true)
        , m_top_n(0)  // 0 means no limit
    {}
};

// Per-thread port statistics buffer
struct ThreadPortBuffer {
    std::map<uint16_t, PortStat> m_port_stats;
    uint64_t m_start_ts;
    uint64_t m_end_ts;

    ThreadPortBuffer()
        : m_start_ts(UINT64_MAX)
        , m_end_ts(0)
    {}
};

// Port subcommand - aggregates port statistics
class FlowStatsPort : public FlowStatsCommand<PortResult> {
private:
    PortOptions m_options;
    std::vector<ThreadPortBuffer> m_thread_buffers;

public:
    explicit FlowStatsPort(const PortOptions& opts)
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

            // Generate flows and aggregate port statistics
            auto& buffer = m_thread_buffers[thread_id];
            auto& thread_data = get_thread_data(thread_id);

            flowgen::FlowRecord flow;
            while (gen.next(flow)) {
                if (is_shutdown_requested()) {
                    break;
                }

                // Generate flow statistics
                FlowStats stats = generate_flow_stats(flow.packet_length,
                                                      flow.protocol,
                                                      flow.destination_port);

                // Track timestamp range
                if (flow.timestamp < buffer.m_start_ts) {
                    buffer.m_start_ts = flow.timestamp;
                }
                uint64_t last_ts = flow.timestamp + stats.duration_ns;
                if (last_ts > buffer.m_end_ts) {
                    buffer.m_end_ts = last_ts;
                }

                // Aggregate source port statistics (tx)
                auto& src_stat = buffer.m_port_stats[flow.source_port];
                if (src_stat.m_port == 0) {
                    src_stat.m_port = flow.source_port;
                }
                src_stat.m_flow_count++;
                src_stat.m_tx_bytes += stats.byte_count;
                src_stat.m_tx_packets += stats.packet_count;

                // Aggregate destination port statistics (rx)
                auto& dst_stat = buffer.m_port_stats[flow.destination_port];
                if (dst_stat.m_port == 0) {
                    dst_stat.m_port = flow.destination_port;
                }
                // Note: Don't increment flow_count again if src==dst port (rare but possible)
                if (flow.source_port != flow.destination_port) {
                    dst_stat.m_flow_count++;
                }
                dst_stat.m_rx_bytes += stats.byte_count;
                dst_stat.m_rx_packets += stats.packet_count;

                // Update statistics
                thread_data.m_flows_generated.fetch_add(1, std::memory_order_relaxed);
                thread_data.m_bytes_generated.fetch_add(stats.byte_count,
                                                       std::memory_order_relaxed);

                // Update progress (lock-free)
                update_progress(thread_id, flow.timestamp, stats.byte_count);
                increment_flow_count(1);
                increment_byte_count(stats.byte_count);
            }

            // Signal completion
            thread_data.m_done.store(true, std::memory_order_release);

        } catch (const std::exception& e) {
            std::cerr << "Error in worker thread " << thread_id << ": " << e.what() << "\n";
            auto& thread_data = get_thread_data(thread_id);
            thread_data.m_done.store(true, std::memory_order_release);
        }
    }

    PortResult collect_results() override {
        PortResult result;

        // Wait for all threads to complete
        for (auto& data_ptr : m_thread_data) {
            while (!data_ptr->m_done.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Merge port statistics from all thread buffers
        result.m_start_ts = UINT64_MAX;
        result.m_end_ts = 0;

        for (auto& buffer : m_thread_buffers) {
            // Update timestamp range
            if (buffer.m_start_ts < result.m_start_ts) {
                result.m_start_ts = buffer.m_start_ts;
            }
            if (buffer.m_end_ts > result.m_end_ts) {
                result.m_end_ts = buffer.m_end_ts;
            }

            // Merge port statistics
            for (const auto& [port, stat] : buffer.m_port_stats) {
                auto& merged_stat = result.m_port_stats[port];
                if (merged_stat.m_port == 0) {
                    merged_stat.m_port = port;
                }
                merged_stat.m_flow_count += stat.m_flow_count;
                merged_stat.m_tx_bytes += stat.m_tx_bytes;
                merged_stat.m_rx_bytes += stat.m_rx_bytes;
                merged_stat.m_tx_packets += stat.m_tx_packets;
                merged_stat.m_rx_packets += stat.m_rx_packets;
            }
        }

        // Calculate totals
        result.m_total_flows = m_total_flows.load();
        result.m_total_bytes = m_total_bytes.load();

        return result;
    }

    void output_results(const PortResult& results) override {
        auto formatter = create_formatter<PortResult>(m_options.m_output_format);

        // Create a modified result with sorting and top-N applied
        PortResult sorted_result = results;

        // Get sorted list
        auto sorted_stats = results.get_sorted(m_options.m_sort_field,
                                               m_options.m_sort_descending,
                                               m_options.m_top_n);

        // Rebuild map with sorted/filtered data
        sorted_result.m_port_stats.clear();
        for (const auto& stat : sorted_stats) {
            sorted_result.m_port_stats[stat.m_port] = stat;
        }

        formatter->format(sorted_result, std::cout, m_options.m_no_header);
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
};

} // namespace flowstats
