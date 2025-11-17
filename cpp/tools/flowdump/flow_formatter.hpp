#ifndef FLOWDUMP_FLOW_FORMATTER_HPP
#define FLOWDUMP_FLOW_FORMATTER_HPP

#include "enhanced_flow.hpp"
#include <vector>
#include <string>

namespace flowdump {

enum class OutputFormat {
    PLAIN_TEXT,
    CSV,
    JSON
};

enum class SortField {
    TIMESTAMP,
    STREAM_ID,
    SOURCE_IP,
    DESTINATION_IP,
    BYTE_COUNT,
    PACKET_COUNT
};

/**
 * Formatter for flow output with sorting capabilities
 */
class FlowFormatter {
public:
    FlowFormatter(OutputFormat format, SortField sort_field, bool pretty = false);

    /**
     * Sort flows according to configured field
     */
    void sort_flows(std::vector<EnhancedFlowRecord>& flows) const;

    /**
     * Format header (for CSV and plain text)
     */
    std::string format_header(bool suppress_header = false) const;

    /**
     * Format a single flow
     */
    std::string format_flow(const EnhancedFlowRecord& flow, bool is_last = false) const;

    /**
     * Format footer (for JSON)
     */
    std::string format_footer() const;

    /**
     * Parse output format from string
     */
    static OutputFormat parse_format(const std::string& format_str);

    /**
     * Parse sort field from string
     */
    static SortField parse_sort_field(const std::string& field_str);

private:
    OutputFormat format_;
    SortField sort_field_;
    bool pretty_;
};

} // namespace flowdump

#endif // FLOWDUMP_FLOW_FORMATTER_HPP
