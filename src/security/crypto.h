#pragma once
#include <string>
#include <vector>
#include <cstdint>

class Crypto {
public:
    /**
     * @brief Simula encriptação de dados usando XOR (Cifragem simetrica basica mockada)
     *
     * @param data O texto plano
     * @param key A chave de encriptacao (senha mestra)
     * @return std::string Base64 ou Hex do array encriptado
     */
    static std::string encrypt(const std::string& data, const std::string& key);

    /**
     * @brief Simula decriptação de dados usando XOR
     *
     * @param encryptedData Hex string gerada por encrypt()
     * @param key A mesma chave
     * @return std::string O texto plano decifrado
     */
    static std::string decrypt(const std::string& encryptedHex, const std::string& key);

private:
    static std::vector<uint8_t> xorCrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key);
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
    static std::string bytesToHex(const std::vector<uint8_t>& bytes);
};
