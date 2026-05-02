#include "hsm.h"
#include "pin_block.h"
#include "crypto.h"
#include "tokenization.h"
#include "../common/logger.h"

HSM& HSM::getInstance() {
    static HSM instance;
    return instance;
}

void HSM::init() {
    std::lock_guard<std::mutex> lock(mtx_);
    masterKey_ = "SUPER_SECRET_KEY_123456789";

    mockPinDatabase_["1234567890123456"] = "1234";
    mockPinDatabase_["1234567890123457"] = "4321";
    mockPinDatabase_["1234567890123458"] = "1111";

    LOG_INFO("HSM Simulado Inicializado com sucesso.");
}

bool HSM::validatePin(const std::string& pinBlockHex, const std::string& pan) {
    if (pinBlockHex.empty() || pan.empty()) {
        return false;
    }

    std::string expectedPin;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = mockPinDatabase_.find(pan);
        if (it != mockPinDatabase_.end()) {
            expectedPin = it->second;
        } else {
            LOG_WARN("HSM: PAN Nao encontrado no banco Mock.");
            return false;
        }
    }

    try {
        std::string extractedPin = PinBlock::extractIso0(pinBlockHex, pan);
        if (extractedPin == expectedPin) {
            LOG_INFO("HSM: PIN Validado com Sucesso.");
            return true;
        } else {
            LOG_WARN("HSM: PIN Incorreto.");
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("HSM: Erro ao validar PIN Block - {}", e.what());
        return false;
    }
}

std::string HSM::maskPan(const std::string& pan) {
    return Tokenizer::getInstance().maskPan(pan);
}

std::string HSM::tokenize(const std::string& pan) {
    return Tokenizer::getInstance().tokenize(pan);
}

std::string HSM::encryptData(const std::string& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    return Crypto::encrypt(data, masterKey_);
}

std::string HSM::decryptData(const std::string& encryptedHex) {
    std::lock_guard<std::mutex> lock(mtx_);
    return Crypto::decrypt(encryptedHex, masterKey_);
}
