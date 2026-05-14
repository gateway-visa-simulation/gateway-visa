#pragma once
#include <string>
#include <map>
#include <vector>
#include <cstdint>

using namespace std;

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
    string mti;
    map<int, string> fields;

    bool has_field(int field) const {
        return fields.find(field) != fields.end();
    }

    string get_field(int field) const {
        auto it = fields.find(field);
        return it != fields.end() ? it->second : "";
    }

    void set_field(int field, const string& val) {
        fields[field] = val;
    }

    string mask_pan() const;
    string get_unique_key() const;
};
