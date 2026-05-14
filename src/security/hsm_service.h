#pragma once
#include <string>
#include <map>
#include <mutex>

using namespace std;

class HsmService {
public:
    static HsmService& get();
    bool verify_pin(const string& pan, const string& pin_block);
    string tokenize_pan(const string& pan);
    string detokenize(const string& token);

private:
    HsmService() = default;
    map<string, string> token_map_;
    map<string, string> pan_map_;
    mutex mtx_;
    uint64_t token_counter_ = 1000000000000000ULL;
};
