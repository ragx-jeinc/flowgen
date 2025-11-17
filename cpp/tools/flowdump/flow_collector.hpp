#ifndef FLOWDUMP_FLOW_COLLECTOR_HPP
#define FLOWDUMP_FLOW_COLLECTOR_HPP

#include "enhanced_flow.hpp"
#include "thread_safe_queue.hpp"
#include "timestamp_chunker.hpp"
#include "flow_formatter.hpp"
#include <ostream>
#include <atomic>
#include <chrono>

namespace flowdump {

/**
 * Flow collector thread
 * Consumes flows from queue, chunks by timestamp, sorts, and outputs
 */
class FlowCollector {
public:
    FlowCollector(ThreadSafeQueue<EnhancedFlowRecord>& input_queue,
                  uint64_t chunk_duration_ns,
                  FlowFormatter& formatter,
                  std::ostream& output,
                  size_t num_generators,
                  bool suppress_header = false);

    /**
     * Run the collector (call in thread)
     */
    void run();

    /**
     * Notify that a generator is done
     */
    void generator_done();

    /**
     * Get total flows collected
     */
    uint64_t flows_collected() const { return flows_collected_; }

private:
    /**
     * Process and output complete chunks
     */
    void process_complete_chunks();

    /**
     * Flush all remaining chunks at the end
     */
    void flush_remaining_chunks();

    /**
     * Output a chunk of flows
     */
    void output_chunk(std::vector<EnhancedFlowRecord>& flows);

    ThreadSafeQueue<EnhancedFlowRecord>& input_queue_;
    TimestampChunker chunker_;
    FlowFormatter& formatter_;
    std::ostream& output_;
    size_t num_generators_;
    std::atomic<size_t> generators_done_;
    uint64_t flows_collected_;
    bool suppress_header_;
    bool header_printed_;
    bool first_flow_;
};

} // namespace flowdump

#endif // FLOWDUMP_FLOW_COLLECTOR_HPP
