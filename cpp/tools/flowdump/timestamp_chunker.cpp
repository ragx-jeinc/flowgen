#include "timestamp_chunker.hpp"
#include <algorithm>

namespace flowdump {

TimestampChunker::TimestampChunker(uint64_t chunk_duration_ns)
    : chunk_duration_ns_(chunk_duration_ns),
      oldest_chunk_id_(0),
      has_oldest_(false) {
}

void TimestampChunker::add_flow(const EnhancedFlowRecord& flow) {
    uint64_t chunk_id = flow.timestamp / chunk_duration_ns_;
    chunks_[chunk_id].push_back(flow);

    if (!has_oldest_) {
        oldest_chunk_id_ = chunk_id;
        has_oldest_ = true;
    }
}

bool TimestampChunker::has_complete_chunk() const {
    if (!has_oldest_ || chunks_.empty()) {
        return false;
    }

    // A chunk is complete when we have data from a later chunk
    // This ensures all flows in the chunk have arrived
    uint64_t latest_chunk = chunks_.rbegin()->first;
    return (latest_chunk > oldest_chunk_id_);
}

std::vector<EnhancedFlowRecord> TimestampChunker::get_complete_chunk() {
    if (!has_complete_chunk()) {
        return {};
    }

    auto it = chunks_.find(oldest_chunk_id_);
    if (it == chunks_.end()) {
        // No data for this chunk, move to next
        oldest_chunk_id_++;
        return {};
    }

    std::vector<EnhancedFlowRecord> result = std::move(it->second);
    chunks_.erase(it);
    oldest_chunk_id_++;

    return result;
}

std::vector<std::vector<EnhancedFlowRecord>> TimestampChunker::flush_all() {
    std::vector<std::vector<EnhancedFlowRecord>> result;

    // Get all remaining chunks in order
    for (auto& [chunk_id, flows] : chunks_) {
        if (!flows.empty()) {
            result.push_back(std::move(flows));
        }
    }

    chunks_.clear();
    has_oldest_ = false;

    return result;
}

size_t TimestampChunker::flow_count() const {
    size_t count = 0;
    for (const auto& [chunk_id, flows] : chunks_) {
        count += flows.size();
    }
    return count;
}

} // namespace flowdump
