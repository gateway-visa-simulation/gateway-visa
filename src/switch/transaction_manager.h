#pragma once
#include <string>
#include <map>
#include <mutex>
#include <chrono>

using namespace std;

class TransactionManager {
public:
    static TransactionManager& get();
    bool is_duplicate(const string& key);
    void add_transaction(const string& key);

private:
    TransactionManager() = default;

    struct TransactionData {
        chrono::steady_clock::time_point timestamp;
    };

    map<string, TransactionData> active_transactions_;
    mutex mtx_;
    const int EXPIRY_MINUTES = 5;

    void cleanup_expired();
};
