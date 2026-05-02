#include "pin_block.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::vector<uint8_t> PinBlock::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::string PinBlock::bytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (uint8_t byte : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::string result = ss.str();
    for(auto &c : result) c = toupper(c);
    return result;
}

std::string PinBlock::generateIso0(const std::string& pin, const std::string& pan) {
    if (pin.length() < 4 || pin.length() > 12) {
        throw std::invalid_argument("PIN length must be between 4 and 12");
    }
    if (pan.length() < 12) {
        throw std::invalid_argument("PAN length must be at least 12");
    }

    // Bloco 1: 0 + tamanho do PIN + PIN + Filler (F)
    std::string block1 = "0" + std::to_string(pin.length()) + pin;
    while (block1.length() < 16) block1 += "F";

    // Bloco 2: 0000 + ultimos 12 digitos do PAN (excluindo check digit)
    // Para simplificar a simulação ISO-0, vamos pegar PAN da direita (12 chars s/ultimo)
    std::string pan12 = pan.substr(pan.length() - 13, 12);
    std::string block2 = "0000" + pan12;

    std::vector<uint8_t> b1 = hexToBytes(block1);
    std::vector<uint8_t> b2 = hexToBytes(block2);
    std::vector<uint8_t> result(8);

    // XOR dos dois blocos
    for (int i = 0; i < 8; ++i) {
        result[i] = b1[i] ^ b2[i];
    }

    return bytesToHex(result);
}

std::string PinBlock::extractIso0(const std::string& pinBlockHex, const std::string& pan) {
    if (pinBlockHex.length() != 16) {
        throw std::invalid_argument("Invalid PIN Block length");
    }
    if (pan.length() < 12) {
        throw std::invalid_argument("PAN length must be at least 12");
    }

    // Recriar Bloco 2 (PAN)
    std::string pan12 = pan.substr(pan.length() - 13, 12);
    std::string block2 = "0000" + pan12;

    std::vector<uint8_t> pb = hexToBytes(pinBlockHex);
    std::vector<uint8_t> b2 = hexToBytes(block2);
    std::vector<uint8_t> b1(8);

    // Reverter XOR para obter Bloco 1
    for (int i = 0; i < 8; ++i) {
        b1[i] = pb[i] ^ b2[i];
    }

    std::string block1 = bytesToHex(b1);

    // Checar formato ISO-0 ('0' + tamanho)
    if (block1[0] != '0') {
        throw std::runtime_error("Invalid ISO-0 PIN Block format");
    }

    int pinLen = block1[1] - '0';
    if (pinLen == 1) { // Lidar com hex A-C caso pin > 9 (A=10, B=11, C=12)
       if(block1[1] == 'A') pinLen = 10;
       else if(block1[1] == 'B') pinLen = 11;
       else if(block1[1] == 'C') pinLen = 12;
    }

    if (pinLen < 4 || pinLen > 12) {
        throw std::runtime_error("Corrupted PIN Block (invalid length)");
    }

    return block1.substr(2, pinLen);
}
