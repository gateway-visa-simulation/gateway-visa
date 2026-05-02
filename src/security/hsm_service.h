#pragma once
#include <string>
#include <map>
#include <mutex>

class HsmService {
public:
    static HsmService& get();
    bool verify_pin(const std::string& pan, const std::string& pin_block);
    std::string tokenize_pan(const std::string& pan);
    std::string detokenize(const std::string& token);

private:
    HsmService() = default;
    std::map<std::string, std::string> token_map_;
    std::map<std::string, std::string> pan_map_;
    std::mutex mtx_;
    uint64_t token_counter_ = 1000000000000000ULL;
};
