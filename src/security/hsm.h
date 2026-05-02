#pragma once
#include <string>
#include <map>
#include <mutex>

class HSM {
public:
    static HSM& getInstance();

    /**
     * @brief Inicializa as chaves mestras e PINs do mock database
     */
    void init();

    /**
     * @brief Extrai e valida um PIN (em ISO-0) contra o valor no banco
     *
     * @param pinBlockHex O bloco recebido no DE52
     * @param pan O PAN do usuario (DE2)
     * @return bool verdadeiro se o PIN confere, falso caso erro ou falha
     */
    bool validatePin(const std::string& pinBlockHex, const std::string& pan);

    /**
     * @brief Mascara logs
     */
    std::string maskPan(const std::string& pan);

    /**
     * @brief Gera o Token do PAN
     */
    std::string tokenize(const std::string& pan);

    /**
     * @brief Encripta o PAN com uma chave mestra HSM
     */
    std::string encryptData(const std::string& data);
    std::string decryptData(const std::string& encryptedHex);

private:
    HSM() = default;

    std::string masterKey_;
    std::map<std::string, std::string> mockPinDatabase_; // PAN -> PIN plaintext para testes
    std::mutex mtx_;
};
