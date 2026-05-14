#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>


using namespace std;

class MetricsRegistry {
public:
    static MetricsRegistry& get();
    void record_transaction();
    void record_latency(double ms);
    void record_response_code(const string& code);
    void print_metrics();

private:
    MetricsRegistry() = default;
    atomic<uint64_t> total_tx_{0};
    atomic<double> total_latency_{0.0};
    map<string, uint64_t> response_codes_;
    mutex mtx_;
};
