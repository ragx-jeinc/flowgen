#include "flow_formatter.hpp"
#include <algorithm>
#include <stdexcept>

namespace flowdump {

FlowFormatter::FlowFormatter(OutputFormat format, SortField sort_field, bool pretty)
    : format_(format), sort_field_(sort_field), pretty_(pretty) {
}

void FlowFormatter::sort_flows(std::vector<EnhancedFlowRecord>& flows) const {
    switch (sort_field_) {
    case SortField::TIMESTAMP:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      return a.timestamp < b.timestamp;
                  });
        break;

    case SortField::STREAM_ID:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.stream_id != b.stream_id)
                          return a.stream_id < b.stream_id;
                      return a.timestamp < b.timestamp;
                  });
        break;

    case SortField::SOURCE_IP:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.source_ip != b.source_ip)
                          return a.source_ip < b.source_ip;
                      return a.timestamp < b.timestamp;
                  });
        break;

    case SortField::DESTINATION_IP:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.destination_ip != b.destination_ip)
                          return a.destination_ip < b.destination_ip;
                      return a.timestamp < b.timestamp;
                  });
        break;

    case SortField::BYTE_COUNT:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.byte_count != b.byte_count)
                          return a.byte_count > b.byte_count;  // Descending
                      return a.timestamp < b.timestamp;
                  });
        break;

    case SortField::PACKET_COUNT:
        std::sort(flows.begin(), flows.end(),
                  [](const auto& a, const auto& b) {
                      if (a.packet_count != b.packet_count)
                          return a.packet_count > b.packet_count;  // Descending
                      return a.timestamp < b.timestamp;
                  });
        break;
    }
}

std::string FlowFormatter::format_header(bool suppress_header) const {
    if (suppress_header) {
        return "";
    }

    switch (format_) {
    case OutputFormat::PLAIN_TEXT:
        return EnhancedFlowRecord::plain_text_header();
    case OutputFormat::CSV:
        return EnhancedFlowRecord::csv_header();
    case OutputFormat::JSON:
        return pretty_ ? "[\n" : "[";
    default:
        return "";
    }
}

std::string FlowFormatter::format_flow(const EnhancedFlowRecord& flow, bool is_last) const {
    switch (format_) {
    case OutputFormat::PLAIN_TEXT:
        return flow.to_plain_text();
    case OutputFormat::CSV:
        return flow.to_csv();
    case OutputFormat::JSON:
        return flow.to_json(pretty_, is_last);
    default:
        return "";
    }
}

std::string FlowFormatter::format_footer() const {
    if (format_ == OutputFormat::JSON) {
        return pretty_ ? "]\n" : "]";
    }
    return "";
}

OutputFormat FlowFormatter::parse_format(const std::string& format_str) {
    std::string lower = format_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "text" || lower == "plain" || lower == "plain_text") {
        return OutputFormat::PLAIN_TEXT;
    } else if (lower == "csv") {
        return OutputFormat::CSV;
    } else if (lower == "json") {
        return OutputFormat::JSON;
    } else {
        throw std::runtime_error("Unknown output format: " + format_str);
    }
}

SortField FlowFormatter::parse_sort_field(const std::string& field_str) {
    std::string lower = field_str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "timestamp" || lower == "time" || lower == "ts") {
        return SortField::TIMESTAMP;
    } else if (lower == "stream_id" || lower == "stream" || lower == "sid") {
        return SortField::STREAM_ID;
    } else if (lower == "src_ip" || lower == "source_ip" || lower == "srcip") {
        return SortField::SOURCE_IP;
    } else if (lower == "dst_ip" || lower == "destination_ip" || lower == "dstip") {
        return SortField::DESTINATION_IP;
    } else if (lower == "bytes" || lower == "byte_count") {
        return SortField::BYTE_COUNT;
    } else if (lower == "packets" || lower == "packet_count" || lower == "pkts") {
        return SortField::PACKET_COUNT;
    } else {
        throw std::runtime_error("Unknown sort field: " + field_str);
    }
}

} // namespace flowdump
