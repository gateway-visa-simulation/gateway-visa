#pragma once
#include <string>
#include <vector>
#include <cstdint>

class PinBlock {
public:
    /**
     * @brief Gera um PIN Block ISO-0 (Format 0)
     *
     * @param pin O PIN em texto plano (ex: "1234")
     * @param pan O PAN completo (ex: "1234567890123456")
     * @return std::string PIN Block em formato hexadecimal (16 caracteres = 8 bytes)
     */
    static std::string generateIso0(const std::string& pin, const std::string& pan);

    /**
     * @brief Decodifica e extrai o PIN de um PIN Block ISO-0
     *
     * @param pinBlockHex O PIN Block em hexadecimal (16 caracteres)
     * @param pan O PAN completo
     * @return std::string O PIN decodificado
     */
    static std::string extractIso0(const std::string& pinBlockHex, const std::string& pan);

private:
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
    static std::string bytesToHex(const std::vector<uint8_t>& bytes);
};
