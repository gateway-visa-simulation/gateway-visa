#include "json_to_iso.h"
#include "../common/logger.h"
#include <iomanip>
#include <sstream>


string JsonToIsoMapper::formatAmount(int amount) {
    ostringstream ss;
    ss << setw(12) << setfill('0') << amount;
    return ss.str();
}

IsoMessage JsonToIsoMapper::mapJsonToIso(const json& response, const IsoMessage& originalRequest) {
    IsoMessage res;

    // MTI Configuração
    // Em um cenario real, o MTI da resposta vem do JSON (ou e inferido)
    if (response.contains("mti")) {
        res.mti = response["mti"].get<string>();
    } else {
        // Padrao fallback (0110 auth response) se nao fornecido
        res.mti = "0110";
    }

    // DE11: STAN - Correlacao OBRIGATORIA
    if (response.contains("stan")) {
        res.set_field(11, response["stan"].get<string>());
    } else {
        LOG_ERROR("JSON de resposta nao contem STAN para correlacao!");
        throw runtime_error("STAN missing in JSON response");
    }

    // DE39: Response Code
    if (response.contains("responseCode")) {
        res.set_field(39, response["responseCode"].get<string>());
    } else {
        LOG_WARN("Response code faltando no JSON, assumindo falha (96). STAN: {}", response["stan"].get<string>());
        res.set_field(39, "96");
    }

    // Opcional: DE4 Amount se for retornado do backend
    if (response.contains("amount")) {
        res.set_field(4, formatAmount(response["amount"].get<int>()));
    }

    // Se a request original foi passada, copia campos criticos que devem retornar ao terminal
    // (A documentação ISO exige ecoar alguns dados do request na response)
    if (!originalRequest.mti.empty()) {
        int copy_fields[] = {2, 3, 4, 7, 12, 13, 22, 41, 42, 49};
        for (int field : copy_fields) {
            // Nao sobrescreve campos ja setados pelo JSON (ex: amount atualizado)
            if (originalRequest.has_field(field) && !res.has_field(field)) {
                res.set_field(field, originalRequest.get_field(field));
            }
        }
    }

    LOG_DEBUG("JSON to ISO map completado para STAN: {} | Code: {}", res.get_field(11), res.get_field(39));

    return res;
}
