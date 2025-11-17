#include "subcommands/flows_command.h"
#include "subcommands/port_command.h"
#include "utils/arg_parser.h"
#include <iostream>
#include <string>
#include <cstring>

using namespace flowstats;

// Print main usage
void print_usage() {
    std::cout << "FlowStats - Modular network flow statistics tool\n\n";
    std::cout << "Usage: flowstats <subcommand> [options]\n\n";
    std::cout << "Subcommands:\n";
    std::cout << "  flows      Generate and collect flow records\n";
    std::cout << "  port       Aggregate port statistics from flows\n";
    std::cout << "  help       Show this help message\n\n";
    std::cout << "Run 'flowstats <subcommand> --help' for subcommand-specific options\n";
}

// Parse progress style from string
ProgressStyle parse_progress_style(const std::string& style_str) {
    std::string style = style_str;
    std::transform(style.begin(), style.end(), style.begin(), ::tolower);

    if (style == "bar") {
        return ProgressStyle::BAR;
    } else if (style == "simple") {
        return ProgressStyle::SIMPLE;
    } else if (style == "spinner") {
        return ProgressStyle::SPINNER;
    } else if (style == "none") {
        return ProgressStyle::NONE;
    } else {
        throw std::runtime_error("Invalid progress style: " + style_str +
                                " (valid: bar, simple, spinner, none)");
    }
}

// Flows subcommand entry point
int flowstats_flows_main(int argc, char** argv) {
    FlowsOptions opts;

    // Temporary variables for parsing
    std::string output_format_str = "text";
    std::string progress_style_str = "bar";

    // Parse arguments
    ArgParser parser("flowstats flows - Generate and collect flow records");

    parser.add_option("c", "config", opts.m_config_file,
                     "Config file path (dummy for now)", false, "dummy.yaml");

    parser.add_option("n", "num-threads", opts.m_num_threads,
                     "Number of generator threads", static_cast<size_t>(10));

    parser.add_option("f", "flows-per-thread", opts.m_flows_per_thread,
                     "Number of flows per thread", static_cast<size_t>(10000));

    parser.add_option("t", "total-flows", opts.m_total_flows,
                     "Total flows to generate (overrides -f)", static_cast<uint64_t>(0));

    parser.add_option("", "start-timestamp", opts.m_start_timestamp_ns,
                     "Start timestamp in nanoseconds", static_cast<uint64_t>(1704067200000000000ULL));

    parser.add_option("", "end-timestamp", opts.m_end_timestamp_ns,
                     "End timestamp in nanoseconds (0 = auto-calculate)", static_cast<uint64_t>(0));

    parser.add_option("o", "output-format", output_format_str,
                     "Output format: text, csv, json, json-pretty", false, "text");

    parser.add_flag("no-header", opts.m_no_header,
                   "Suppress header in output");

    parser.add_flag("no-progress", opts.m_no_header,  // Will be inverted
                   "Disable progress indicator");

    parser.add_option("", "progress-style", progress_style_str,
                     "Progress style: bar, simple, spinner, none", false, "bar");

    if (!parser.parse(argc, argv)) {
        if (parser.has_error()) {
            std::cerr << "Error: " << parser.error() << "\n\n";
            parser.print_help();
        }
        return parser.has_error() ? 1 : 0;
    }

    // Parse output format
    try {
        opts.m_output_format = parse_output_format(output_format_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Parse progress style
    try {
        opts.m_progress_style = parse_progress_style(progress_style_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Handle no-progress flag properly
    if (opts.m_no_header) {  // Reused variable, need to fix
        opts.m_show_progress = false;
    }

    // Create and execute command
    FlowStatsFlows cmd(opts);
    return cmd.execute();
}

// Port subcommand entry point
int flowstats_port_main(int argc, char** argv) {
    PortOptions opts;

    // Temporary variables for parsing
    std::string output_format_str = "text";
    std::string progress_style_str = "bar";
    std::string sort_field_str = "total_bytes";

    // Parse arguments
    ArgParser parser("flowstats port - Aggregate port statistics from flows");

    parser.add_option("c", "config", opts.m_config_file,
                     "Config file path (dummy for now)", false, "dummy.yaml");

    parser.add_option("n", "num-threads", opts.m_num_threads,
                     "Number of generator threads", static_cast<size_t>(10));

    parser.add_option("f", "flows-per-thread", opts.m_flows_per_thread,
                     "Number of flows per thread", static_cast<size_t>(10000));

    parser.add_option("t", "total-flows", opts.m_total_flows,
                     "Total flows to generate (overrides -f)", static_cast<uint64_t>(0));

    parser.add_option("", "start-timestamp", opts.m_start_timestamp_ns,
                     "Start timestamp in nanoseconds", static_cast<uint64_t>(1704067200000000000ULL));

    parser.add_option("", "end-timestamp", opts.m_end_timestamp_ns,
                     "End timestamp in nanoseconds (0 = auto-calculate)", static_cast<uint64_t>(0));

    parser.add_option("o", "output-format", output_format_str,
                     "Output format: text, csv, json, json-pretty", false, "text");

    parser.add_flag("no-header", opts.m_no_header,
                   "Suppress header in output");

    parser.add_flag("no-progress", opts.m_no_header,  // Will be inverted
                   "Disable progress indicator");

    parser.add_option("", "progress-style", progress_style_str,
                     "Progress style: bar, simple, spinner, none", false, "bar");

    parser.add_option("s", "sort-by", sort_field_str,
                     "Sort by field: port, flows, tx_bytes, rx_bytes, total_bytes, tx_packets, rx_packets, total_packets",
                     false, "total_bytes");

    parser.add_option("", "top", opts.m_top_n,
                     "Show only top N results (0 = show all)", static_cast<size_t>(0));

    if (!parser.parse(argc, argv)) {
        if (parser.has_error()) {
            std::cerr << "Error: " << parser.error() << "\n\n";
            parser.print_help();
        }
        return parser.has_error() ? 1 : 0;
    }

    // Parse output format
    try {
        opts.m_output_format = parse_output_format(output_format_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Parse progress style
    try {
        opts.m_progress_style = parse_progress_style(progress_style_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Parse sort field
    try {
        opts.m_sort_field = parse_sort_field(sort_field_str);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Handle no-progress flag properly
    if (opts.m_no_header) {  // Reused variable, need to fix
        opts.m_show_progress = false;
    }

    // Create and execute command
    FlowStatsPort cmd(opts);
    return cmd.execute();
}

// Main entry point
int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string subcommand = argv[1];

    // Help
    if (subcommand == "-h" || subcommand == "--help" || subcommand == "help") {
        print_usage();
        return 0;
    }

    // Flows subcommand
    if (subcommand == "flows") {
        return flowstats_flows_main(argc - 1, argv + 1);
    }

    // Port subcommand
    if (subcommand == "port") {
        return flowstats_port_main(argc - 1, argv + 1);
    }

    // Unknown subcommand
    std::cerr << "Error: Unknown subcommand: " << subcommand << "\n\n";
    print_usage();
    return 1;
}
