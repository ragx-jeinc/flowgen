#include "enhanced_flow.hpp"
#include "thread_safe_queue.hpp"
#include "timestamp_chunker.hpp"
#include "flow_formatter.hpp"
#include "generator_worker.hpp"
#include "flow_collector.hpp"
#include "arg_parser.hpp"
#include <flowgen/generator.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

using namespace flowdump;

struct ProgramOptions {
    std::string config_file;
    size_t num_threads = 10;
    uint64_t flows_per_thread = 0;
    uint64_t total_flows = 0;
    std::string output_format_str = "text";
    std::string sort_field_str = "timestamp";
    OutputFormat output_format = OutputFormat::PLAIN_TEXT;
    SortField sort_field = SortField::TIMESTAMP;
    uint64_t time_window_ms = 10;
    bool pretty = false;
    bool no_header = false;
    uint64_t start_timestamp_ns = 1704067200000000000ULL;  // 2024-01-01 00:00:00
    uint64_t end_timestamp_ns = 0;  // 0 means use duration-based calculation
};

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

OutputFormat parse_output_format(const std::string& format) {
    std::string fmt = format;
    std::transform(fmt.begin(), fmt.end(), fmt.begin(), ::tolower);

    if (fmt == "text") return OutputFormat::PLAIN_TEXT;
    if (fmt == "csv") return OutputFormat::CSV;
    if (fmt == "json") return OutputFormat::JSON;

    throw std::runtime_error("Invalid output format: " + format + " (valid: text, csv, json)");
}

SortField parse_sort_field(const std::string& field) {
    std::string fld = field;
    std::transform(fld.begin(), fld.end(), fld.begin(), ::tolower);

    if (fld == "timestamp") return SortField::TIMESTAMP;
    if (fld == "stream_id") return SortField::STREAM_ID;
    if (fld == "src_ip") return SortField::SOURCE_IP;
    if (fld == "dst_ip") return SortField::DESTINATION_IP;
    if (fld == "bytes") return SortField::BYTE_COUNT;
    if (fld == "packets") return SortField::PACKET_COUNT;

    throw std::runtime_error("Invalid sort field: " + field +
                           " (valid: timestamp, stream_id, src_ip, dst_ip, bytes, packets)");
}

int main(int argc, char** argv) {
    ProgramOptions opts;

    // Create argument parser
    ArgParser parser("FlowDump - Multi-threaded network flow generator with aggregation");

    // Add options
    parser.add_option("-c", "config", opts.config_file,
                     "Config file path", true);

    parser.add_option("-n", "num-threads", opts.num_threads,
                     "Number of generator threads", static_cast<size_t>(10));

    parser.add_option("-f", "flows-per-thread", opts.flows_per_thread,
                     "Number of flows per thread", static_cast<uint64_t>(0));

    parser.add_option("-t", "total-flows", opts.total_flows,
                     "Total flows to generate (overrides --flows-per-thread)", static_cast<uint64_t>(0));

    parser.add_option("-o", "output-format", opts.output_format_str,
                     "Output format: text, csv, json", false, "text");

    parser.add_option("-s", "sort-by", opts.sort_field_str,
                     "Sort by: timestamp, stream_id, src_ip, dst_ip, bytes, packets", false, "timestamp");

    parser.add_option("-w", "time-window", opts.time_window_ms,
                     "Time window for chunking in milliseconds", static_cast<uint64_t>(10));

    parser.add_option("", "start-timestamp", opts.start_timestamp_ns,
                     "Start timestamp in nanoseconds (Unix epoch)", static_cast<uint64_t>(1704067200000000000ULL));

    parser.add_option("", "end-timestamp", opts.end_timestamp_ns,
                     "End timestamp in nanoseconds (Unix epoch, 0=auto-calculate)", static_cast<uint64_t>(0));

    parser.add_flag("no-header", opts.no_header,
                   "Suppress header in CSV/text output");

    parser.add_flag("pretty", opts.pretty,
                   "Pretty-print JSON output");

    // Parse arguments
    if (!parser.parse(argc, argv)) {
        if (parser.should_show_help()) {
            parser.print_help();
            return 0;
        } else {
            std::cerr << "Error: " << parser.error() << "\n\n";
            parser.print_help();
            return 1;
        }
    }

    // Validate config file exists
    if (!file_exists(opts.config_file)) {
        std::cerr << "Error: Config file does not exist: " << opts.config_file << "\n";
        return 1;
    }

    // Parse output format
    try {
        opts.output_format = parse_output_format(opts.output_format_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Parse sort field
    try {
        opts.sort_field = parse_sort_field(opts.sort_field_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Validation
    if (opts.num_threads == 0) {
        std::cerr << "Error: Number of threads must be > 0\n";
        return 1;
    }

    if (opts.time_window_ms == 0) {
        std::cerr << "Error: Time window must be > 0\n";
        return 1;
    }

    // Load configuration
    // TODO: Load from YAML file when config parser is available
    // For now, create a basic config
    flowgen::GeneratorConfig base_config;

    base_config.bandwidth_gbps = 10.0;
    base_config.source_subnets = {"192.168.1.0/24", "192.168.2.0/24"};
    base_config.destination_subnets = {"10.0.0.0/8", "172.16.0.0/12"};
    base_config.source_weights = {70.0, 30.0};
    base_config.min_packet_size = 64;
    base_config.max_packet_size = 1500;
    base_config.average_packet_size = 800;

    // Set start timestamp
    base_config.start_timestamp_ns = opts.start_timestamp_ns;

    // Calculate flows_per_second based on bandwidth
    double flows_per_second = (base_config.bandwidth_gbps * 1e9 / 8.0) /
                               base_config.average_packet_size;

    // Determine flow count and end timestamp
    if (opts.end_timestamp_ns > 0) {
        // User specified end timestamp - calculate flow count from time range
        if (opts.end_timestamp_ns <= opts.start_timestamp_ns) {
            std::cerr << "Error: End timestamp must be greater than start timestamp\n";
            return 1;
        }

        uint64_t duration_ns = opts.end_timestamp_ns - opts.start_timestamp_ns;
        double duration_seconds = static_cast<double>(duration_ns) / 1e9;
        uint64_t calculated_total_flows = static_cast<uint64_t>(duration_seconds * flows_per_second);

        // If user also specified flow count, warn about override
        if (opts.total_flows > 0 || opts.flows_per_thread > 0) {
            std::cerr << "Warning: --end-timestamp overrides flow count options. "
                      << "Generating " << calculated_total_flows << " flows to fit time range.\n";
        }

        opts.total_flows = calculated_total_flows;
        opts.flows_per_thread = opts.total_flows / opts.num_threads;
        if (opts.total_flows % opts.num_threads != 0) {
            opts.flows_per_thread++;  // Round up
        }
    } else {
        // No end timestamp - use flow count to calculate duration
        if (opts.total_flows > 0) {
            opts.flows_per_thread = opts.total_flows / opts.num_threads;
            if (opts.total_flows % opts.num_threads != 0) {
                opts.flows_per_thread++;  // Round up
            }
        } else if (opts.flows_per_thread == 0) {
            opts.flows_per_thread = 10000;  // Default
        }

        // Calculate end timestamp based on flow count
        uint64_t total_flows = opts.num_threads * opts.flows_per_thread;
        double duration_seconds = static_cast<double>(total_flows) / flows_per_second;
        uint64_t duration_ns = static_cast<uint64_t>(duration_seconds * 1e9);
        opts.end_timestamp_ns = opts.start_timestamp_ns + duration_ns;
    }

    // Set max flows in config
    base_config.max_flows = opts.flows_per_thread;

    // Add traffic patterns
    base_config.traffic_patterns = {
        {"web_traffic", 40.0},
        {"dns_traffic", 20.0},
        {"database_traffic", 15.0},
        {"ssh_traffic", 10.0},
        {"random", 15.0}
    };

    // Create shared queue
    ThreadSafeQueue<EnhancedFlowRecord> flow_queue;

    // Create formatter
    FlowFormatter formatter(opts.output_format, opts.sort_field, opts.pretty);

    // Create collector
    uint64_t chunk_duration_ns = opts.time_window_ms * 1000000ULL;  // ms to ns
    FlowCollector collector(flow_queue, chunk_duration_ns, formatter,
                           std::cout, opts.num_threads, opts.no_header);

    // Launch collector thread
    std::thread collector_thread([&collector]() {
        collector.run();
    });

    // Launch generator threads
    std::vector<std::thread> generator_threads;
    std::vector<std::unique_ptr<GeneratorWorker>> workers;

    for (size_t i = 0; i < opts.num_threads; ++i) {
        uint32_t stream_id = i + 1;
        auto worker = std::make_unique<GeneratorWorker>(
            stream_id, base_config, flow_queue, opts.flows_per_thread
        );

        workers.push_back(std::move(worker));
    }

    // Start all generator threads
    for (size_t i = 0; i < workers.size(); ++i) {
        generator_threads.emplace_back([&workers, i, &collector]() {
            workers[i]->run();
            collector.generator_done();
        });
    }

    // Wait for all generators to complete
    for (auto& thread : generator_threads) {
        thread.join();
    }

    // Signal queue is done
    flow_queue.set_done();

    // Wait for collector to finish
    collector_thread.join();

    // Print summary to stderr so it doesn't interfere with output
    uint64_t total_generated = 0;
    for (const auto& worker : workers) {
        total_generated += worker->flows_generated();
    }

    std::cerr << "\nSummary:\n"
              << "  Threads: " << opts.num_threads << "\n"
              << "  Flows generated: " << total_generated << "\n"
              << "  Flows collected: " << collector.flows_collected() << "\n"
              << "  Timestamp range: " << opts.start_timestamp_ns << " - "
              << opts.end_timestamp_ns << " ns\n";

    return 0;
}
