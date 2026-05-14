#pragma once
#include <string>
#include <map>
#include <mutex>
#include <chrono>

using namespace std;

class CircuitBreaker {
public:
    static CircuitBreaker& get();
    bool can_request(const string& issuer);
    void record_success(const string& issuer);
    void record_failure(const string& issuer);

private:
    CircuitBreaker() = default;

    struct State {
        int failures = 0;
        bool open = false;
        chrono::steady_clock::time_point next_attempt;
    };

    map<string, State> states_;
    mutex mtx_;
    const int MAX_FAILURES = 3;
    const int TIMEOUT_MS = 5000;
};
