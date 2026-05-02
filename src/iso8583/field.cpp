#include "field.h"

const std::map<int, FieldDef>& IsoFieldDef::get_definitions() {
    static const std::map<int, FieldDef> definitions = {
        {2,  {2, 19, FieldFormat::LLVAR, FieldEncoding::BCD}},       // PAN
        {3,  {3, 6, FieldFormat::FIXED, FieldEncoding::BCD}},        // Processing Code
        {4,  {4, 12, FieldFormat::FIXED, FieldEncoding::BCD}},       // Amount
        {7,  {7, 10, FieldFormat::FIXED, FieldEncoding::BCD}},       // Transmission DateTime
        {11, {11, 6, FieldFormat::FIXED, FieldEncoding::BCD}},       // STAN
        {12, {12, 6, FieldFormat::FIXED, FieldEncoding::BCD}},       // Local Time
        {13, {13, 4, FieldFormat::FIXED, FieldEncoding::BCD}},       // Local Date
        {22, {22, 3, FieldFormat::FIXED, FieldEncoding::BCD}},       // POS Entry Mode
        {39, {39, 2, FieldFormat::FIXED, FieldEncoding::ASCII}},     // Response Code
        {41, {41, 8, FieldFormat::FIXED, FieldEncoding::ASCII}},     // Terminal ID
        {42, {42, 15, FieldFormat::FIXED, FieldEncoding::ASCII}},    // Merchant ID
        {49, {49, 3, FieldFormat::FIXED, FieldEncoding::ASCII}},     // Currency Code
        {52, {52, 8, FieldFormat::FIXED, FieldEncoding::BINARY}}     // PIN Block (64 bits = 8 bytes)
    };
    return definitions;
}

bool IsoFieldDef::is_valid_for_mti(const std::string& mti, int field) {
    if (mti == "0110" && field == 39) return true;
    if (mti == "0100" && field == 39) return false;
    return true;
}
