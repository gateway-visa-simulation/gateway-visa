#pragma once
#include <string>
#include <vector>
#include <memory> // Para std::unique_ptr
#include <asio.hpp> // Para asio::ip::tcp::socket

using std::string;
using std::vector;
using asio::ip::tcp;

class HSM {
public:
    static HSM& getInstance();

    void init(); // Agora apenas testa a conexão com o SoftHSM externo

    // Valida o PIN Block usando o serviço SoftHSM externo
    bool validatePin(const string& pinBlockHex, const string& pan);

    // Métodos de criptografia/decriptografia (delegam para o SoftHSM externo)
    string encryptData(const string& data);
    string decryptData(const string& encryptedData);

    // Métodos de tokenização (podem delegar para um Tokenizer local ou remoto)
    string maskPan(const string& pan);

private:
    HSM();
    ~HSM();
    HSM(const HSM&) = delete;
    HSM& operator=(const HSM&) = delete;

    // Comunicação com o serviço SoftHSM externo
    string sendToSoftHSM(const string& message);

    // Configurações de conexão
    string softHsmIp_ = "127.0.0.1";
    unsigned short softHsmPort_ = 8000;
};
