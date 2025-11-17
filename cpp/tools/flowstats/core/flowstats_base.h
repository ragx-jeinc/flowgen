#pragma once

#include "progress_tracker.h"
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>
#include <string>

namespace flowstats {

// Timestamp range for progress tracking
struct TimestampRange {
    uint64_t start_ns;
    uint64_t end_ns;
};

// Per-thread data structure (thread-local, no locking needed)
struct PerThreadData {
    uint32_t m_thread_id;
    std::atomic<size_t> m_flows_generated;
    std::atomic<uint64_t> m_bytes_generated;
    std::atomic<bool> m_done;

    PerThreadData()
        : m_thread_id(0)
        , m_flows_generated(0)
        , m_bytes_generated(0)
        , m_done(false)
    {}

    explicit PerThreadData(uint32_t id)
        : m_thread_id(id)
        , m_flows_generated(0)
        , m_bytes_generated(0)
        , m_done(false)
    {}
};

// Base class template for all flowstats subcommands
// Uses Template Method Pattern for common workflow
template<typename ResultType>
class FlowStatsCommand {
protected:
    // Common member variables (m_ prefix)
    std::string m_config_file;
    size_t m_num_threads;
    size_t m_flows_per_thread;

    // Threading
    std::vector<std::thread> m_threads;
    std::vector<std::unique_ptr<PerThreadData>> m_thread_data;
    std::atomic<bool> m_shutdown_requested;

    // Statistics
    std::atomic<uint64_t> m_total_flows;
    std::atomic<uint64_t> m_total_bytes;

    // Progress tracking
    std::unique_ptr<ProgressTracker> m_progress_tracker;
    bool m_show_progress;
    ProgressStyle m_progress_style;

public:
    FlowStatsCommand()
        : m_num_threads(10)
        , m_flows_per_thread(10000)
        , m_shutdown_requested(false)
        , m_total_flows(0)
        , m_total_bytes(0)
        , m_show_progress(true)
        , m_progress_style(ProgressStyle::BAR)
    {}

    virtual ~FlowStatsCommand() = default;

    // Main execution workflow (Template Method Pattern)
    int execute() {
        // Step 1: Validate options
        if (!validate_options()) {
            std::cerr << "Error: Invalid options\n";
            return 1;
        }

        // Step 2: Initialize
        try {
            initialize();
        } catch (const std::exception& e) {
            std::cerr << "Error during initialization: " << e.what() << "\n";
            return 1;
        }

        // Step 3: Initialize progress tracker
        if (m_show_progress) {
            initialize_progress_tracker();
            m_progress_tracker->start();
        }

        // Step 4: Start worker threads
        start_worker_threads();

        // Step 5: Collect results
        ResultType results;
        try {
            results = collect_results();
        } catch (const std::exception& e) {
            std::cerr << "Error during collection: " << e.what() << "\n";
            m_shutdown_requested.store(true, std::memory_order_release);
            wait_for_completion();
            if (m_progress_tracker) {
                m_progress_tracker->stop();
            }
            return 1;
        }

        // Step 6: Wait for completion
        wait_for_completion();

        // Step 7: Stop progress tracker
        if (m_progress_tracker) {
            m_progress_tracker->stop();
        }

        // Step 8: Output results
        try {
            output_results(results);
        } catch (const std::exception& e) {
            std::cerr << "Error during output: " << e.what() << "\n";
            return 1;
        }

        // Step 9: Summary
        if (m_show_progress) {
            output_summary();
        }

        return 0;
    }

    // Virtual functions - subclasses MUST implement
    virtual bool validate_options() = 0;
    virtual void initialize() = 0;
    virtual void run_worker_thread(size_t thread_id) = 0;
    virtual ResultType collect_results() = 0;
    virtual void output_results(const ResultType& results) = 0;

    // Virtual functions - subclasses MAY override
    virtual TimestampRange get_timestamp_range() const {
        // Default: 1 second duration starting from Unix epoch 2024-01-01
        return {1704067200000000000ULL, 1704067201000000000ULL};
    }

protected:
    // Common helper functions
    void start_worker_threads() {
        // Create per-thread data using unique_ptr (atomics are not movable/copyable)
        for (size_t i = 0; i < m_num_threads; ++i) {
            m_thread_data.push_back(std::make_unique<PerThreadData>(static_cast<uint32_t>(i)));
        }

        // Start threads
        for (size_t i = 0; i < m_num_threads; ++i) {
            m_threads.emplace_back(&FlowStatsCommand::run_worker_thread, this, i);
        }
    }

    void wait_for_completion() {
        for (auto& t : m_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    // Helper to access thread data
    PerThreadData& get_thread_data(size_t thread_id) {
        return *m_thread_data[thread_id];
    }

    void initialize_progress_tracker() {
        auto range = get_timestamp_range();

        m_progress_tracker = std::make_unique<ProgressTracker>(
            range.start_ns,
            range.end_ns,
            m_num_threads,
            m_progress_style,
            1000  // 1 second update interval
        );
    }

    void output_summary() {
        std::cerr << "\nSummary:\n";
        std::cerr << "  Threads: " << m_num_threads << "\n";
        std::cerr << "  Flows processed: " << m_total_flows.load() << "\n";
        std::cerr << "  Total bytes: " << m_total_bytes.load() << "\n";
    }

    // Check if shutdown requested
    bool is_shutdown_requested() const {
        return m_shutdown_requested.load(std::memory_order_acquire);
    }

    // Update progress (called from worker threads)
    void update_progress(size_t thread_id, uint64_t timestamp, uint64_t bytes) {
        if (m_progress_tracker) {
            m_progress_tracker->update_timestamp(thread_id, timestamp);
            m_progress_tracker->add_bytes(bytes);
        }
    }

    void increment_flow_count(uint64_t count = 1) {
        m_total_flows.fetch_add(count, std::memory_order_relaxed);
        if (m_progress_tracker) {
            m_progress_tracker->add_flows(count);
        }
    }

    void increment_byte_count(uint64_t bytes) {
        m_total_bytes.fetch_add(bytes, std::memory_order_relaxed);
    }
};

} // namespace flowstats
