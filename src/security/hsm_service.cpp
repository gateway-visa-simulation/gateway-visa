#include "hsm_service.h"
#include "../common/logger.h"

HsmService& HsmService::get() {
    static HsmService instance;
    return instance;
}

bool HsmService::verify_pin(const std::string& pan, const std::string& pin_block) {
    // Stub implementation: valid PIN is anything ending in "1234" or if empty
    if (pin_block.empty()) return true; // Signature-based
    return pin_block.find("1234") != std::string::npos;
}

std::string HsmService::tokenize_pan(const std::string& pan) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (pan_map_.find(pan) != pan_map_.end()) {
        return pan_map_[pan];
    }

    std::string token = std::to_string(token_counter_++);
    pan_map_[pan] = token;
    token_map_[token] = pan;
    LOG_DEBUG("Tokenized PAN. Token: {}", token);
    return token;
}

std::string HsmService::detokenize(const std::string& token) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (token_map_.find(token) != token_map_.end()) {
        return token_map_[token];
    }
    return "";
}
