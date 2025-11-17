#include "progress_tracker.h"
#include <iostream>
#include <algorithm>

namespace flowstats {

ProgressTracker::ProgressTracker(uint64_t start_ts, uint64_t end_ts, size_t num_threads,
                                ProgressStyle style, uint32_t update_interval_ms)
    : m_start_timestamp_ns(start_ts)
    , m_end_timestamp_ns(end_ts)
    , m_total_duration_ns(end_ts - start_ts)
    , m_thread_current_timestamps(num_threads)
    , m_total_flows_processed(0)
    , m_total_bytes_processed(0)
    , m_active(false)
    , m_style(style)
    , m_update_interval_ms(update_interval_ms)
    , m_shutdown(false)
    , m_spinner_frame(0)
{
    // Initialize all thread timestamps to start
    for (auto& ts : m_thread_current_timestamps) {
        ts.store(start_ts, std::memory_order_relaxed);
    }
}

ProgressTracker::~ProgressTracker() {
    stop();
}

void ProgressTracker::start() {
    m_start_time = std::chrono::steady_clock::now();
    m_active.store(true, std::memory_order_release);

    // Launch progress display thread
    if (m_style != ProgressStyle::NONE) {
        m_progress_thread = std::thread(&ProgressTracker::progress_display_loop, this);
    }
}

void ProgressTracker::stop() {
    m_shutdown.store(true, std::memory_order_release);
    if (m_progress_thread.joinable()) {
        m_progress_thread.join();
    }
}

void ProgressTracker::update_timestamp(size_t thread_id, uint64_t current_ts) {
    if (thread_id < m_thread_current_timestamps.size()) {
        m_thread_current_timestamps[thread_id].store(current_ts, std::memory_order_relaxed);
    }
}

void ProgressTracker::add_flows(uint64_t count) {
    m_total_flows_processed.fetch_add(count, std::memory_order_relaxed);
}

void ProgressTracker::add_bytes(uint64_t bytes) {
    m_total_bytes_processed.fetch_add(bytes, std::memory_order_relaxed);
}

double ProgressTracker::get_progress_percentage() const {
    // Find minimum (slowest) timestamp across all threads
    uint64_t min_ts = m_end_timestamp_ns;
    for (const auto& ts : m_thread_current_timestamps) {
        uint64_t current = ts.load(std::memory_order_relaxed);
        if (current < min_ts) {
            min_ts = current;
        }
    }

    if (min_ts >= m_end_timestamp_ns) {
        return 100.0;
    }

    if (min_ts <= m_start_timestamp_ns) {
        return 0.0;
    }

    uint64_t elapsed = min_ts - m_start_timestamp_ns;
    return (elapsed * 100.0) / m_total_duration_ns;
}

uint64_t ProgressTracker::get_current_timestamp() const {
    uint64_t min_ts = m_end_timestamp_ns;
    for (const auto& ts : m_thread_current_timestamps) {
        uint64_t current = ts.load(std::memory_order_relaxed);
        if (current < min_ts) {
            min_ts = current;
        }
    }
    return min_ts;
}

std::chrono::seconds ProgressTracker::get_eta() const {
    double progress = get_progress_percentage();
    if (progress <= 0.0 || progress >= 100.0) {
        return std::chrono::seconds(0);
    }

    auto elapsed = std::chrono::steady_clock::now() - m_start_time;
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
        elapsed / (progress / 100.0)
    );

    return total_time - std::chrono::duration_cast<std::chrono::seconds>(elapsed);
}

double ProgressTracker::get_throughput() const {
    auto elapsed = std::chrono::steady_clock::now() - m_start_time;
    double elapsed_sec = std::chrono::duration<double>(elapsed).count();

    if (elapsed_sec < 0.001) {
        return 0.0;
    }

    return m_total_flows_processed.load(std::memory_order_relaxed) / elapsed_sec;
}

double ProgressTracker::get_bandwidth_gbps() const {
    auto elapsed = std::chrono::steady_clock::now() - m_start_time;
    double elapsed_sec = std::chrono::duration<double>(elapsed).count();

    if (elapsed_sec < 0.001) {
        return 0.0;
    }

    uint64_t bytes = m_total_bytes_processed.load(std::memory_order_relaxed);
    return (bytes * 8.0) / (elapsed_sec * 1e9);
}

void ProgressTracker::progress_display_loop() {
    using namespace std::chrono_literals;

    auto interval = std::chrono::milliseconds(m_update_interval_ms);

    while (!m_shutdown.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(interval);

        if (!m_active.load(std::memory_order_acquire)) {
            continue;
        }

        display_progress();
    }

    // Final update
    display_progress_final();
}

void ProgressTracker::display_progress() {
    double progress = get_progress_percentage();
    uint64_t current_ts = get_current_timestamp();
    auto eta = get_eta();
    double throughput = get_throughput();
    double bandwidth = get_bandwidth_gbps();

    std::string current_time = format_timestamp(current_ts);
    std::string eta_str = format_duration(eta);
    std::string flow_count = format_count(m_total_flows_processed.load(std::memory_order_relaxed));

    std::ostringstream oss;

    switch (m_style) {
    case ProgressStyle::BAR: {
        std::string bar = build_progress_bar(progress, 40);
        oss << "\r" << bar << " "
            << std::fixed << std::setprecision(1) << progress << "% | "
            << "Time: " << current_time << " | "
            << "ETA: " << eta_str << " | "
            << std::setprecision(0) << throughput << " flows/s | "
            << std::setprecision(2) << bandwidth << " Gbps"
            << std::flush;
        break;
    }

    case ProgressStyle::SIMPLE:
        oss << "\rProgress: " << std::fixed << std::setprecision(1) << progress << "% - "
            << flow_count << " flows - "
            << "ETA: " << eta_str
            << std::flush;
        break;

    case ProgressStyle::SPINNER: {
        std::string spinner = build_spinner();
        oss << "\r" << spinner << " "
            << std::fixed << std::setprecision(1) << progress << "% - "
            << flow_count << " flows - "
            << std::setprecision(0) << throughput << " flows/s"
            << std::flush;
        break;
    }

    case ProgressStyle::NONE:
        return;
    }

    std::cerr << oss.str();
}

void ProgressTracker::display_progress_final() {
    std::cerr << "\n";  // New line after progress bar
}

std::string ProgressTracker::build_progress_bar(double percentage, size_t width) const {
    size_t filled = static_cast<size_t>((percentage / 100.0) * width);
    filled = std::min(filled, width);

    std::string bar = "[";
    for (size_t i = 0; i < width; ++i) {
        if (i < filled) {
            bar += "=";
        } else if (i == filled && filled < width) {
            bar += ">";
        } else {
            bar += " ";
        }
    }
    bar += "]";

    return bar;
}

std::string ProgressTracker::build_spinner() {
    const char* frames = "|/-\\";
    std::string result(1, frames[m_spinner_frame]);
    m_spinner_frame = (m_spinner_frame + 1) % 4;
    return result;
}

std::string ProgressTracker::format_timestamp(uint64_t ts_ns) const {
    time_t seconds = ts_ns / 1000000000ULL;
    struct tm tm_info;
    gmtime_r(&seconds, &tm_info);

    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);

    return std::string(buffer);
}

std::string ProgressTracker::format_duration(std::chrono::seconds duration) const {
    auto secs = duration.count();

    if (secs < 60) {
        return std::to_string(secs) + "s";
    } else if (secs < 3600) {
        auto mins = secs / 60;
        auto rem_secs = secs % 60;
        return std::to_string(mins) + "m " + std::to_string(rem_secs) + "s";
    } else {
        auto hours = secs / 3600;
        auto mins = (secs % 3600) / 60;
        return std::to_string(hours) + "h " + std::to_string(mins) + "m";
    }
}

std::string ProgressTracker::format_count(uint64_t count) const {
    if (count < 1000) {
        return std::to_string(count);
    } else if (count < 1000000) {
        return std::to_string(count / 1000) + "K";
    } else if (count < 1000000000) {
        return std::to_string(count / 1000000) + "M";
    } else {
        return std::to_string(count / 1000000000) + "G";
    }
}

} // namespace flowstats
