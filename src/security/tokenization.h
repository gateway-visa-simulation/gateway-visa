#pragma once
#include <string>
#include <map>
#include <mutex>

using namespace std;

class Tokenizer {
public:
    static Tokenizer& getInstance();

    /**
     * @brief Mascara um PAN para logs e exibicao
     *
     * @param pan "1234567890123456"
     * @return string "123456******3456"
     */
    string maskPan(const string& pan);

    /**
     * @brief Gera um token (vault) e guarda o mapping para o PAN real.
     *
     * @param pan PAN real
     * @return string Token gerado em formato seguro
     */
    string tokenize(const string& pan);

    /**
     * @brief Devolve o PAN original de um token em memoria.
     *
     * @param token
     * @return string O PAN se encontrado, string vazia se invalido.
     */
    string detokenize(const string& token);

private:
    Tokenizer() = default;

    map<string, string> vault_;   // token -> pan
    map<string, string> reverse_; // pan -> token
    mutex mtx_;
    uint64_t counter_ = 1000000;
};
