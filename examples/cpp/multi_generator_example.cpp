// multi_generator_example.cpp
// Example: Multiple FlowGenerator instances writing to separate directories
// Features: Parallel thread-based generation for high performance

#include <flowgen/generator.hpp>
#include <flowgen/flow_record.hpp>
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
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Simple command-line argument parser
struct MultiGenOptions {
    size_t num_generators = 12;
    size_t flows_per_generator = 10000;
    size_t flows_per_file = 1000;
    std::string output_base_path = "./output";
    double bandwidth_gbps = 10.0;
    bool verbose = false;
    bool parallel = true;  // Enable parallel generation by default
};

// Thread-safe console output
std::mutex g_console_mutex;

void print_usage(const char* prog_name) {
    std::cout << "Multi-Generator Example (Parallel)\n"
              << "===================================\n\n"
              << "Usage: " << prog_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -n, --num-generators NUM      Number of generator instances (default: 12)\n"
              << "  -f, --flows-per-generator NUM Flows per generator (default: 10000)\n"
              << "  -b, --batch-size NUM          Flows per CSV file (default: 1000)\n"
              << "  -o, --output-path PATH        Base output directory (default: ./output)\n"
              << "  -w, --bandwidth GBPS          Bandwidth in Gbps (default: 10.0)\n"
              << "  -v, --verbose                 Verbose output\n"
              << "  --sequential                  Sequential generation (default: parallel)\n"
              << "  -h, --help                    Show this help\n\n"
              << "Example:\n"
              << "  " << prog_name << " -n 12 -f 50000 -o /tmp/flowdata\n"
              << "  " << prog_name << " -n 20 -f 100000 -v --sequential\n\n"
              << "Output Structure:\n"
              << "  <output-path>/\n"
              << "    generator_0/\n"
              << "      flows_0000.csv\n"
              << "      flows_0001.csv\n"
              << "      ...\n"
              << "    generator_1/\n"
              << "      flows_0000.csv\n"
              << "      ...\n\n"
              << "Performance:\n"
              << "  Parallel mode uses one thread per generator for maximum throughput.\n"
              << "  Sequential mode generates flows one generator at a time.\n";
}

bool parse_args(int argc, char** argv, MultiGenOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return false;
        } else if (arg == "-n" || arg == "--num-generators") {
            if (i + 1 < argc) {
                opts.num_generators = std::stoull(argv[++i]);
            }
        } else if (arg == "-f" || arg == "--flows-per-generator") {
            if (i + 1 < argc) {
                opts.flows_per_generator = std::stoull(argv[++i]);
            }
        } else if (arg == "-b" || arg == "--batch-size") {
            if (i + 1 < argc) {
                opts.flows_per_file = std::stoull(argv[++i]);
            }
        } else if (arg == "-o" || arg == "--output-path") {
            if (i + 1 < argc) {
                opts.output_base_path = argv[++i];
            }
        } else if (arg == "-w" || arg == "--bandwidth") {
            if (i + 1 < argc) {
                opts.bandwidth_gbps = std::stod(argv[++i]);
            }
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--sequential") {
            opts.parallel = false;
        }
    }

    return true;
}

// Create directory (recursive)
bool create_directory(const std::string& path) {
    // Try to create directory
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }

    // Check if it already exists
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }

    // Try creating parent directories
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        std::string parent = path.substr(0, pos);
        if (!create_directory(parent)) {
            return false;
        }
        return mkdir(path.c_str(), 0755) == 0;
    }

    return false;
}

// Generator instance manager
class GeneratorInstance {
private:
    size_t m_id;
    std::string m_output_dir;
    size_t m_flows_per_file;
    size_t m_total_flows;
    bool m_verbose;

    flowgen::FlowGenerator m_generator;

    size_t m_flows_generated;
    size_t m_files_written;
    size_t m_current_batch_count;
    std::ofstream m_current_file;

public:
    GeneratorInstance(size_t id, const std::string& base_path,
                     const flowgen::GeneratorConfig& config,
                     size_t flows_per_file, size_t total_flows, bool verbose)
        : m_id(id)
        , m_flows_per_file(flows_per_file)
        , m_total_flows(total_flows)
        , m_verbose(verbose)
        , m_flows_generated(0)
        , m_files_written(0)
        , m_current_batch_count(0)
    {
        // Create output directory for this generator
        m_output_dir = base_path + "/generator_" + std::to_string(id);

        if (!create_directory(m_output_dir)) {
            throw std::runtime_error("Failed to create directory: " + m_output_dir);
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

        while (m_flows_generated < m_total_flows && m_generator.next(flow)) {
            // Check if we need to rotate to a new file
            if (m_current_batch_count >= m_flows_per_file) {
                close_current_file();
                open_next_file();
            }

            // Write flow to current file
            m_current_file << flow.to_csv() << "\n";
            m_current_batch_count++;
            m_flows_generated++;

            // Progress reporting (every 10% or every 10K flows, whichever is less frequent)
            if (m_verbose) {
                size_t progress_interval = std::max(m_total_flows / 10, size_t(10000));
                if (m_flows_generated % progress_interval == 0) {
                    std::lock_guard<std::mutex> lock(g_console_mutex);
                    double pct = (m_flows_generated * 100.0) / m_total_flows;
                    std::cout << "[Generator " << m_id << "] Progress: "
                              << std::fixed << std::setprecision(1) << pct << "% ("
                              << m_flows_generated << "/" << m_total_flows << " flows, "
                              << m_files_written + 1 << " files)" << std::endl;
                }
            }
        }

        close_current_file();
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
    config.max_flows = opts.flows_per_generator;
    config.bandwidth_gbps = opts.bandwidth_gbps;

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

    // Parse command line arguments
    if (!parse_args(argc, argv, opts)) {
        return 0; // Help was shown
    }

    // Print configuration
    std::cout << "\n========================================\n";
    std::cout << "Multi-Generator Flow Example\n";
    std::cout << "========================================\n\n";
    std::cout << "Configuration:\n";
    std::cout << "  Number of generators: " << opts.num_generators << "\n";
    std::cout << "  Flows per generator: " << opts.flows_per_generator << "\n";
    std::cout << "  Flows per file: " << opts.flows_per_file << "\n";
    std::cout << "  Bandwidth: " << opts.bandwidth_gbps << " Gbps\n";
    std::cout << "  Execution mode: " << (opts.parallel ? "Parallel" : "Sequential") << "\n";
    std::cout << "  Output base path: " << opts.output_base_path << "\n";
    std::cout << "  Total flows: " << (opts.num_generators * opts.flows_per_generator) << "\n";
    std::cout << "\n";

    // Create base output directory
    if (!create_directory(opts.output_base_path)) {
        std::cerr << "Error: Failed to create base output directory: "
                  << opts.output_base_path << std::endl;
        return 1;
    }

    std::cout << "Created base directory: " << opts.output_base_path << "\n\n";

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
                opts.flows_per_generator,
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
                std::cout << "Generator " << gen->get_id() << ": Generating "
                          << opts.flows_per_generator << " flows..." << std::flush;
            }

            try {
                gen->generate_all();

                if (!opts.verbose) {
                    std::cout << " Done (" << gen->get_files_written() << " files)\n";
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
