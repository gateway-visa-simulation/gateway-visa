#include "iso_parser.h"
#include "../common/logger.h"
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
    ss << hex << setfill('0');
    for (uint8_t byte : bcd_data) {
        ss << setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

static uint8_t hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0; // fallback
}

static vector<uint8_t> string_to_bcd_local(const string& str) {
    vector<uint8_t> bcd;
    string s = str;
    if (s.length() % 2 != 0) {
        s = "0" + s;
    }
    for (size_t i = 0; i < s.length(); i += 2) {
        uint8_t byte = (hex_char_to_val(s[i]) << 4) | hex_char_to_val(s[i+1]);
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

    if (data.size() < 2) throw runtime_error("Message too short for MTI");
    msg.mti = bcd_to_string_local({data[offset], data[offset + 1]});
    offset += 2;

    if (data.size() < offset + 8) throw runtime_error("Buffer too small for primary bitmap");
    vector<bool> bitmap(129, false);
    int fields_to_process = 64;
    if ((data[offset] & 0x80) != 0) {
        fields_to_process = 128;
        if (data.size() < offset + 16) throw runtime_error("Buffer too small for full bitmap");
    }

    for (int byte_idx = 0; byte_idx < (fields_to_process / 8); ++byte_idx) {
        for (int bit_idx = 0; bit_idx < 8; ++bit_idx) {
            if ((data[offset + byte_idx] >> (7 - bit_idx)) & 1) {
                bitmap[byte_idx * 8 + bit_idx + 1] = true;
            }
        }
    }
    offset += (fields_to_process / 8);

    for (int field_id = 2; field_id <= fields_to_process; ++field_id) {
        if (bitmap[field_id]) {
            if (field_defs.find(field_id) == field_defs.end()) {
                throw runtime_error("Definition not found for field " + to_string(field_id));
            }
            const FieldDef& def = field_defs.at(field_id);
            int len = 0;

            switch (def.format) {
                case FieldFormat::FIXED:
                    len = def.max_length;
                    break;
                case FieldFormat::LLVAR: {
                    if (data.size() < offset + 1) throw runtime_error("Buffer overflow reading LLVAR length for field " + to_string(field_id));
                    int d1 = (data[offset] >> 4);
                    int d2 = (data[offset] & 0x0F);
                    if (d1 > 9 || d2 > 9) throw runtime_error("Invalid BCD in LLVAR length for field " + to_string(field_id));
                    len = d1 * 10 + d2;
                    offset += 1;
                    break;
                }
                case FieldFormat::LLLVAR: {
                    if (data.size() < offset + 2) throw runtime_error("Buffer overflow reading LLLVAR length for field " + to_string(field_id));
                    int d1 = (data[offset] >> 4);
                    int d2 = (data[offset] & 0x0F);
                    int d3 = (data[offset + 1] >> 4);
                    int d4 = (data[offset + 1] & 0x0F);
                    if (d1 > 9 || d2 > 9 || d3 > 9 || d4 > 9) throw runtime_error("Invalid BCD in LLLVAR length for field " + to_string(field_id));
                    len = d1 * 1000 + d2 * 100 + d3 * 10 + d4;
                    offset += 2;
                    break;
                }
            }

            if (len > 999) {
                 throw runtime_error("Implausible length " + to_string(len) + " for field " + to_string(field_id));
            }

            int bytes_to_read = (def.encoding == FieldEncoding::BCD) ? (len + 1) / 2 : len;
            if (data.size() < offset + bytes_to_read) {
                LOG_WARN("Insufficient data for field {}: expected {} bytes, but only {} bytes available. Skipping field.", 
                         field_id, bytes_to_read, data.size() - offset);
                continue;
            }

            vector<uint8_t> field_data(data.begin() + offset, data.begin() + offset + bytes_to_read);

            if (def.encoding == FieldEncoding::BCD) {
                string bcd_str = bcd_to_string_local(field_data);
                msg.set_field(field_id, bcd_str.substr(0, len));
            } else {
                string value(field_data.begin(), field_data.end());
                // Trim apenas em campos texto: campos BINARY (DE-52 PIN block, DE-64 MAC)
                // podem terminar legitimamente em 0x20/0x09/etc e não podem perder bytes.
                if (def.format == FieldFormat::FIXED && def.encoding != FieldEncoding::BINARY) {
                    size_t last_char = value.find_last_not_of(" \t\n\r\f\v\0");
                     if (string::npos != last_char) {
                        value.erase(last_char + 1);
                    } else {
                        value.clear();
                    }
                }
                msg.set_field(field_id, value);
            }
            offset += bytes_to_read;
        }
    }
    return msg;
}

vector<uint8_t> IsoParser::build(const IsoMessage& msg) {
    initialize_field_defs();
    vector<uint8_t> data;
    map<int, vector<uint8_t>> field_data_map;
    vector<bool> bitmap(129, false);
    bool has_secondary_bitmap = false;

    for (const auto& pair : msg.fields) {
        int field_id = pair.first;
        const string& value = pair.second;

        if (field_id < 2 || field_id > 128 || value.empty()) continue;

        bitmap[field_id] = true;
        if (field_id > 64) {
            has_secondary_bitmap = true;
        }

        const auto& def = field_defs.at(field_id);
        vector<uint8_t> field_bytes;

        if (def.encoding == FieldEncoding::BCD) {
            field_bytes = string_to_bcd_local(value);
        } else {
            string padded_val = value;
            if (def.format == FieldFormat::FIXED && padded_val.length() < def.max_length) {
                padded_val.append(def.max_length - padded_val.length(), ' ');
            }
            field_bytes.assign(padded_val.begin(), padded_val.end());
        }
        field_data_map[field_id] = field_bytes;
    }

    auto mti_bcd = string_to_bcd_local(msg.mti);
    data.insert(data.end(), mti_bcd.begin(), mti_bcd.end());

    if (has_secondary_bitmap) {
        bitmap[1] = true;
    }
    int bitmap_size_bytes = has_secondary_bitmap ? 16 : 8;
    vector<uint8_t> bitmap_bytes(bitmap_size_bytes, 0);
    for (int i = 1; i <= bitmap_size_bytes * 8; ++i) {
        if (bitmap[i]) {
            bitmap_bytes[(i - 1) / 8] |= (1 << (7 - ((i - 1) % 8)));
        }
    }
    data.insert(data.end(), bitmap_bytes.begin(), bitmap_bytes.end());

    for (int i = 2; i <= (has_secondary_bitmap ? 128 : 64); ++i) {
        if (bitmap[i]) {
            const auto& def = field_defs.at(i);
            const auto& field_bytes = field_data_map.at(i);
            const auto& value = msg.fields.at(i);

            if (def.format == FieldFormat::LLVAR) {
                auto len_bcd = string_to_bcd_local(to_string(value.length()));
                data.insert(data.end(), len_bcd.begin(), len_bcd.end());
            } else if (def.format == FieldFormat::LLLVAR) {
                auto len_bcd = string_to_bcd_local(to_string(value.length()));
                if (len_bcd.size() < 2) {
                    len_bcd.insert(len_bcd.begin(), 1, 0);
                }
                data.insert(data.end(), len_bcd.begin(), len_bcd.end());
            }

            data.insert(data.end(), field_bytes.begin(), field_bytes.end());
        }
    }

    return data;
}

string IsoParser::bcd_to_string(const vector<uint8_t>& data, size_t len, bool pad_left) {
    string s = bcd_to_string_local(data);
    return s.substr(0, len);
}

vector<uint8_t> IsoParser::string_to_bcd(const string& str, size_t len, bool pad_left) {
    return string_to_bcd_local(str);
}