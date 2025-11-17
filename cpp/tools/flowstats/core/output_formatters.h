#pragma once

#include "../utils/enhanced_flow.h"
#include "../utils/port_stat.h"
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <iomanip>

namespace flowstats {

// Output format enum
enum class OutputFormat {
    TEXT,
    CSV,
    JSON,
    JSON_PRETTY
};

// Parse output format from string
inline OutputFormat parse_output_format(const std::string& format_str) {
    std::string fmt = format_str;
    std::transform(fmt.begin(), fmt.end(), fmt.begin(), ::tolower);

    if (fmt == "text" || fmt == "plain") {
        return OutputFormat::TEXT;
    } else if (fmt == "csv") {
        return OutputFormat::CSV;
    } else if (fmt == "json") {
        return OutputFormat::JSON;
    } else if (fmt == "json-pretty" || fmt == "pretty") {
        return OutputFormat::JSON_PRETTY;
    } else {
        throw std::runtime_error("Invalid output format: " + format_str +
                                " (valid: text, csv, json, json-pretty)");
    }
}

// Base formatter class
template<typename ResultType>
class OutputFormatter {
public:
    virtual ~OutputFormatter() = default;
    virtual void format(const ResultType& results, std::ostream& out, bool no_header = false) = 0;
};

// Specialization for CollectResult (vector of flows)
struct CollectResult {
    std::vector<EnhancedFlowRecord> m_flows;
    uint64_t m_total_flows;
    uint64_t m_total_bytes;
    uint64_t m_start_ts;
    uint64_t m_end_ts;

    CollectResult()
        : m_total_flows(0)
        , m_total_bytes(0)
        , m_start_ts(0)
        , m_end_ts(0)
    {}
};

// Text formatter for CollectResult
class TextFormatter : public OutputFormatter<CollectResult> {
public:
    void format(const CollectResult& results, std::ostream& out, bool no_header = false) override {
        if (!no_header) {
            out << EnhancedFlowRecord::plain_text_header() << "\n";
        }

        for (const auto& flow : results.m_flows) {
            out << flow.to_plain_text(false) << "\n";
        }
    }
};

// CSV formatter for CollectResult
class CSVFormatter : public OutputFormatter<CollectResult> {
public:
    void format(const CollectResult& results, std::ostream& out, bool no_header = false) override {
        if (!no_header) {
            out << EnhancedFlowRecord::csv_header() << "\n";
        }

        for (const auto& flow : results.m_flows) {
            out << flow.to_csv() << "\n";
        }
    }
};

// JSON formatter for CollectResult
class JSONFormatter : public OutputFormatter<CollectResult> {
public:
    explicit JSONFormatter(bool pretty = false)
        : m_pretty(pretty)
    {}

    void format(const CollectResult& results, std::ostream& out, bool no_header = false) override {
        out << "[";
        if (m_pretty) {
            out << "\n";
        }

        size_t count = results.m_flows.size();
        for (size_t i = 0; i < count; ++i) {
            bool last = (i == count - 1);
            out << results.m_flows[i].to_json(m_pretty, last);
        }

        out << "]";
        if (m_pretty) {
            out << "\n";
        }
    }

private:
    bool m_pretty;
};

// ========== Port Statistics Formatters ==========

// Text formatter for PortResult
class PortTextFormatter : public OutputFormatter<PortResult> {
public:
    void format(const PortResult& results, std::ostream& out, bool no_header = false) override {
        if (!no_header) {
            out << std::left
                << std::setw(8) << "PORT"
                << std::setw(12) << "FLOWS"
                << std::setw(16) << "TX_BYTES"
                << std::setw(16) << "RX_BYTES"
                << std::setw(16) << "TOTAL_BYTES"
                << std::setw(12) << "TX_PACKETS"
                << std::setw(12) << "RX_PACKETS"
                << std::setw(12) << "TOTAL_PACKETS"
                << "\n";
        }

        for (const auto& [port, stat] : results.m_port_stats) {
            out << std::left
                << std::setw(8) << stat.m_port
                << std::setw(12) << stat.m_flow_count
                << std::setw(16) << stat.m_tx_bytes
                << std::setw(16) << stat.m_rx_bytes
                << std::setw(16) << stat.total_bytes()
                << std::setw(12) << stat.m_tx_packets
                << std::setw(12) << stat.m_rx_packets
                << std::setw(12) << stat.total_packets()
                << "\n";
        }
    }
};

// CSV formatter for PortResult
class PortCSVFormatter : public OutputFormatter<PortResult> {
public:
    void format(const PortResult& results, std::ostream& out, bool no_header = false) override {
        if (!no_header) {
            out << "port,flows,tx_bytes,rx_bytes,total_bytes,tx_packets,rx_packets,total_packets\n";
        }

        for (const auto& [port, stat] : results.m_port_stats) {
            out << stat.m_port << ","
                << stat.m_flow_count << ","
                << stat.m_tx_bytes << ","
                << stat.m_rx_bytes << ","
                << stat.total_bytes() << ","
                << stat.m_tx_packets << ","
                << stat.m_rx_packets << ","
                << stat.total_packets() << "\n";
        }
    }
};

// JSON formatter for PortResult
class PortJSONFormatter : public OutputFormatter<PortResult> {
public:
    explicit PortJSONFormatter(bool pretty = false)
        : m_pretty(pretty)
    {}

    void format(const PortResult& results, std::ostream& out, bool no_header = false) override {
        const std::string indent1 = m_pretty ? "  " : "";
        const std::string indent2 = m_pretty ? "    " : "";
        const std::string nl = m_pretty ? "\n" : "";

        out << "[" << nl;

        size_t count = results.m_port_stats.size();
        size_t i = 0;
        for (const auto& [port, stat] : results.m_port_stats) {
            bool last = (i == count - 1);

            out << indent1 << "{" << nl;
            out << indent2 << "\"port\": " << stat.m_port << "," << nl;
            out << indent2 << "\"flows\": " << stat.m_flow_count << "," << nl;
            out << indent2 << "\"tx_bytes\": " << stat.m_tx_bytes << "," << nl;
            out << indent2 << "\"rx_bytes\": " << stat.m_rx_bytes << "," << nl;
            out << indent2 << "\"total_bytes\": " << stat.total_bytes() << "," << nl;
            out << indent2 << "\"tx_packets\": " << stat.m_tx_packets << "," << nl;
            out << indent2 << "\"rx_packets\": " << stat.m_rx_packets << "," << nl;
            out << indent2 << "\"total_packets\": " << stat.total_packets() << nl;
            out << indent1 << "}" << (last ? "" : ",") << nl;

            ++i;
        }

        out << "]" << nl;
    }

private:
    bool m_pretty;
};

// Factory function to create appropriate formatter - CollectResult specialization
template<typename ResultType>
std::unique_ptr<OutputFormatter<ResultType>> create_formatter(OutputFormat format);

template<>
inline std::unique_ptr<OutputFormatter<CollectResult>> create_formatter<CollectResult>(OutputFormat format) {
    switch (format) {
    case OutputFormat::TEXT:
        return std::make_unique<TextFormatter>();
    case OutputFormat::CSV:
        return std::make_unique<CSVFormatter>();
    case OutputFormat::JSON:
        return std::make_unique<JSONFormatter>(false);
    case OutputFormat::JSON_PRETTY:
        return std::make_unique<JSONFormatter>(true);
    default:
        throw std::runtime_error("Unknown output format");
    }
}

// Factory function to create appropriate formatter - PortResult specialization
template<>
inline std::unique_ptr<OutputFormatter<PortResult>> create_formatter<PortResult>(OutputFormat format) {
    switch (format) {
    case OutputFormat::TEXT:
        return std::make_unique<PortTextFormatter>();
    case OutputFormat::CSV:
        return std::make_unique<PortCSVFormatter>();
    case OutputFormat::JSON:
        return std::make_unique<PortJSONFormatter>(false);
    case OutputFormat::JSON_PRETTY:
        return std::make_unique<PortJSONFormatter>(true);
    default:
        throw std::runtime_error("Unknown output format");
    }
}

} // namespace flowstats
