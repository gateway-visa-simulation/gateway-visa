#include "transaction_repository.h"
#include <nlohmann/json.hpp>
#include "../common/logger.h"

TransactionRepository& TransactionRepository::get() {
    static TransactionRepository instance;
    return instance;
}

TransactionRepository::TransactionRepository() {
    file_.open("transactions.log", std::ios::app);
    if (!file_.is_open()) {
        LOG_ERROR("Failed to open transactions.log");
    }
}

TransactionRepository::~TransactionRepository() {
    if (file_.is_open()) {
        file_.close();
    }
}

void TransactionRepository::save(const IsoMessage& req, const IsoMessage& res) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!file_.is_open()) return;

    nlohmann::json j;
    j["timestamp"] = std::time(nullptr);
    j["stan"] = req.get_field(11);
    j["mti_req"] = req.mti;
    j["mti_res"] = res.mti;
    j["response_code"] = res.get_field(39);
    j["amount"] = req.get_field(4);

    file_ << j.dump() << std::endl;
}
