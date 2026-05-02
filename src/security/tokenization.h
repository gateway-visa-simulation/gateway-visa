#pragma once
#include <string>
#include <map>
#include <mutex>

class Tokenizer {
public:
    static Tokenizer& getInstance();

    /**
     * @brief Mascara um PAN para logs e exibicao
     *
     * @param pan "1234567890123456"
     * @return std::string "123456******3456"
     */
    std::string maskPan(const std::string& pan);

    /**
     * @brief Gera um token (vault) e guarda o mapping para o PAN real.
     *
     * @param pan PAN real
     * @return std::string Token gerado em formato seguro
     */
    std::string tokenize(const std::string& pan);

    /**
     * @brief Devolve o PAN original de um token em memoria.
     *
     * @param token
     * @return std::string O PAN se encontrado, string vazia se invalido.
     */
    std::string detokenize(const std::string& token);

private:
    Tokenizer() = default;

    std::map<std::string, std::string> vault_;   // token -> pan
    std::map<std::string, std::string> reverse_; // pan -> token
    std::mutex mtx_;
    uint64_t counter_ = 1000000;
};
