#pragma once
#include "../iso8583/iso_message.h"
#include <string>
#include <mutex>
#include <fstream>

using namespace std;

class TransactionRepository {
public:
    static TransactionRepository& get();
    void save(const IsoMessage& req, const IsoMessage& res);

private:
    TransactionRepository();
    ~TransactionRepository();
    mutex mtx_;
    ofstream file_;
};
