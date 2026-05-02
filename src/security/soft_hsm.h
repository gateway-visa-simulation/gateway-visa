#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../iso8583/iso_message.h"

class SoftHsm {
public:
    static SoftHsm& get_instance();

    // Valida o MAC de uma mensagem ISO 8583 usando o algoritmo Retail MAC.
    bool validate_retail_mac(const IsoMessage& msg, const std::string& mak_hex);

    // Decifra um PIN Block (formato ISO-0) e retorna o PIN em texto plano.
    std::optional<std::string> decrypt_pin_block_iso0(const std::string& pin_block_hex, const std::string& pan, const std::string& zpk_hex);

private:
    SoftHsm() = default;
    ~SoftHsm() = default;
    SoftHsm(const SoftHsm&) = delete;
    SoftHsm& operator=(const SoftHsm&) = delete;

    // Helpers
    std::vector<uint8_t> hex_to_bytes(const std::string& hex);
    std::string bytes_to_hex(const std::vector<uint8_t>& bytes);
    std::vector<uint8_t> build_mac_data_block(const IsoMessage& msg);
    std::vector<uint8_t> des_encrypt_cbc(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);
    std::vector<uint8_t> tdes_decrypt_ecb(const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);
};
