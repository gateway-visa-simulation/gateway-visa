#pragma once
#include <string>
#include <vector>
#include <cstdint>

using namespace std;

class Crypto {
public:
    /**
     * @brief Simula encriptação de dados usando XOR (Cifragem simetrica basica mockada)
     *
     * @param data O texto plano
     * @param key A chave de encriptacao (senha mestra)
     * @return string Base64 ou Hex do array encriptado
     */
    static string encrypt(const string& data, const string& key);

    /**
     * @brief Simula decriptação de dados usando XOR
     *
     * @param encryptedData Hex string gerada por encrypt()
     * @param key A mesma chave
     * @return string O texto plano decifrado
     */
    static string decrypt(const string& encryptedHex, const string& key);

private:
    static vector<uint8_t> xorCrypt(const vector<uint8_t>& data, const vector<uint8_t>& key);
    static vector<uint8_t> hexToBytes(const string& hex);
    static string bytesToHex(const vector<uint8_t>& bytes);
};
