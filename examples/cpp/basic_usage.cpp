/**
 * FlowGen C++ Example - Enhanced with CLI Arguments
 *
 * Demonstrates:
 * - Command-line argument parsing
 * - Configurable flow generation parameters
 * - Smart defaults (enterprise network scenario)
 * - Multiple output formats
 * - Reproducible generation with seeds
 */

#include <flowgen/generator.hpp>
#include <flowgen/flow_record.hpp>
#include <flowgen/utils.hpp>
#include <iostream>
#include <fstream>
#include <chrono>
#include <map>
#include <sstream>
#include <vector>

// Simple command-line argument parser
class ArgParser {
public:
    ArgParser(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                args_["help"] = "true";
            } else if (arg[0] == '-') {
                // Option with value
                std::string key = arg;
                std::string value;

                // Check if next arg is the value
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    value = argv[++i];
                } else {
                    // Flag without value (e.g., --verbose)
                    value = "true";
                }

                args_[key] = value;
            }
        }
    }

    bool has(const std::string& key) const {
        return args_.find(key) != args_.end() ||
               args_.find("--" + key) != args_.end() ||
               args_.find("-" + key) != args_.end();
    }

    std::string get(const std::string& key, const std::string& default_val = "") const {
        // Try long form
        auto it = args_.find("--" + key);
        if (it != args_.end()) return it->second;

        // Try short form
        it = args_.find("-" + key);
        if (it != args_.end()) return it->second;

        // Try exact match
        it = args_.find(key);
        if (it != args_.end()) return it->second;

        return default_val;
    }

    int get_int(const std::string& key, int default_val) const {
        std::string val = get(key);
        return val.empty() ? default_val : std::stoi(val);
    }

    uint64_t get_uint64(const std::string& key, uint64_t default_val) const {
        std::string val = get(key);
        return val.empty() ? default_val : std::stoull(val);
    }

    double get_double(const std::string& key, double default_val) const {
        std::string val = get(key);
        return val.empty() ? default_val : std::stod(val);
    }

    std::vector<std::string> get_list(const std::string& key,
                                      const std::vector<std::string>& default_val = {}) const {
        std::string val = get(key);
        if (val.empty()) return default_val;

        std::vector<std::string> result;
        std::stringstream ss(val);
        std::string item;

        while (std::getline(ss, item, ',')) {
            // Trim whitespace
            item.erase(0, item.find_first_not_of(" \t"));
            item.erase(item.find_last_not_of(" \t") + 1);
            result.push_back(item);
        }

        return result;
    }

private:
    std::map<std::string, std::string> args_;
};

// Parse traffic patterns from string
std::vector<flowgen::GeneratorConfig::TrafficPattern>
parse_patterns(const std::string& pattern_str) {
    std::vector<flowgen::GeneratorConfig::TrafficPattern> patterns;

    std::stringstream ss(pattern_str);
    std::string item;

    while (std::getline(ss, item, ',')) {
        // Trim
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);

        // Split on ':'
        size_t colon = item.find(':');
        if (colon == std::string::npos) {
            std::cerr << "Warning: Invalid pattern format (expected type:percentage): "
                      << item << "\n";
            continue;
        }

        std::string type = item.substr(0, colon);
        double percentage = std::stod(item.substr(colon + 1));

        patterns.push_back({type, percentage});
    }

    return patterns;
}

// Print help text
void print_help(const char* program_name) {
    std::cout << R"(
FlowGen C++ Example - Network Flow Generator

USAGE:
    )" << program_name << R"( [OPTIONS]

OPTIONS:
    -h, --help                Show this help message
    -f, --flows NUM           Number of flows to generate (default: 100000)
    -b, --bandwidth GBPS      Target bandwidth in Gbps (default: 10.0)
    -o, --output FILE         Output CSV file (default: cpp_output_flows.csv)

    --src-subnets CIDRS       Source subnets, comma-separated
                              (default: "192.168.0.0/16,10.10.0.0/16")

    --dst-subnets CIDRS       Destination subnets, comma-separated
                              (default: "10.100.0.0/16,203.0.113.0/24")

    --src-weights WEIGHTS     Source subnet weights, comma-separated
                              (default: equal weights)

    --patterns SPECS          Traffic patterns, format: "type:pct,type:pct,..."
                              (default: "web_traffic:40,database_traffic:20,
                                         dns_traffic:20,ssh_traffic:10,random:10")

    --start-time EPOCH_NS     Start timestamp in nanoseconds since epoch
                              (default: 1704067200000000000 = 2024-01-01 00:00:00)

    --pkt-min BYTES           Minimum packet size (default: 64)
    --pkt-max BYTES           Maximum packet size (default: 1500)
    --pkt-avg BYTES           Average packet size (default: 800)

    --seed VALUE              Random seed for reproducibility
    --verbose                 Print detailed progress

EXAMPLES:
    # Basic usage with defaults
    )" << program_name << R"(

    # Generate 1M flows at 40 Gbps
    )" << program_name << R"( -f 1000000 -b 40

    # Enterprise scenario with custom subnets
    )" << program_name << R"( \
      --src-subnets "192.168.0.0/16,10.10.0.0/16,172.16.0.0/12" \
      --dst-subnets "10.100.0.0/16,203.0.113.0/24"

    # Service mesh scenario
    )" << program_name << R"( -f 500000 -b 20 \
      --src-subnets "10.1.0.0/16,10.2.0.0/16,10.3.0.0/16" \
      --dst-subnets "10.10.0.0/16,10.11.0.0/16,10.12.0.0/16"

    # Reproducible test with seed
    )" << program_name << R"( --seed 12345 -f 10000

DEFAULT SCENARIO:
    Enterprise network with client subnets (192.168.0.0/16, 10.10.0.0/16)
    communicating with server subnets (10.100.0.0/16, 203.0.113.0/24).

TRAFFIC PATTERN TYPES:
    web_traffic         HTTP/HTTPS traffic (ports 80, 443)
    database_traffic    MySQL, PostgreSQL, MongoDB, Redis
    dns_traffic         DNS queries (port 53, UDP)
    ssh_traffic         SSH sessions (port 22, TCP)
    smtp_traffic        Email traffic (ports 25, 587, 465)
    ftp_traffic         FTP data and control (ports 20, 21)
    random              Completely random flows
)";
}

int main(int argc, char* argv[]) {
    using namespace flowgen;

    ArgParser args(argc, argv);

    // Print help if requested
    if (args.has("help")) {
        print_help(argv[0]);
        return 0;
    }

    std::cout << "FlowGen C++ Example\n";
    std::cout << "===================\n\n";

    // Parse command-line arguments with SMART DEFAULTS
    uint64_t max_flows = args.get_uint64("flows", 100000);
    double bandwidth_gbps = args.get_double("bandwidth", 10.0);
    std::string output_file = args.get("output", "cpp_output_flows.csv");
    bool verbose = args.has("verbose");

    // Network configuration - ENTERPRISE DEFAULTS
    auto src_subnets = args.get_list("src-subnets",
        {"192.168.0.0/16", "10.10.0.0/16"});
    auto dst_subnets = args.get_list("dst-subnets",
        {"10.100.0.0/16", "203.0.113.0/24"});

    std::vector<double> src_weights;
    if (args.has("src-weights")) {
        auto weight_strs = args.get_list("src-weights");
        for (const auto& w : weight_strs) {
            src_weights.push_back(std::stod(w));
        }
    }

    // Packet configuration
    uint32_t pkt_min = args.get_int("pkt-min", 64);
    uint32_t pkt_max = args.get_int("pkt-max", 1500);
    uint32_t pkt_avg = args.get_int("pkt-avg", 800);

    // Timestamp
    uint64_t start_ts_ns = args.get_uint64("start-time", 1704067200000000000ULL);

    // Traffic patterns - REALISTIC DEFAULTS
    std::vector<GeneratorConfig::TrafficPattern> patterns;
    if (args.has("patterns")) {
        patterns = parse_patterns(args.get("patterns"));
    } else {
        // Default: realistic enterprise mix
        patterns = {
            {"web_traffic", 40.0},
            {"database_traffic", 20.0},
            {"dns_traffic", 20.0},
            {"ssh_traffic", 10.0},
            {"random", 10.0}
        };
    }

    // Random seed
    if (args.has("seed")) {
        uint64_t seed = args.get_uint64("seed", 0);
        utils::Random::instance().seed(seed);
        std::cout << "Using random seed: " << seed << "\n\n";
    }

    // Print configuration
    std::cout << "Configuration:\n";
    std::cout << "  Flows: " << max_flows << "\n";
    std::cout << "  Bandwidth: " << bandwidth_gbps << " Gbps\n";
    std::cout << "  Output: " << output_file << "\n";
    std::cout << "  Source subnets: ";
    for (const auto& s : src_subnets) std::cout << s << " ";
    std::cout << "\n  Destination subnets: ";
    for (const auto& d : dst_subnets) std::cout << d << " ";
    std::cout << "\n  Packet size: " << pkt_min << "-" << pkt_max
              << " bytes (avg: " << pkt_avg << ")\n";
    std::cout << "  Traffic patterns: ";
    for (const auto& p : patterns) {
        std::cout << p.type << ":" << p.percentage << "% ";
    }
    std::cout << "\n\n";

    // Create generator configuration (lightweight - no stopping conditions)
    GeneratorConfig config;
    config.bandwidth_gbps = bandwidth_gbps;
    config.start_timestamp_ns = start_ts_ns;
    config.source_subnets = src_subnets;
    config.destination_subnets = dst_subnets;
    config.source_weights = src_weights;
    config.min_packet_size = pkt_min;
    config.max_packet_size = pkt_max;
    config.average_packet_size = pkt_avg;
    config.traffic_patterns = patterns;

    // Validate configuration
    std::string error;
    if (!config.validate(&error)) {
        std::cerr << "Config validation failed: " << error << "\n";
        return 1;
    }

    // Create and initialize generator
    FlowGenerator generator;

    std::cout << "Initializing generator...\n";
    if (!generator.initialize(config)) {
        std::cerr << "Failed to initialize generator\n";
        return 1;
    }

    // Calculate expected flow rate from bandwidth
    double flows_per_second = utils::calculate_flows_per_second(bandwidth_gbps, pkt_avg);
    std::cout << "Generator initialized successfully\n";
    std::cout << "Target rate: " << flows_per_second << " flows/sec\n";
    std::cout << "Will generate " << max_flows << " flows\n\n";

    // Open output file
    std::ofstream output(output_file);
    if (!output.is_open()) {
        std::cerr << "Failed to open output file: " << output_file << "\n";
        return 1;
    }

    // Write CSV header
    output << FlowRecord::csv_header() << "\n";

    // Generate flows (application manages stop condition)
    std::cout << "Generating flows...\n";
    auto start_time = std::chrono::high_resolution_clock::now();

    FlowRecord flow;
    uint64_t count = 0;

    // Application-managed loop - generate until we reach max_flows
    while (count < max_flows) {
        // Generate next flow (lightweight generator always succeeds)
        generator.next(flow);

        // Write flow to CSV
        output << flow.to_csv() << "\n";

        count++;

        // Print progress
        if (verbose && count % 10000 == 0) {
            std::cout << "  Generated " << count << " flows...\n";
        } else if (!verbose && count % 50000 == 0) {
            std::cout << "  Generated " << count << " flows...\n";
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end_time - start_time).count();

    output.close();

    // Calculate timestamp range
    uint64_t final_timestamp = generator.current_timestamp_ns();
    double timestamp_range_sec = (final_timestamp - start_ts_ns) / 1e9;

    // Print statistics
    std::cout << "\nGeneration complete!\n";
    std::cout << "  Total flows: " << count << "\n";
    std::cout << "  Elapsed time: " << elapsed << " seconds\n";
    std::cout << "  Generation rate: " << (count / elapsed) << " flows/sec\n";
    std::cout << "  Timestamp range: " << timestamp_range_sec << " seconds\n";
    std::cout << "\nOutput written to: " << output_file << "\n";

    // Show first few flows if verbose
    if (verbose) {
        std::cout << "\nFirst 5 flows from output:\n";
        std::ifstream check(output_file);
        std::string line;
        int line_count = 0;
        while (std::getline(check, line) && line_count < 6) {
            std::cout << "  " << line << "\n";
            line_count++;
        }
    }

    return 0;
}
