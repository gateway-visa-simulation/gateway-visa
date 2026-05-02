#include "circuit_breaker.h"
#include "../common/logger.h"

CircuitBreaker& CircuitBreaker::get() {
    static CircuitBreaker instance;
    return instance;
}

bool CircuitBreaker::can_request(const std::string& issuer) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& state = states_[issuer];
    if (state.open) {
        if (std::chrono::steady_clock::now() > state.next_attempt) {
            state.open = false; // Half-open
            LOG_INFO("Circuit breaker half-open for {}", issuer);
            return true;
        }
        return false;
    }
    return true;
}

void CircuitBreaker::record_success(const std::string& issuer) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& state = states_[issuer];
    if (state.open || state.failures > 0) {
        state.open = false;
        state.failures = 0;
        LOG_INFO("Circuit breaker closed for {}", issuer);
    }
}

void CircuitBreaker::record_failure(const std::string& issuer) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto& state = states_[issuer];
    state.failures++;
    if (state.failures >= MAX_FAILURES) {
        state.open = true;
        state.next_attempt = std::chrono::steady_clock::now() + std::chrono::milliseconds(TIMEOUT_MS);
        LOG_WARN("Circuit breaker OPEN for {}", issuer);
    }
}
