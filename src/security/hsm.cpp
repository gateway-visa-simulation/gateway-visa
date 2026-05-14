#include "hsm.h"
#include "../common/logger.h"
#include <asio.hpp> // Para asio::ip::tcp::socket, asio::buffer, asio::connect
#include <array>    // Para std::array
#include <iostream> // Para std::cout, std::cerr

// O HSM agora funciona como um Proxy/Cliente para o serviço SoftHSM
HSM& HSM::getInstance() {
    static HSM instance;
    return instance;
}

HSM::HSM() {
    // Construtor, pode inicializar membros se necessário
}

HSM::~HSM() {
    // Destrutor
}

void HSM::init() {
    // Agora o init apenas testa a conexão com o serviço SoftHSM
    LOG_INFO("HSM: Gateway configurado para conectar ao SoftHSM em {}:{}", softHsmIp_, softHsmPort_);

    // Tenta uma conexão para verificar se o SoftHSM está ativo
    try {
        asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        asio::connect(socket, resolver.resolve(softHsmIp_, std::to_string(softHsmPort_)));
        LOG_INFO("HSM: Conexão de teste com SoftHSM em {}:{} bem-sucedida.", softHsmIp_, softHsmPort_);
    } catch (const std::exception& e) {
        LOG_ERROR("HSM: Falha ao conectar ao SoftHSM em {}:{}. Erro: {}", softHsmIp_, softHsmPort_, e.what());
        // Em um ambiente de produção, você pode querer lançar uma exceção ou ter um mecanismo de retry.
    }
}

bool HSM::validatePin(const string& pinBlockHex, const string& pan) {
    // 1. Monta o payload para o SoftHSM (Pode ser um JSON ou Struct)
    // Exemplo simplificado de comando: "VALIDATE_PIN|pinBlock|pan|keyName"
    string request = "VALIDATE_PIN|" + pinBlockHex + "|" + pan + "|ZPK_DEFAULT"; // Usando uma chave default

    string response = sendToSoftHSM(request);

    // 2. O SoftHSM processa e retorna o resultado
    if (response == "VALID") {
        LOG_INFO("HSM Remoto: PIN Validado com Sucesso.");
        return true;
    }

    LOG_WARN("HSM Remoto: Falha na validação do PIN. Resposta: {}", response);
    return false;
}

string HSM::encryptData(const string& data) {
    string request = "ENCRYPT_DATA|" + data + "|DEK_DEFAULT"; // Usando uma chave default
    string response = sendToSoftHSM(request);
    if (response.rfind("ERROR", 0) == 0) { // Verifica se a resposta começa com "ERROR"
        LOG_ERROR("HSM Remoto: Falha na criptografia. Resposta: {}", response);
        return ""; // Retorna string vazia em caso de erro
    }
    return response;
}

string HSM::decryptData(const string& encryptedData) {
    string request = "DECRYPT_DATA|" + encryptedData + "|DEK_DEFAULT"; // Usando uma chave default
    string response = sendToSoftHSM(request);
    if (response.rfind("ERROR", 0) == 0) { // Verifica se a resposta começa com "ERROR"
        LOG_ERROR("HSM Remoto: Falha na decriptografia. Resposta: {}", response);
        return ""; // Retorna string vazia em caso de erro
    }
    return response;
}

// Função auxiliar para comunicação via Socket
string HSM::sendToSoftHSM(const string& message) {
    asio::io_context io_context;
    tcp::socket socket(io_context);

    try {
        tcp::resolver resolver(io_context);
        asio::connect(socket, resolver.resolve(softHsmIp_, std::to_string(softHsmPort_)));

        // Envia o tamanho da mensagem (4 bytes, network byte order)
        uint32_t message_length = message.length();
        message_length = htonl(message_length); // Converte para network byte order
        asio::write(socket, asio::buffer(&message_length, sizeof(message_length)));

        // Envia a mensagem
        asio::write(socket, asio::buffer(message));

        // Recebe o tamanho da resposta (4 bytes)
        uint32_t reply_length;
        asio::read(socket, asio::buffer(&reply_length, sizeof(reply_length)));
        reply_length = ntohl(reply_length); // Converte de network byte order

        // Recebe a resposta
        std::vector<char> reply_buffer(reply_length);
        asio::read(socket, asio::buffer(reply_buffer));

        return string(reply_buffer.begin(), reply_buffer.end());

    } catch (const std::exception& e) {
        LOG_ERROR("HSM: Erro de comunicação com SoftHSM em {}:{}. Erro: {}", softHsmIp_, softHsmPort_, e.what());
        return "ERROR: " + string(e.what());
    }
}

// Métodos de Tokenização continuam delegando para o Tokenizer local ou remoto
string HSM::maskPan(const string& pan) {
    // Se você tiver um serviço de tokenização remoto, ele seria chamado aqui.
    // Por enquanto, mantém a máscara local.
    if (pan.length() < 10) return pan;
    string masked = pan;
    for (size_t i = 6; i < masked.length() - 4; ++i) {
        masked[i] = '*';
    }
    return masked;
}
