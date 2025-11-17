#pragma once

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace flowstats {

// Progress display styles
enum class ProgressStyle {
    BAR,        // [====>    ] 45.2% | Time: ... | ETA: ...
    SIMPLE,     // Progress: 45.2% - 150K flows - ETA: 2m 15s
    SPINNER,    // / - \ | animation with percentage
    NONE        // No progress display
};

// Progress tracker - monitors timestamp progression and statistics
class ProgressTracker {
public:
    ProgressTracker(uint64_t start_ts, uint64_t end_ts, size_t num_threads,
                   ProgressStyle style = ProgressStyle::BAR,
                   uint32_t update_interval_ms = 1000);

    ~ProgressTracker();

    // Disable copy/move
    ProgressTracker(const ProgressTracker&) = delete;
    ProgressTracker& operator=(const ProgressTracker&) = delete;

    // Start/stop progress monitoring
    void start();
    void stop();

    // Update current timestamp for a thread (lock-free)
    void update_timestamp(size_t thread_id, uint64_t current_ts);

    // Update flow/byte counts (lock-free)
    void add_flows(uint64_t count);
    void add_bytes(uint64_t bytes);

    // Get current progress percentage
    double get_progress_percentage() const;

    // Get current processing timestamp (minimum across threads)
    uint64_t get_current_timestamp() const;

    // Calculate ETA
    std::chrono::seconds get_eta() const;

    // Get throughput (flows/sec)
    double get_throughput() const;

    // Get bandwidth (Gbps)
    double get_bandwidth_gbps() const;

private:
    // Time range
    uint64_t m_start_timestamp_ns;
    uint64_t m_end_timestamp_ns;
    uint64_t m_total_duration_ns;

    // Per-thread current timestamps (lock-free)
    std::vector<std::atomic<uint64_t>> m_thread_current_timestamps;

    // Statistics
    std::atomic<uint64_t> m_total_flows_processed;
    std::atomic<uint64_t> m_total_bytes_processed;

    // Timing
    std::chrono::steady_clock::time_point m_start_time;
    std::atomic<bool> m_active;

    // Progress display
    ProgressStyle m_style;
    uint32_t m_update_interval_ms;
    std::thread m_progress_thread;
    std::atomic<bool> m_shutdown;
    size_t m_spinner_frame;

    // Progress display loop
    void progress_display_loop();
    void display_progress();
    void display_progress_final();

    // Display helpers
    std::string build_progress_bar(double percentage, size_t width) const;
    std::string build_spinner();
    std::string format_timestamp(uint64_t ts_ns) const;
    std::string format_duration(std::chrono::seconds duration) const;
    std::string format_count(uint64_t count) const;
};

} // namespace flowstats
