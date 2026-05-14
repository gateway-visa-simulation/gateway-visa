#include "iso_to_json.h"
#include "../common/logger.h"
#include <stdexcept>
#include <sstream>


string IsoToJsonMapper::maskPan(const string& pan) {
    if (pan.length() < 10) return pan;
    string masked = pan;
    for (size_t i = 6; i < masked.length() - 4; ++i) {
        masked[i] = '*';
    }
    return masked;
}

int IsoToJsonMapper::parseAmount(const string& amountStr) {
    if (amountStr.empty()) return 0;
    try {
        return stoi(amountStr);
    } catch (...) {
        LOG_WARN("Falha ao fazer parse do Amount: {}", amountStr);
        return 0;
    }
}

json IsoToJsonMapper::mapIsoToJson(const IsoMessage& iso) {
    json j;

    // Validação de campos obrigatórios
    if (!iso.has_field(2) || !iso.has_field(4) || !iso.has_field(11)) {
        LOG_ERROR("Mensagem ISO sem campos obrigatorios (2, 4 ou 11)");
        throw runtime_error("Missing mandatory ISO fields for JSON mapping");
    }

    j["mti"] = iso.mti;

    // DE11: STAN (Correlation Key)
    j["stan"] = iso.get_field(11);

    // DE2: PAN (Sempre mascarado!)
    string raw_pan = iso.get_field(2);
    j["pan"] = maskPan(raw_pan);

    // DE4: Amount (Conversão de BCD/String '000000001000' para int '1000')
    j["amount"] = parseAmount(iso.get_field(4));

    // DE3: Processing Code
    if (iso.has_field(3)) j["processingCode"] = iso.get_field(3);

    // DE7: Transmission Date Time
    if (iso.has_field(7)) j["timestamp"] = iso.get_field(7);

    // DE41: Terminal ID
    if (iso.has_field(41)) j["terminalId"] = iso.get_field(41);

    // DE42: Merchant ID
    if (iso.has_field(42)) j["merchantId"] = iso.get_field(42);

    // DE49: Currency
    if (iso.has_field(49)) j["currency"] = iso.get_field(49);

    LOG_DEBUG("ISO to JSON map completado para STAN: {}", j["stan"].get<string>());

    return j;
}
