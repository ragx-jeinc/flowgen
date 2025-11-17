#include "flow_collector.hpp"
#include <iostream>

namespace flowdump {

FlowCollector::FlowCollector(ThreadSafeQueue<EnhancedFlowRecord>& input_queue,
                             uint64_t chunk_duration_ns,
                             FlowFormatter& formatter,
                             std::ostream& output,
                             size_t num_generators,
                             bool suppress_header)
    : input_queue_(input_queue),
      chunker_(chunk_duration_ns),
      formatter_(formatter),
      output_(output),
      num_generators_(num_generators),
      generators_done_(0),
      flows_collected_(0),
      suppress_header_(suppress_header),
      header_printed_(false),
      first_flow_(true) {
}

void FlowCollector::run() {
    // Print header if needed
    if (!suppress_header_ && !header_printed_) {
        std::string header = formatter_.format_header(suppress_header_);
        if (!header.empty()) {
            output_ << header << "\n";
            header_printed_ = true;
        }
    }

    // Process flows from queue
    while (true) {
        // Try to get flow with timeout
        auto flow_opt = input_queue_.try_pop(std::chrono::milliseconds(10));

        if (flow_opt.has_value()) {
            // Add flow to chunker
            chunker_.add_flow(*flow_opt);
            flows_collected_++;

            // Process complete chunks
            process_complete_chunks();
        } else {
            // No flow received (timeout or done)
            // Check if all generators are done
            if (generators_done_ >= num_generators_ && input_queue_.empty()) {
                break;
            }
        }
    }

    // Flush remaining chunks
    flush_remaining_chunks();

    // Print footer if needed
    std::string footer = formatter_.format_footer();
    if (!footer.empty()) {
        output_ << footer;
    }
}

void FlowCollector::generator_done() {
    generators_done_++;
}

void FlowCollector::process_complete_chunks() {
    while (chunker_.has_complete_chunk()) {
        auto chunk = chunker_.get_complete_chunk();
        if (!chunk.empty()) {
            output_chunk(chunk);
        }
    }
}

void FlowCollector::flush_remaining_chunks() {
    auto remaining_chunks = chunker_.flush_all();
    for (auto& chunk : remaining_chunks) {
        if (!chunk.empty()) {
            output_chunk(chunk);
        }
    }
}

void FlowCollector::output_chunk(std::vector<EnhancedFlowRecord>& flows) {
    // Sort flows according to configured field
    formatter_.sort_flows(flows);

    // Output each flow
    for (size_t i = 0; i < flows.size(); ++i) {
        bool is_last = (i == flows.size() - 1) &&
                       (generators_done_ >= num_generators_) &&
                       (input_queue_.empty()) &&
                       (chunker_.chunk_count() == 0);

        std::string formatted = formatter_.format_flow(flows[i], is_last);
        output_ << formatted << "\n";
        first_flow_ = false;
    }
}

} // namespace flowdump
