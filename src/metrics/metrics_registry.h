#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>

class MetricsRegistry {
public:
    static MetricsRegistry& get();
    void record_transaction();
    void record_latency(double ms);
    void record_response_code(const std::string& code);
    void print_metrics();

private:
    MetricsRegistry() = default;
    std::atomic<uint64_t> total_tx_{0};
    std::atomic<double> total_latency_{0.0};
    std::map<std::string, uint64_t> response_codes_;
    std::mutex mtx_;
};
