#include "pin_block.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>


vector<uint8_t> PinBlock::hexToBytes(const string& hex) {
    vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

string PinBlock::bytesToHex(const vector<uint8_t>& bytes) {
    stringstream ss;
    for (uint8_t byte : bytes) {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(byte);
    }
    string result = ss.str();
    for(auto &c : result) c = toupper(c);
    return result;
}

string PinBlock::generateIso0(const string& pin, const string& pan) {
    if (pin.length() < 4 || pin.length() > 12) {
        throw invalid_argument("PIN length must be between 4 and 12");
    }
    if (pan.length() < 12) {
        throw invalid_argument("PAN length must be at least 12");
    }

    // Bloco 1: 0 + tamanho do PIN + PIN + Filler (F)
    string block1 = "0" + to_string(pin.length()) + pin;
    while (block1.length() < 16) block1 += "F";

    // Bloco 2: 0000 + ultimos 12 digitos do PAN (excluindo check digit)
    // Para simplificar a simulação ISO-0, vamos pegar PAN da direita (12 chars s/ultimo)
    string pan12 = pan.substr(pan.length() - 13, 12);
    string block2 = "0000" + pan12;

    vector<uint8_t> b1 = hexToBytes(block1);
    vector<uint8_t> b2 = hexToBytes(block2);
    vector<uint8_t> result(8);

    // XOR dos dois blocos
    for (int i = 0; i < 8; ++i) {
        result[i] = b1[i] ^ b2[i];
    }

    return bytesToHex(result);
}

string PinBlock::extractIso0(const string& pinBlockHex, const string& pan) {
    if (pinBlockHex.length() != 16) {
        throw invalid_argument("Invalid PIN Block length");
    }
    if (pan.length() < 12) {
        throw invalid_argument("PAN length must be at least 12");
    }

    // Recriar Bloco 2 (PAN)
    string pan12 = pan.substr(pan.length() - 13, 12);
    string block2 = "0000" + pan12;

    vector<uint8_t> pb = hexToBytes(pinBlockHex);
    vector<uint8_t> b2 = hexToBytes(block2);
    vector<uint8_t> b1(8);

    // Reverter XOR para obter Bloco 1
    for (int i = 0; i < 8; ++i) {
        b1[i] = pb[i] ^ b2[i];
    }

    string block1 = bytesToHex(b1);

    // Checar formato ISO-0 ('0' + tamanho)
    if (block1[0] != '0') {
        throw runtime_error("Invalid ISO-0 PIN Block format");
    }

    int pinLen = block1[1] - '0';
    if (pinLen == 1) { // Lidar com hex A-C caso pin > 9 (A=10, B=11, C=12)
       if(block1[1] == 'A') pinLen = 10;
       else if(block1[1] == 'B') pinLen = 11;
       else if(block1[1] == 'C') pinLen = 12;
    }

    if (pinLen < 4 || pinLen > 12) {
        throw runtime_error("Corrupted PIN Block (invalid length)");
    }

    return block1.substr(2, pinLen);
}
