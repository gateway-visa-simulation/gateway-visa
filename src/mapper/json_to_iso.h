#pragma once
#include <nlohmann/json.hpp>
#include "../iso8583/iso_message.h"


using namespace std;

using json = nlohmann::json;

class JsonToIsoMapper {
public:
    /**
     * @brief Converte uma resposta JSON (Kafka) de volta para uma mensagem ISO 8583
     *
     * Mapeia os campos da resposta (DE39), mantém o STAN para correlação,
     * e configura o MTI (ex: 0110).
     *
     * @param response JSON com a resposta do backend
     * @param originalRequest (Opcional) Mensagem ISO original para copiar campos necessarios
     * @return IsoMessage Mensagem ISO pronta para envio TCP
     */
    static IsoMessage mapJsonToIso(const json& response, const IsoMessage& originalRequest = IsoMessage());

private:
    static string formatAmount(int amount);
};
