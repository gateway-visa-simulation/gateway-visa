#include "metrics_registry.h"
#include "../common/logger.h"
#include <nlohmann/json.hpp>

MetricsRegistry& MetricsRegistry::get() {
    static MetricsRegistry instance;
    return instance;
}

void MetricsRegistry::record_transaction() {
    total_tx_++;
}

void MetricsRegistry::record_latency(double ms) {
    total_latency_ = total_latency_ + ms;
}

void MetricsRegistry::record_response_code(const string& code) {
    lock_guard<mutex> lock(mtx_);
    response_codes_[code]++;
}

void MetricsRegistry::print_metrics() {
    lock_guard<mutex> lock(mtx_);
    nlohmann::json j;
    j["total_transactions"] = total_tx_.load();
    double avg_latency = total_tx_.load() > 0 ? total_latency_.load() / total_tx_.load() : 0.0;
    j["avg_latency_ms"] = avg_latency;
    j["response_codes"] = response_codes_;

    LOG_INFO("METRICS: {}", j.dump());
}
