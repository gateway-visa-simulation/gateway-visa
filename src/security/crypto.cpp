#include "crypto.h"
#include <iomanip>
#include <sstream>

std::vector<uint8_t> Crypto::hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::string Crypto::bytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (uint8_t byte : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::string result = ss.str();
    for(auto &c : result) c = toupper(c);
    return result;
}

std::vector<uint8_t> Crypto::xorCrypt(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) {
    std::vector<uint8_t> result(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}

std::string Crypto::encrypt(const std::string& data, const std::string& key) {
    std::vector<uint8_t> dataBytes(data.begin(), data.end());
    std::vector<uint8_t> keyBytes(key.begin(), key.end());

    std::vector<uint8_t> encryptedBytes = xorCrypt(dataBytes, keyBytes);
    return bytesToHex(encryptedBytes);
}

std::string Crypto::decrypt(const std::string& encryptedHex, const std::string& key) {
    std::vector<uint8_t> encryptedBytes = hexToBytes(encryptedHex);
    std::vector<uint8_t> keyBytes(key.begin(), key.end());

    std::vector<uint8_t> decryptedBytes = xorCrypt(encryptedBytes, keyBytes);
    return std::string(decryptedBytes.begin(), decryptedBytes.end());
}
