#include "tokenization.h"
#include <sstream>

Tokenizer& Tokenizer::getInstance() {
    static Tokenizer instance;
    return instance;
}

string Tokenizer::maskPan(const string& pan) {
    if (pan.length() < 10) return pan;
    string masked = pan;
    for (size_t i = 6; i < masked.length() - 4; ++i) {
        masked[i] = '*';
    }
    return masked;
}

string Tokenizer::tokenize(const string& pan) {
    lock_guard<mutex> lock(mtx_);

    // Retornar o mesmo token se já existir para este PAN
    auto it = reverse_.find(pan);
    if (it != reverse_.end()) {
        return it->second;
    }

    // Gerar um token fake: TKN + PAN mascarado (na real seria AES cifrado ou UUID randomico)
    // Para simplificar a simulação visual sem expor o PAN real
    stringstream ss;
    ss << "TKN" << ++counter_ << pan.substr(pan.length() - 4, 4);
    string token = ss.str();

    vault_[token] = pan;
    reverse_[pan] = token;

    return token;
}

string Tokenizer::detokenize(const string& token) {
    lock_guard<mutex> lock(mtx_);
    auto it = vault_.find(token);
    if (it != vault_.end()) {
        return it->second;
    }
    return "";
}
