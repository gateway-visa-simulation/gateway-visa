#pragma once
#include <vector>
#include <cstdint>
#include <bitset>

using namespace std;

class Bitmap {
public:
    Bitmap();
    void set(int field);
    bool is_set(int field) const;
    vector<uint8_t> to_bytes() const;
    static Bitmap from_bytes(const vector<uint8_t>& bytes);
private:
    bitset<128> bits_;
};
