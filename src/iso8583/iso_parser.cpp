#include "iso_parser.h"
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <vector>

// Dicionário de campos estático
static map<int, FieldDef> field_defs;

// Funções auxiliares de conversão BCD
static string bcd_to_string_local(const vector<uint8_t>& bcd_data) {
    stringstream ss;
    for (uint8_t byte : bcd_data) {
        ss << hex << (byte >> 4) << (byte & 0x0F);
    }
    return ss.str();
}

static vector<uint8_t> string_to_bcd_local(const string& str) {
    vector<uint8_t> bcd;
    string s = str;
    if (s.length() % 2 != 0) {
        s = "0" + s;
    }
    for (size_t i = 0; i < s.length(); i += 2) {
        uint8_t byte = ((s[i] - '0') << 4) | (s[i+1] - '0');
        bcd.push_back(byte);
    }
    return bcd;
}


static void initialize_field_defs() {
    if (!field_defs.empty()) return;
    field_defs[1] = {1, 16, FieldFormat::FIXED, FieldEncoding::BINARY};
    field_defs[2] = {2, 19, FieldFormat::LLVAR, FieldEncoding::BCD};
    field_defs[3] = {3, 6, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[4] = {4, 12, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[7] = {7, 10, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[11] = {11, 6, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[12] = {12, 6, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[13] = {13, 4, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[14] = {14, 4, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[22] = {22, 3, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[24] = {24, 3, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[25] = {25, 2, FieldFormat::FIXED, FieldEncoding::BCD};
    field_defs[32] = {32, 11, FieldFormat::LLVAR, FieldEncoding::BCD};
    field_defs[35] = {35, 37, FieldFormat::LLVAR, FieldEncoding::BCD};
    field_defs[37] = {37, 12, FieldFormat::FIXED, FieldEncoding::ASCII};
    field_defs[38] = {38, 6, FieldFormat::FIXED, FieldEncoding::ASCII};
    field_defs[39] = {39, 2, FieldFormat::FIXED, FieldEncoding::ASCII};
    field_defs[41] = {41, 8, FieldFormat::FIXED, FieldEncoding::ASCII};
    field_defs[42] = {42, 15, FieldFormat::FIXED, FieldEncoding::ASCII};
    field_defs[49] = {49, 3, FieldFormat::FIXED, FieldEncoding::ASCII};
    field_defs[52] = {52, 8, FieldFormat::FIXED, FieldEncoding::BINARY};
    field_defs[55] = {55, 999, FieldFormat::LLLVAR, FieldEncoding::BINARY};
    field_defs[62] = {62, 999, FieldFormat::LLLVAR, FieldEncoding::ASCII};
    field_defs[63] = {63, 999, FieldFormat::LLLVAR, FieldEncoding::ASCII};
    field_defs[64] = {64, 8, FieldFormat::FIXED, FieldEncoding::BINARY};
    field_defs[70] = {70, 3, FieldFormat::FIXED, FieldEncoding::ASCII};
    field_defs[128] = {128, 8, FieldFormat::FIXED, FieldEncoding::BINARY};
}

IsoMessage IsoParser::parse(const vector<uint8_t>& data) {
    initialize_field_defs();
    IsoMessage msg;
    size_t offset = 0;

    // MTI
    msg.mti = bcd_to_string_local({data[0], data[1]});
    offset += 2;

    // Bitmap
    vector<bool> bitmap(128, false);
    int fields_to_process = 64;
    if ((data[offset] & 0x80) != 0) {
        fields_to_process = 128;
    }

    for (int byte_idx = 0; byte_idx < (fields_to_process / 8); ++byte_idx) {
        for (int bit_idx = 0; bit_idx < 8; ++bit_idx) {
            if ((data[offset + byte_idx] >> (7 - bit_idx)) & 1) {
                bitmap[byte_idx * 8 + bit_idx] = true;
            }
        }
    }
    offset += (fields_to_process / 8);

    // Data Elements
    for (int i = 1; i < fields_to_process; ++i) {
        if (bitmap[i]) {
            int field_id = i + 1;
            if (field_defs.find(field_id) == field_defs.end()) continue;

            FieldDef def = field_defs.at(field_id);
            int len = 0;

            switch (def.format) {
                case FieldFormat::FIXED:
                    len = def.max_length;
                    break;
                case FieldFormat::LLVAR:
                    len = stoi(bcd_to_string_local({data[offset]}));
                    offset += 1;
                    break;
                case FieldFormat::LLLVAR:
                    len = stoi(bcd_to_string_local({data[offset], data[offset+1]}));
                    offset += 2;
                    break;
            }

            int bytes_to_read = (def.encoding == FieldEncoding::BCD) ? (len + 1) / 2 : len;
            if (offset + bytes_to_read > data.size()) {
                throw runtime_error("Buffer overflow while parsing field " + to_string(field_id));
            }

            vector<uint8_t> field_data(data.begin() + offset, data.begin() + offset + bytes_to_read);

            if (def.encoding == FieldEncoding::BCD) {
                msg.set_field(field_id, bcd_to_string_local(field_data).substr(0, len));
            } else { // ASCII or BINARY
                msg.set_field(field_id, string(field_data.begin(), field_data.end()));
            }
            offset += bytes_to_read;
        }
    }
    return msg;
}

vector<uint8_t> IsoParser::build(const IsoMessage& msg) {
    initialize_field_defs();
    vector<uint8_t> data;

    // MTI
    auto mti_bcd = string_to_bcd_local(msg.mti);
    data.insert(data.end(), mti_bcd.begin(), mti_bcd.end());

    // Bitmap
    vector<bool> bitmap(128, false);
    bool has_secondary = false;
    for (const auto& pair : msg.fields) {
        if (pair.first > 1 && pair.first <= 128 && !pair.second.empty()) { // CORREÇÃO APLICADA
            bitmap[pair.first - 1] = true;
            if (pair.first > 64) has_secondary = true;
        }
    }
    if (has_secondary) bitmap[0] = true;

    int bitmap_size = has_secondary ? 16 : 8;
    vector<uint8_t> bitmap_data(bitmap_size, 0);
    for (int i = 0; i < bitmap_size * 8; ++i) {
        if (bitmap[i]) {
            bitmap_data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }
    data.insert(data.end(), bitmap_data.begin(), bitmap_data.end());

    // Data Elements
    for (int i = 1; i < (has_secondary ? 128 : 64); ++i) {
        if (bitmap[i]) {
            int field_id = i + 1;
            if (msg.fields.find(field_id) == msg.fields.end()) continue;

            const auto& val = msg.fields.at(field_id);
            const auto& def = field_defs.at(field_id);

            switch (def.format) {
                case FieldFormat::LLVAR:
                    data.push_back(string_to_bcd_local(to_string(val.length()))[0]);
                    break;
                case FieldFormat::LLLVAR: {
                    auto len_bcd = string_to_bcd_local(to_string(val.length()));
                    data.insert(data.end(), len_bcd.begin(), len_bcd.end());
                    break;
                }
                default: break;
            }

            if (def.encoding == FieldEncoding::BCD) {
                auto field_data = string_to_bcd_local(val);
                data.insert(data.end(), field_data.begin(), field_data.end());
            } else { // ASCII or BINARY
                data.insert(data.end(), val.begin(), val.end());
            }
        }
    }
    return data;
}

// Implementação das funções públicas para manter a compatibilidade do header
string IsoParser::bcd_to_string(const vector<uint8_t>& data, size_t len, bool pad_left) {
    string s = bcd_to_string_local(data);
    return s.substr(0, len);
}

vector<uint8_t> IsoParser::string_to_bcd(const string& str, size_t len, bool pad_left) {
    return string_to_bcd_local(str);
}
