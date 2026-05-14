#pragma once
#include "iso_message.h"
#include <vector>
#include <cstdint>
#include <map>

using namespace std;

class IsoParser {
public:
    /**
     * @brief Realiza o parsing de um vetor de bytes ISO 8583 completo
     *
     * Extrai MTI, resolve Bitmap primário (e secundário se presente),
     * extrai e decodifica cada um dos campos de acordo com a sua def.
     * Lança runtime_error em caso de truncamento ou malformação.
     *
     * @param data Payload binário bruto recebido via TCP
     * @return IsoMessage Mensagem parseada em memória
     */
    static IsoMessage parse(const vector<uint8_t>& data);

    /**
     * @brief Constrói a mensagem ISO 8583 em bytes (pronto para envio TCP)
     *
     * Monta o MTI (BCD ou ASCII, depende do dialeto - aqui BCD), ajusta o
     * Bitmap com base nos campos presentes no IsoMessage, e os serializa
     * aplicando formatação (LLVAR, LLLVAR) e Encoding (BCD/ASCII).
     *
     * @param msg Objeto de mensagem contendo os campos definidos
     * @return vector<uint8_t> Array de bytes final para rede
     */
    static vector<uint8_t> build(const IsoMessage& msg);

private:
    static string bcd_to_string(const vector<uint8_t>& data, size_t len, bool pad_left = false);
    static vector<uint8_t> string_to_bcd(const string& str, size_t len, bool pad_left = false);
};
