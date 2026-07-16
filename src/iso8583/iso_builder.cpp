#include "iso_builder.h"

IsoMessage IsoBuilder::build_response(const IsoMessage& request, const string& response_code) {
    IsoMessage response;

    // MTI
    if (request.mti == "0100") {
        response.mti = "0110";
    } else if (request.mti == "0200") {
        response.mti = "0210";
    } else if (request.mti == "0400") {
        response.mti = "0410";
    } else {
        // Fallback genérico para outros tipos de mensagem
        response.mti = request.mti; // Mantém o MTI para mensagens não esperadas
    }

    // Copia todos os campos da requisição para a resposta, exceto o MTI e o MAC
    for (const auto& pair : request.fields) {
        if (pair.first != 64 && pair.first != 128) {
            response.set_field(pair.first, pair.second);
        }
    }

    // Define ou sobrescreve o código de resposta
    response.set_field(39, response_code);

    // Garante que campos essenciais para a correlação estejam presentes
    if (request.has_field(11)) {
        response.set_field(11, request.get_field(11));
    }
    if (request.has_field(41)) {
        response.set_field(41, request.get_field(41));
    }

    return response;
}