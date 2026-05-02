#pragma once
#include "../iso8583/iso_message.h"
#include <string>
#include <mutex>
#include <fstream>

class TransactionRepository {
public:
    static TransactionRepository& get();
    void save(const IsoMessage& req, const IsoMessage& res);

private:
    TransactionRepository();
    ~TransactionRepository();
    std::mutex mtx_;
    std::ofstream file_;
};
