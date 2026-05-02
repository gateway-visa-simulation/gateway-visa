#pragma once
#include <vector>
#include <cstdint>
#include <bitset>

class Bitmap {
public:
    Bitmap();
    void set(int field);
    bool is_set(int field) const;
    std::vector<uint8_t> to_bytes() const;
    static Bitmap from_bytes(const std::vector<uint8_t>& bytes);
private:
    std::bitset<128> bits_;
};
