#include "crypto.h"
#include <iomanip>
#include <sstream>

vector<uint8_t> Crypto::hexToBytes(const string& hex) {
    vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        string byteString = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

string Crypto::bytesToHex(const vector<uint8_t>& bytes) {
    stringstream ss;
    for (uint8_t byte : bytes) {
        ss << hex << setw(2) << setfill('0') << static_cast<int>(byte);
    }
    string result = ss.str();
    for(auto &c : result) c = toupper(c);
    return result;
}

vector<uint8_t> Crypto::xorCrypt(const vector<uint8_t>& data, const vector<uint8_t>& key) {
    vector<uint8_t> result(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}

string Crypto::encrypt(const string& data, const string& key) {
    vector<uint8_t> dataBytes(data.begin(), data.end());
    vector<uint8_t> keyBytes(key.begin(), key.end());

    vector<uint8_t> encryptedBytes = xorCrypt(dataBytes, keyBytes);
    return bytesToHex(encryptedBytes);
}

string Crypto::decrypt(const string& encryptedHex, const string& key) {
    vector<uint8_t> encryptedBytes = hexToBytes(encryptedHex);
    vector<uint8_t> keyBytes(key.begin(), key.end());

    vector<uint8_t> decryptedBytes = xorCrypt(encryptedBytes, keyBytes);
    return string(decryptedBytes.begin(), decryptedBytes.end());
}
