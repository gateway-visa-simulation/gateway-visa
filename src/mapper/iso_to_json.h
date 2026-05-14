#pragma once
#include <nlohmann/json.hpp>
#include "../iso8583/iso_message.h"
#include <string>

using namespace std;

using json = nlohmann::json;

class IsoToJsonMapper {
public:
    /**
     * @brief Converte uma mensagem ISO 8583 em um objeto JSON
     *
     * Mapeia os campos críticos (DE2, DE3, DE4, DE7, DE11, DE41, DE42, DE49)
     * e garante o mascaramento do PAN. Lança exceção se faltarem campos obrigatórios.
     *
     * @param iso A mensagem ISO 8583
     * @return json Objeto JSON mapeado
     */
    static json mapIsoToJson(const IsoMessage& iso);

private:
    static string maskPan(const string& pan);
    static int parseAmount(const string& amountStr);
};
