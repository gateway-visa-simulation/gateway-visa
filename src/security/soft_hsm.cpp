#include "soft_hsm.h"
#include "../common/logger.h"
#include <openssl/evp.h>
#include <openssl/des.h>
#include <iomanip>
#include <sstream>
#include <algorithm>

SoftHsm& SoftHsm::get_instance() {
    static SoftHsm instance;
    return instance;
}

bool SoftHsm::validate_retail_mac(const IsoMessage& msg, const std::string& mak_hex) {
    // O MAC pode estar no DE 64 (primário) ou DE 128 (secundário)
    std::string received_mac_hex;
    if (msg.has_field(128)) {
        received_mac_hex = msg.get_field(128);
    } else if (msg.has_field(64)) {
        received_mac_hex = msg.get_field(64);
    } else {
        LOG_WARN("MAC validation failed: Message has no MAC field (64 or 128).");
        return false;
    }

    // 1. Preparar dados
    auto mak_bytes = hex_to_bytes(mak_hex);
    auto received_mac_bytes = hex_to_bytes(received_mac_hex);
    auto data_block = build_mac_data_block(msg);

    // O Retail MAC (ISO 9797-1 Alg 1) usa DES-CBC sobre os dados e o último bloco é o MAC.
    // A chave MAK é geralmente dupla (16 bytes). Usamos a primeira metade para a criptografia.
    if (mak_bytes.size() < 16) {
        LOG_ERROR("Invalid MAK size for Retail MAC. Must be 16 bytes (32 hex chars).");
        return false;
    }
    std::vector<uint8_t> key1(mak_bytes.begin(), mak_bytes.begin() + 8);

    // 2. Calcular o MAC
    auto encrypted_block = des_encrypt_cbc(key1, data_block);
    if (encrypted_block.empty()) {
        return false;
    }
    // O MAC é o último bloco de 8 bytes do resultado
    std::vector<uint8_t> calculated_mac(encrypted_block.end() - 8, encrypted_block.end());

    // 3. Comparar
    if (calculated_mac == received_mac_bytes) {
        LOG_INFO("Retail MAC validation successful.");
        return true;
    } else {
        LOG_WARN("Retail MAC validation FAILED. Expected [{}], Received [{}].",
                 bytes_to_hex(calculated_mac), received_mac_hex);
        return false;
    }
}

std::optional<std::string> SoftHsm::decrypt_pin_block_iso0(const std::string& pin_block_hex, const std::string& pan, const std::string& zpk_hex) {
    if (zpk_hex.size() < 32) {
        LOG_ERROR("Invalid ZPK size for PIN block decryption. Must be 16 bytes (32 hex chars).");
        return std::nullopt;
    }

    // 1. Preparar dados
    auto zpk_bytes = hex_to_bytes(zpk_hex);
    auto pin_block_bytes = hex_to_bytes(pin_block_hex);

    // 2. Decifrar o PIN Block com TDES
    auto clear_pin_block = tdes_decrypt_ecb(zpk_bytes, pin_block_bytes);

    // 3. Construir o "PAN block" (formato ISO-0)
    // São os 12 últimos dígitos do PAN, excluindo o dígito verificador.
    if (pan.length() < 13) {
        LOG_ERROR("PAN is too short for ISO-0 PIN block processing.");
        return std::nullopt;
    }
    std::string pan_part = "0000" + pan.substr(pan.length() - 13, 12);
    auto pan_block = hex_to_bytes(pan_part);

    // 4. Fazer o XOR para obter o bloco de PIN puro
    std::vector<uint8_t> pin_data_block(8);
    for (size_t i = 0; i < 8; ++i) {
        pin_data_block[i] = clear_pin_block[i] ^ pan_block[i];
    }

    // 5. Extrair o PIN
    // Formato: 0L P P P P P/F F... (L = tamanho, P = PIN, F = padding)
    int pin_len = pin_data_block[0] & 0x0F;
    if (pin_len < 4 || pin_len > 12) {
        LOG_ERROR("Invalid PIN length decoded from PIN block: {}", pin_len);
        return std::nullopt;
    }

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for(size_t i = 1; i < pin_data_block.size(); ++i) {
        ss << std::setw(2) << (int)pin_data_block[i];
    }

    std::string pin_and_padding = ss.str();
    std::string pin = pin_and_padding.substr(0, pin_len);

    LOG_INFO("PIN block decrypted successfully. PIN length: {}", pin_len);
    return pin;
}


// --- Helpers ---

std::vector<uint8_t> SoftHsm::build_mac_data_block(const IsoMessage& msg) {
    std::vector<uint8_t> block;
    // MTI
    block.insert(block.end(), msg.mti.begin(), msg.mti.end());
    // Data Elements
    for (int i = 1; i <= 128; ++i) {
        if (msg.has_field(i)) {
            const auto& field_data = msg.get_field(i);
            block.insert(block.end(), field_data.begin(), field_data.end());
        }
    }
    return block;
}

std::vector<uint8_t> SoftHsm::des_encrypt_cbc(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    // Padding manual para ser múltiplo de 8
    std::vector<uint8_t> padded_data = data;
    size_t padding_needed = 8 - (data.size() % 8);
    if (padding_needed != 8) {
        padded_data.insert(padded_data.end(), padding_needed, 0x00);
    }

    std::vector<uint8_t> ciphertext(padded_data.size());
    int len = 0;
    unsigned char iv[8] = {0}; // IV de zeros

    EVP_EncryptInit_ex(ctx, EVP_des_cbc(), NULL, key.data(), iv);
    EVP_CIPHER_CTX_set_padding(ctx, 0); // Desabilitamos o padding do OpenSSL
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, padded_data.data(), padded_data.size());

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext;
}

std::vector<uint8_t> SoftHsm::tdes_decrypt_ecb(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    std::vector<uint8_t> plaintext(data.size());
    int len = 0;

    // Para TDES, a chave pode ser de 16 ou 24 bytes. OpenSSL lida com isso.
    const EVP_CIPHER* cipher = (key.size() == 16) ? EVP_des_ede_ecb() : EVP_des_ede3_ecb();

    EVP_DecryptInit_ex(ctx, cipher, NULL, key.data(), NULL);
    EVP_CIPHER_CTX_set_padding(ctx, 0);
    EVP_DecryptUpdate(ctx, plaintext.data(), &len, data.data(), data.size());

    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(len);
    return plaintext;
}

std::vector<uint8_t> SoftHsm::hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        bytes.push_back(static_cast<uint8_t>(std::stoul(byteString, nullptr, 16)));
    }
    return bytes;
}

std::string SoftHsm::bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}
