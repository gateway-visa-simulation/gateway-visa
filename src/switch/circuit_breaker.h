#pragma once
#include <string>
#include <map>
#include <mutex>
#include <chrono>

class CircuitBreaker {
public:
    static CircuitBreaker& get();
    bool can_request(const std::string& issuer);
    void record_success(const std::string& issuer);
    void record_failure(const std::string& issuer);

private:
    CircuitBreaker() = default;

    struct State {
        int failures = 0;
        bool open = false;
        std::chrono::steady_clock::time_point next_attempt;
    };

    std::map<std::string, State> states_;
    std::mutex mtx_;
    const int MAX_FAILURES = 3;
    const int TIMEOUT_MS = 5000;
};
