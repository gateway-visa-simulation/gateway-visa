#include "transaction_manager.h"
#include "../common/logger.h"

using namespace std;

TransactionManager& TransactionManager::get() {
    static TransactionManager instance;
    return instance;
}

bool TransactionManager::is_duplicate(const string& key) {
    lock_guard<mutex> lock(mtx_);
    cleanup_expired();
    return active_transactions_.find(key) != active_transactions_.end();
}

void TransactionManager::add_transaction(const string& key) {
    lock_guard<mutex> lock(mtx_);
    active_transactions_[key] = {chrono::steady_clock::now()};
    LOG_DEBUG("Added transaction {} to active list", key);
}

void TransactionManager::cleanup_expired() {
    auto now = chrono::steady_clock::now();
    for (auto it = active_transactions_.begin(); it != active_transactions_.end(); ) {
        if (now - it->second.timestamp > chrono::minutes(EXPIRY_MINUTES)) {
            LOG_DEBUG("Removing expired transaction {}", it->first);
            it = active_transactions_.erase(it);
        } else {
            ++it;
        }
    }
}
