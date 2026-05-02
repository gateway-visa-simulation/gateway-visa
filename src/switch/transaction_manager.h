#pragma once
#include <string>
#include <map>
#include <mutex>
#include <chrono>

class TransactionManager {
public:
    static TransactionManager& get();
    bool is_duplicate(const std::string& key);
    void add_transaction(const std::string& key);

private:
    TransactionManager() = default;

    struct TransactionData {
        std::chrono::steady_clock::time_point timestamp;
    };

    std::map<std::string, TransactionData> active_transactions_;
    std::mutex mtx_;
    const int EXPIRY_MINUTES = 5;

    void cleanup_expired();
};
