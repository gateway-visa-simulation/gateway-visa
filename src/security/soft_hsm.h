#pragma once
#include <string>
#include <vector>
#include <optional>
#include "../iso8583/iso_message.h"


using namespace std;

class SoftHsm {
public:
    static SoftHsm& get_instance();

    // Valida o MAC de uma mensagem ISO 8583 usando o algoritmo Retail MAC.
    bool validate_retail_mac(const IsoMessage& msg, const string& mak_hex);

    // Decifra um PIN Block (formato ISO-0) e retorna o PIN em texto plano.
    optional<string> decrypt_pin_block_iso0(const string& pin_block_hex, const string& pan, const string& zpk_hex);

private:
    SoftHsm() = default;
    ~SoftHsm() = default;
    SoftHsm(const SoftHsm&) = delete;
    SoftHsm& operator=(const SoftHsm&) = delete;

    // Helpers
    vector<uint8_t> hex_to_bytes(const string& hex);
    string bytes_to_hex(const vector<uint8_t>& bytes);
    vector<uint8_t> build_mac_data_block(const IsoMessage& msg);
    vector<uint8_t> des_encrypt_cbc(const vector<uint8_t>& key, const vector<uint8_t>& data);
    vector<uint8_t> tdes_decrypt_ecb(const vector<uint8_t>& key, const vector<uint8_t>& data);
};
