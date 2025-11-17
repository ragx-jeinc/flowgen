#include "generator_worker.hpp"
#include <iostream>

namespace flowdump {

GeneratorWorker::GeneratorWorker(uint32_t stream_id,
                                 const flowgen::GeneratorConfig& config,
                                 ThreadSafeQueue<EnhancedFlowRecord>& output_queue,
                                 uint64_t flows_to_generate)
    : stream_id_(stream_id),
      config_(config),
      output_queue_(output_queue),
      flows_to_generate_(flows_to_generate),
      flows_generated_(0) {
}

void GeneratorWorker::run() {
    // Create generator for this thread
    flowgen::FlowGenerator generator;

    // Modify config to generate exact number of flows for this worker
    flowgen::GeneratorConfig worker_config = config_;
    worker_config.max_flows = flows_to_generate_;

    if (!generator.initialize(worker_config)) {
        std::cerr << "Failed to initialize generator for stream "
                  << std::hex << stream_id_ << std::dec << std::endl;
        return;
    }

    // Generate flows
    flowgen::FlowRecord basic_flow;
    while (generator.next(basic_flow)) {
        EnhancedFlowRecord enhanced = enhance_flow(basic_flow);
        output_queue_.push(std::move(enhanced));
        flows_generated_++;
    }
}

EnhancedFlowRecord GeneratorWorker::enhance_flow(const flowgen::FlowRecord& basic_flow) {
    EnhancedFlowRecord enhanced;

    enhanced.stream_id = stream_id_;
    enhanced.timestamp = basic_flow.timestamp;  // Keep for chunking (first packet time)
    enhanced.source_ip = basic_flow.source_ip;
    enhanced.destination_ip = basic_flow.destination_ip;
    enhanced.source_port = basic_flow.source_port;
    enhanced.destination_port = basic_flow.destination_port;
    enhanced.protocol = basic_flow.protocol;

    // Generate realistic packet count, byte count, and flow duration
    FlowStats stats = generate_flow_stats(
        basic_flow.packet_length,
        basic_flow.protocol,
        basic_flow.destination_port
    );

    enhanced.packet_count = stats.packet_count;
    enhanced.byte_count = stats.byte_count;

    // Set first and last packet timestamps
    enhanced.first_timestamp = basic_flow.timestamp;
    enhanced.last_timestamp = basic_flow.timestamp + stats.duration_ns;

    return enhanced;
}

} // namespace flowdump
