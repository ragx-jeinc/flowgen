#ifndef FLOWDUMP_TIMESTAMP_CHUNKER_HPP
#define FLOWDUMP_TIMESTAMP_CHUNKER_HPP

#include "enhanced_flow.hpp"
#include <map>
#include <vector>
#include <cstdint>

namespace flowdump {

/**
 * Timestamp-based flow chunker for merging multi-threaded streams
 *
 * Groups flows into time-based chunks and outputs complete chunks
 * in order. A chunk is considered complete when we receive flows
 * from a later time window.
 */
class TimestampChunker {
public:
    /**
     * Constructor
     * @param chunk_duration_ns Duration of each chunk in nanoseconds
     */
    explicit TimestampChunker(uint64_t chunk_duration_ns);

    /**
     * Add a flow to the chunker
     */
    void add_flow(const EnhancedFlowRecord& flow);

    /**
     * Check if there's a complete chunk ready to be retrieved
     */
    bool has_complete_chunk() const;

    /**
     * Get the next complete chunk
     * Returns empty vector if no complete chunk available
     */
    std::vector<EnhancedFlowRecord> get_complete_chunk();

    /**
     * Flush all remaining chunks (call at end of processing)
     */
    std::vector<std::vector<EnhancedFlowRecord>> flush_all();

    /**
     * Get current number of buffered chunks
     */
    size_t chunk_count() const { return chunks_.size(); }

    /**
     * Get total number of flows buffered
     */
    size_t flow_count() const;

private:
    uint64_t chunk_duration_ns_;
    std::map<uint64_t, std::vector<EnhancedFlowRecord>> chunks_;
    uint64_t oldest_chunk_id_;
    bool has_oldest_;
};

} // namespace flowdump

#endif // FLOWDUMP_TIMESTAMP_CHUNKER_HPP
