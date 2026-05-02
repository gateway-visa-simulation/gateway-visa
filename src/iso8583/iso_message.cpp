#include "iso_message.h"

std::string IsoMessage::mask_pan() const {
    if (!has_field(2)) return "";
    std::string pan = get_field(2);
    if (pan.length() < 10) return pan;
    std::string masked = pan;
    for (size_t i = 6; i < masked.length() - 4; ++i) {
        masked[i] = '*';
    }
    return masked;
}

std::string IsoMessage::get_unique_key() const {
    std::string key = get_field(11) + "_" + get_field(2) + "_" + get_field(7);
    return key;
}
