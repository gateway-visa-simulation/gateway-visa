#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstdint>

enum class FieldFormat {
    FIXED, LLVAR, LLLVAR
};

enum class FieldEncoding {
    ASCII, BCD, BINARY
};

struct FieldDef {
    int id;
    int max_length;
    FieldFormat format;
    FieldEncoding encoding;
};

class IsoMessage {
public:
    std::string mti;
    std::map<int, std::string> fields;

    bool has_field(int field) const {
        return fields.find(field) != fields.end();
    }

    std::string get_field(int field) const {
        auto it = fields.find(field);
        return it != fields.end() ? it->second : "";
    }

    void set_field(int field, const std::string& val) {
        fields[field] = val;
    }

    std::string mask_pan() const;
    std::string get_unique_key() const;
};
