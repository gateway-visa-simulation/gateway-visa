#pragma once
#include <string>
#include <vector>
#include <cstdint>

using namespace std;

class PinBlock {
public:
    /**
     * @brief Gera um PIN Block ISO-0 (Format 0)
     *
     * @param pin O PIN em texto plano (ex: "1234")
     * @param pan O PAN completo (ex: "1234567890123456")
     * @return string PIN Block em formato hexadecimal (16 caracteres = 8 bytes)
     */
    static string generateIso0(const string& pin, const string& pan);

    /**
     * @brief Decodifica e extrai o PIN de um PIN Block ISO-0
     *
     * @param pinBlockHex O PIN Block em hexadecimal (16 caracteres)
     * @param pan O PAN completo
     * @return string O PIN decodificado
     */
    static string extractIso0(const string& pinBlockHex, const string& pan);

private:
    static vector<uint8_t> hexToBytes(const string& hex);
    static string bytesToHex(const vector<uint8_t>& bytes);
};
