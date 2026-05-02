#pragma once
#include <string>
#include "../iso8583/iso_message.h"

// O IssuerService não precisa mais manter estado em memória (como o std::map<std::string, Account>)
// pois agora ele delega a consulta de saldo, limites e status para o DatabaseManager.
class IssuerService {
public:
    static IssuerService& get_instance();

    std::string process_transaction(const IsoMessage& req);
    std::string process_reversal(const IsoMessage& req);

private:
    IssuerService() = default; // Construtor vazio, não inicializa mais mock accounts
};
