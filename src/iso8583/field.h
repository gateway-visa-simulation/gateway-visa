#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include "iso_message.h"

using namespace std;

// Map de definicoes de campo conhecido no sistema ISO 8583 (Visa/Mastercard padrao)
class IsoFieldDef {
public:
    static const map<int, FieldDef>& get_definitions();
    static bool is_valid_for_mti(const string& mti, int field);
};
