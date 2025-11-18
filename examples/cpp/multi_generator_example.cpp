// multi_generator_example.cpp
// Example: Multiple FlowGenerator instances writing to separate directories
// Features: Parallel thread-based generation for high performance
// Uses lightweight FlowGenerator - application manages all stop conditions

#include <flowgen/generator.hpp>
#include <flowgen/flow_record.hpp>
#include "arg_parser.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>

// Command-line options
struct MultiGenOptions {
    // Generator configuration
    size_t num_generators = 12;
    double bandwidth_gbps = 10.0;
    std::string output_base_path = "./output";
    size_t flows_per_file = 1000;

    // Stop conditions (mutually exclusive - one must be specified)
    uint64_t start_timestamp_ns = 0;   // Required
    uint64_t end_timestamp_ns = 0;     // Option 1: explicit end time
    uint64_t duration_ns = 0;          // Option 2: duration from start
    size_t total_flows = 0;            // Option 3: total flow count

    // Execution mode
    bool verbose = false;
    bool parallel = true;
};

// Thread-safe console output
std::mutex g_console_mutex;

// Generator instance manager
class GeneratorInstance {
private:
    size_t m_id;
    std::string m_output_dir;
    size_t m_flows_per_file;
    uint64_t m_end_timestamp_ns;  // Stopping condition (timestamp)
    size_t m_max_flows;           // Stopping condition (flow count)
    bool m_verbose;

    flowgen::FlowGenerator m_generator;

    size_t m_flows_generated;
    size_t m_files_written;
    size_t m_current_batch_count;
    std::ofstream m_current_file;

public:
    GeneratorInstance(size_t id, const std::string& base_path,
                     const flowgen::GeneratorConfig& config,
                     size_t flows_per_file,
                     uint64_t end_timestamp_ns,
                     size_t max_flows,
                     bool verbose)
        : m_id(id)
        , m_flows_per_file(flows_per_file)
        , m_end_timestamp_ns(end_timestamp_ns)
        , m_max_flows(max_flows)
        , m_verbose(verbose)
        , m_flows_generated(0)
        , m_files_written(0)
        , m_current_batch_count(0)
    {
        // Create output directory for this generator using std::filesystem
        m_output_dir = base_path + "/generator_" + std::to_string(id);

        std::filesystem::path dir_path(m_output_dir);
        if (!std::filesystem::exists(dir_path)) {
            if (!std::filesystem::create_directories(dir_path)) {
                throw std::runtime_error("Failed to create directory: " + m_output_dir);
            }
        }

        if (m_verbose) {
            std::lock_guard<std::mutex> lock(g_console_mutex);
            std::cout << "[Generator " << m_id << "] Created directory: "
                      << m_output_dir << std::endl;
        }

        // Initialize generator
        if (!m_generator.initialize(config)) {
            throw std::runtime_error("Failed to initialize generator " + std::to_string(id));
        }

        // Open first file
        open_next_file();
    }

    ~GeneratorInstance() {
        close_current_file();
    }

    // Generate all flows for this instance
    void generate_all() {
        flowgen::FlowRecord flow;

        // Keep generating flows until stopping condition met
        while (!should_stop()) {
            // Generate next flow (always succeeds with lightweight generator)
            m_generator.next(flow);

            // Check if we need to rotate to a new file
            if (m_current_batch_count >= m_flows_per_file) {
                close_current_file();
                open_next_file();
            }

            // Write flow to current file
            m_current_file << flow.to_csv() << "\n";
            m_current_batch_count++;
            m_flows_generated++;

            // Progress reporting (every 10K flows)
            if (m_verbose && m_flows_generated % 10000 == 0) {
                std::lock_guard<std::mutex> lock(g_console_mutex);
                std::cout << "[Generator " << m_id << "] Progress: "
                          << m_flows_generated << " flows, "
                          << m_files_written + 1 << " files" << std::endl;
            }
        }

        close_current_file();
    }

    // Check if we should stop generating flows
    bool should_stop() const {
        // Check flow count limit
        if (m_max_flows > 0 && m_flows_generated >= m_max_flows) {
            return true;
        }

        // Check timestamp limit
        if (m_end_timestamp_ns > 0 && m_generator.current_timestamp_ns() >= m_end_timestamp_ns) {
            return true;
        }

        return false;
    }

    size_t get_id() const { return m_id; }
    size_t get_flows_generated() const { return m_flows_generated; }
    size_t get_files_written() const { return m_files_written; }
    const std::string& get_output_dir() const { return m_output_dir; }

private:
    void open_next_file() {
        // Generate filename with zero-padded file number
        std::ostringstream oss;
        oss << m_output_dir << "/flows_"
            << std::setw(4) << std::setfill('0') << m_files_written
            << ".csv";
        std::string filename = oss.str();

        m_current_file.open(filename);
        if (!m_current_file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        // Write CSV header
        m_current_file << flowgen::FlowRecord::csv_header() << "\n";

        m_current_batch_count = 0;

        if (m_verbose) {
            std::lock_guard<std::mutex> lock(g_console_mutex);
            std::cout << "[Generator " << m_id << "] Opened file: " << filename << std::endl;
        }
    }

    void close_current_file() {
        if (m_current_file.is_open()) {
            m_current_file.close();

            if (m_current_batch_count > 0) {
                m_files_written++;
            }
        }
    }
};

// Create configuration for a generator instance
flowgen::GeneratorConfig create_config(size_t generator_id, const MultiGenOptions& opts) {
    flowgen::GeneratorConfig config;

    // Basic settings
    config.bandwidth_gbps = opts.bandwidth_gbps;
    config.start_timestamp_ns = opts.start_timestamp_ns + (generator_id * 1000000ULL);  // +1ms per generator

    // Different source subnets for each generator to create diversity
    // Generator 0: 192.168.0.0/16
    // Generator 1: 10.10.0.0/16
    // Generator 2: 172.16.0.0/16
    // etc.
    std::vector<std::string> base_subnets = {
        "192.168.0.0/16",
        "10.10.0.0/16",
        "172.16.0.0/12",
        "10.20.0.0/16",
        "10.30.0.0/16",
        "10.40.0.0/16",
        "10.50.0.0/16",
        "10.60.0.0/16",
        "10.70.0.0/16",
        "10.80.0.0/16",
        "10.90.0.0/16",
        "10.100.0.0/16"
    };

    config.source_subnets = {base_subnets[generator_id % base_subnets.size()]};

    // Common destination subnets
    config.destination_subnets = {
        "10.200.0.0/16",
        "10.201.0.0/16",
        "203.0.113.0/24"
    };

    // Traffic patterns (vary slightly by generator for diversity)
    if (generator_id % 3 == 0) {
        // Web-heavy
        config.traffic_patterns = {
            {"web_traffic", 50.0},
            {"dns_traffic", 20.0},
            {"database_traffic", 15.0},
            {"ssh_traffic", 10.0},
            {"random", 5.0}
        };
    } else if (generator_id % 3 == 1) {
        // Database-heavy
        config.traffic_patterns = {
            {"database_traffic", 40.0},
            {"web_traffic", 30.0},
            {"dns_traffic", 15.0},
            {"ssh_traffic", 10.0},
            {"random", 5.0}
        };
    } else {
        // Balanced
        config.traffic_patterns = {
            {"web_traffic", 30.0},
            {"dns_traffic", 25.0},
            {"database_traffic", 20.0},
            {"ssh_traffic", 15.0},
            {"random", 10.0}
        };
    }

    // Packet size settings
    config.min_packet_size = 64;
    config.max_packet_size = 1500;
    config.average_packet_size = 800;

    // Timestamp (each generator starts at slightly different time for realism)
    config.start_timestamp_ns = 1704067200000000000ULL + (generator_id * 1000000ULL); // +1ms per generator

    // Bidirectional mode (enabled for some generators)
    if (generator_id % 2 == 0) {
        config.bidirectional_mode = "random";
        config.bidirectional_probability = 0.5;
    } else {
        config.bidirectional_mode = "none";
    }

    return config;
}

int main(int argc, char** argv) {
    MultiGenOptions opts;

    // Create argument parser
    examples::ArgParser parser("Multi-Generator Example - Lightweight flow generation");

    // Add options
    parser.add_option("-n", "num-generators", opts.num_generators,
                     "Number of generator instances", size_t(12));
    parser.add_option("-w", "bandwidth", opts.bandwidth_gbps,
                     "Bandwidth in Gbps", 10.0);
    parser.add_option("-o", "output-path", opts.output_base_path,
                     "Base output directory", false, "./output");
    parser.add_option("-b", "batch-size", opts.flows_per_file,
                     "Flows per CSV file", size_t(1000));

    // Stop conditions (one must be specified)
    parser.add_option("", "start-timestamp", opts.start_timestamp_ns,
                     "Start timestamp (nanoseconds since epoch)", size_t(1704067200000000000ULL));
    parser.add_option("", "end-timestamp", opts.end_timestamp_ns,
                     "End timestamp (nanoseconds)", size_t(0));
    parser.add_option("", "duration", opts.duration_ns,
                     "Duration (nanoseconds)", size_t(0));
    parser.add_option("", "total-flows", opts.total_flows,
                     "Total flows to generate", size_t(0));

    // Execution mode
    parser.add_flag("verbose", opts.verbose,
                   "Verbose output");
    parser.add_flag("sequential", opts.parallel,
                   "Sequential generation (default: parallel)");

    // Parse arguments
    if (!parser.parse(argc, argv)) {
        if (parser.should_show_help()) {
            parser.print_help();
            std::cout << "\nStop Conditions (specify ONE of the following):\n"
                      << "  --end-timestamp: Generate flows until this timestamp\n"
                      << "  --duration: Generate flows for this many nanoseconds\n"
                      << "  --total-flows: Generate this many flows total (across all generators)\n\n"
                      << "Examples:\n"
                      << "  # Generate flows for 60 seconds\n"
                      << "  " << argv[0] << " -n 12 -w 10 --duration 60000000000\n\n"
                      << "  # Generate 1M total flows\n"
                      << "  " << argv[0] << " -n 10 --total-flows 1000000\n\n"
                      << "  # Generate until specific timestamp\n"
                      << "  " << argv[0] << " -n 5 --start-timestamp 1704067200000000000 \\\n"
                      << "                      --end-timestamp 1704067260000000000\n\n"
                      << "Output Structure:\n"
                      << "  <output-path>/generator_0/, generator_1/, ...\n"
                      << "  Each generator directory contains flows_NNNN.csv files\n";
        } else {
            std::cerr << "Error: " << parser.error() << std::endl;
        }
        return parser.should_show_help() ? 0 : 1;
    }

    // Flip parallel flag (since the flag sets it to true, but we want sequential to set parallel=false)
    opts.parallel = !opts.parallel;

    // Validate stop conditions - exactly one must be specified
    int stop_conditions = 0;
    if (opts.end_timestamp_ns > 0) stop_conditions++;
    if (opts.duration_ns > 0) stop_conditions++;
    if (opts.total_flows > 0) stop_conditions++;

    if (stop_conditions == 0) {
        std::cerr << "Error: Must specify one stop condition (--end-timestamp, --duration, or --total-flows)\n";
        return 1;
    }
    if (stop_conditions > 1) {
        std::cerr << "Error: Only one stop condition can be specified\n";
        return 1;
    }

    // Calculate end_timestamp_ns if duration specified
    if (opts.duration_ns > 0) {
        opts.end_timestamp_ns = opts.start_timestamp_ns + opts.duration_ns;
    }

    // Calculate total flows per generator if total_flows specified
    size_t flows_per_generator = 0;
    if (opts.total_flows > 0) {
        flows_per_generator = opts.total_flows / opts.num_generators;
        if (opts.total_flows % opts.num_generators != 0) {
            flows_per_generator++; // Round up
        }
    }

    // Print configuration
    std::cout << "\n========================================\n";
    std::cout << "Multi-Generator Flow Example (Lightweight)\n";
    std::cout << "========================================\n\n";
    std::cout << "Configuration:\n";
    std::cout << "  Number of generators: " << opts.num_generators << "\n";
    std::cout << "  Bandwidth: " << opts.bandwidth_gbps << " Gbps\n";
    std::cout << "  Execution mode: " << (opts.parallel ? "Parallel" : "Sequential") << "\n";
    std::cout << "  Output base path: " << opts.output_base_path << "\n";
    std::cout << "  Flows per file: " << opts.flows_per_file << "\n";

    // Print stop condition
    if (opts.end_timestamp_ns > 0 && opts.duration_ns > 0) {
        std::cout << "  Stop condition: Duration (" << (opts.duration_ns / 1000000000.0) << " seconds)\n";
    } else if (opts.end_timestamp_ns > 0) {
        std::cout << "  Stop condition: End timestamp (" << opts.end_timestamp_ns << " ns)\n";
    } else {
        std::cout << "  Stop condition: Total flows (" << opts.total_flows << ")\n";
        std::cout << "  Flows per generator: ~" << flows_per_generator << "\n";
    }
    std::cout << "\n";

    // Create base output directory using std::filesystem
    std::filesystem::path output_path(opts.output_base_path);
    try {
        if (!std::filesystem::exists(output_path)) {
            std::filesystem::create_directories(output_path);
        }
        std::cout << "Output directory: " << std::filesystem::absolute(output_path) << "\n\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to create output directory: " << e.what() << std::endl;
        return 1;
    }

    // Create all generator instances
    std::cout << "Initializing " << opts.num_generators << " generators...\n";
    std::vector<std::unique_ptr<GeneratorInstance>> generators;

    try {
        for (size_t i = 0; i < opts.num_generators; ++i) {
            flowgen::GeneratorConfig config = create_config(i, opts);

            auto gen = std::make_unique<GeneratorInstance>(
                i,
                opts.output_base_path,
                config,
                opts.flows_per_file,
                opts.end_timestamp_ns,          // Stopping condition: timestamp
                flows_per_generator,            // Stopping condition: flow count
                opts.verbose
            );

            generators.push_back(std::move(gen));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during initialization: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "All generators initialized successfully.\n\n";

    // Generate flows - parallel or sequential mode
    if (opts.parallel) {
        std::cout << "Generating flows (PARALLEL mode with " << opts.num_generators << " threads)...\n\n";
    } else {
        std::cout << "Generating flows (SEQUENTIAL mode)...\n\n";
    }

    auto start_time = std::chrono::steady_clock::now();

    if (opts.parallel) {
        // Parallel generation: one thread per generator
        std::vector<std::thread> threads;
        std::atomic<size_t> completed_generators(0);
        std::atomic<bool> error_occurred(false);
        std::string error_message;
        std::mutex error_mutex;

        for (auto& gen : generators) {
            threads.emplace_back([&gen, &completed_generators, &error_occurred, &error_message, &error_mutex, &opts]() {
                try {
                    gen->generate_all();

                    // Increment completed count
                    completed_generators.fetch_add(1, std::memory_order_relaxed);

                    if (!opts.verbose) {
                        std::lock_guard<std::mutex> lock(g_console_mutex);
                        std::cout << "Generator " << gen->get_id() << ": Done ("
                                  << gen->get_files_written() << " files, "
                                  << gen->get_flows_generated() << " flows)\n";
                    }
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(error_mutex);
                    if (!error_occurred.exchange(true)) {
                        error_message = "Error in generator " + std::to_string(gen->get_id()) + ": " + e.what();
                    }
                }
            });
        }

        // Wait for all threads to complete
        for (auto& t : threads) {
            t.join();
        }

        // Check for errors
        if (error_occurred) {
            std::cerr << "\n" << error_message << std::endl;
            return 1;
        }

    } else {
        // Sequential generation: one generator at a time
        for (auto& gen : generators) {
            if (!opts.verbose) {
                std::cout << "Generator " << gen->get_id() << ": Generating flows..." << std::flush;
            }

            try {
                gen->generate_all();

                if (!opts.verbose) {
                    std::cout << " Done (" << gen->get_flows_generated()
                              << " flows, " << gen->get_files_written() << " files)\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "\nError in generator " << gen->get_id()
                          << ": " << e.what() << std::endl;
                return 1;
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Print summary
    std::cout << "\n========================================\n";
    std::cout << "Generation Complete!\n";
    std::cout << "========================================\n\n";

    size_t total_flows = 0;
    size_t total_files = 0;

    std::cout << "Summary by Generator:\n";
    std::cout << std::string(60, '-') << "\n";
    std::cout << std::setw(10) << "Gen ID"
              << std::setw(15) << "Flows"
              << std::setw(10) << "Files"
              << std::setw(25) << "Output Directory" << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (const auto& gen : generators) {
        std::cout << std::setw(10) << gen->get_id()
                  << std::setw(15) << gen->get_flows_generated()
                  << std::setw(10) << gen->get_files_written()
                  << "  " << gen->get_output_dir() << "\n";

        total_flows += gen->get_flows_generated();
        total_files += gen->get_files_written();
    }

    std::cout << std::string(60, '-') << "\n";
    std::cout << std::setw(10) << "TOTAL"
              << std::setw(15) << total_flows
              << std::setw(10) << total_files << "\n";
    std::cout << std::string(60, '-') << "\n\n";

    std::cout << "Performance:\n";
    std::cout << "  Elapsed time: " << (duration.count() / 1000.0) << " seconds\n";
    std::cout << "  Generation rate: " << (total_flows * 1000.0 / duration.count())
              << " flows/second\n";

    std::cout << "\nOutput structure:\n";
    std::cout << "  " << opts.output_base_path << "/\n";
    for (size_t i = 0; i < std::min(size_t(3), opts.num_generators); ++i) {
        std::cout << "  ├── generator_" << i << "/\n";
        std::cout << "  │   ├── flows_0000.csv\n";
        std::cout << "  │   ├── flows_0001.csv\n";
        std::cout << "  │   └── ...\n";
    }
    if (opts.num_generators > 3) {
        std::cout << "  └── ... (+" << (opts.num_generators - 3) << " more generators)\n";
    }

    std::cout << "\nExample commands:\n";
    std::cout << "  # Count flows in generator 0\n";
    std::cout << "  wc -l " << opts.output_base_path << "/generator_0/*.csv\n\n";
    std::cout << "  # View first file from generator 0\n";
    std::cout << "  head " << opts.output_base_path << "/generator_0/flows_0000.csv\n\n";
    std::cout << "  # Combine all flows from all generators\n";
    std::cout << "  cat " << opts.output_base_path << "/generator_*/flows_*.csv > all_flows.csv\n\n";

    return 0;
}
