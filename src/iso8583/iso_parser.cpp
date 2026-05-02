#include "iso_parser.h"
#include "bitmap.h"
#include "field.h"
#include "../common/logger.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::string IsoParser::bcd_to_string(const std::vector<uint8_t>& data, size_t len, bool pad_left) {
    std::string res;
    for (uint8_t byte : data) {
        res += std::to_string((byte >> 4) & 0x0F);
        res += std::to_string(byte & 0x0F);
    }
    if (res.length() > len) {
        if (pad_left) {
            res = res.substr(res.length() - len);
        } else {
            res = res.substr(0, len);
        }
    }
    return res;
}

std::vector<uint8_t> IsoParser::string_to_bcd(const std::string& str, size_t len, bool pad_left) {
    std::string s = str;
    if (s.length() < len) {
        s = pad_left ? std::string(len - s.length(), '0') + s : s + std::string(len - s.length(), '0');
    }
    if (s.length() % 2 != 0) {
        s = pad_left ? "0" + s : s + "0";
    }
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < s.length(); i += 2) {
        uint8_t high = s[i] - '0';
        uint8_t low = s[i+1] - '0';
        bytes.push_back((high << 4) | low);
    }
    return bytes;
}

IsoMessage IsoParser::parse(const std::vector<uint8_t>& data) {
    IsoMessage msg;
    if (data.size() < 10) {
        throw std::runtime_error("Message too short to parse even MTI/Bitmap");
    }

    std::vector<uint8_t> mti_bytes(data.begin(), data.begin() + 2);
    msg.mti = bcd_to_string(mti_bytes, 4);

    std::vector<uint8_t> bitmap_bytes(data.begin() + 2, data.begin() + 10);
    Bitmap bmp = Bitmap::from_bytes(bitmap_bytes);

    size_t offset = 10;

    if (bmp.is_set(1)) {
        if (data.size() < 18) {
            throw std::runtime_error("Message too short for secondary bitmap");
        }
        std::vector<uint8_t> extended_bitmap_bytes(data.begin() + 2, data.begin() + 18);
        bmp = Bitmap::from_bytes(extended_bitmap_bytes);
        offset = 18;
    }

    const auto& defs = IsoFieldDef::get_definitions();

    for (int i = 2; i <= 128; ++i) {
        if (bmp.is_set(i)) {
            if (defs.find(i) == defs.end()) {
                LOG_WARN("Skipando parsing do campo nao suportado/desconhecido: DE{}", i);
                continue;
            }

            const auto& def = defs.at(i);
            size_t logical_len = def.max_length;

            if (def.format == FieldFormat::LLVAR) {
                if (offset >= data.size()) throw std::runtime_error("Truncated LLVAR prefix at DE" + std::to_string(i));
                logical_len = std::stoi(bcd_to_string({data[offset]}, 2));
                offset += 1;
            } else if (def.format == FieldFormat::LLLVAR) {
                if (offset + 1 >= data.size()) throw std::runtime_error("Truncated LLLVAR prefix at DE" + std::to_string(i));
                logical_len = std::stoi(bcd_to_string({data[offset], data[offset+1]}, 4));
                offset += 2;
            }

            size_t bytes_len = 0;
            switch(def.encoding) {
                case FieldEncoding::BCD:
                    bytes_len = (logical_len + 1) / 2;
                    break;
                case FieldEncoding::ASCII:
                case FieldEncoding::BINARY:
                    bytes_len = logical_len;
                    break;
            }

            if (offset + bytes_len > data.size()) {
                throw std::runtime_error("Message truncated parsing data for DE" + std::to_string(i));
            }

            std::vector<uint8_t> field_data(data.begin() + offset, data.begin() + offset + bytes_len);

            if (def.encoding == FieldEncoding::BCD) {
                msg.set_field(i, bcd_to_string(field_data, logical_len, def.format != FieldFormat::FIXED && logical_len % 2 != 0));
            } else if (def.encoding == FieldEncoding::ASCII) {
                msg.set_field(i, std::string(field_data.begin(), field_data.end()));
            } else if (def.encoding == FieldEncoding::BINARY) {
                std::stringstream ss;
                for(auto b: field_data) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
                msg.set_field(i, ss.str());
            }

            offset += bytes_len;
        }
    }
    return msg;
}

std::vector<uint8_t> IsoParser::build(const IsoMessage& msg) {
    std::vector<uint8_t> result;

    auto mti_bcd = string_to_bcd(msg.mti, 4);
    result.insert(result.end(), mti_bcd.begin(), mti_bcd.end());

    Bitmap bmp;
    for (const auto& [field, _] : msg.fields) {
        bmp.set(field);
    }

    auto bmp_bytes = bmp.to_bytes();
    result.insert(result.end(), bmp_bytes.begin(), bmp_bytes.end());

    const auto& defs = IsoFieldDef::get_definitions();

    for (int i = 2; i <= 128; ++i) {
        if (msg.has_field(i)) {
            if (defs.find(i) == defs.end()) {
                LOG_ERROR("Nao e possivel serializar campo {} - sem definicao de formatação.", i);
                continue;
            }

            const auto& def = defs.at(i);
            std::string val = msg.get_field(i);
            size_t logical_len = val.length();

            if (def.format == FieldFormat::LLVAR) {
                auto len_bcd = string_to_bcd(std::to_string(logical_len), 2, true);
                result.insert(result.end(), len_bcd.begin(), len_bcd.end());
            } else if (def.format == FieldFormat::LLLVAR) {
                auto len_bcd = string_to_bcd(std::to_string(logical_len), 4, true);
                result.insert(result.end(), len_bcd.begin(), len_bcd.end());
            }

            if (def.encoding == FieldEncoding::BCD) {
                auto val_bcd = string_to_bcd(val, logical_len, def.format != FieldFormat::FIXED && logical_len % 2 != 0);
                result.insert(result.end(), val_bcd.begin(), val_bcd.end());
            } else if (def.encoding == FieldEncoding::ASCII) {
                result.insert(result.end(), val.begin(), val.end());
            } else if (def.encoding == FieldEncoding::BINARY) {
                for (size_t k = 0; k < val.length(); k += 2) {
                    std::string byteStr = val.substr(k, 2);
                    uint8_t byte = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
                    result.push_back(byte);
                }
            }
        }
    }
    return result;
}
