#include "bitmap.h"


Bitmap::Bitmap() {
    bits_.reset();
}

void Bitmap::set(int field) {
    if (field >= 1 && field <= 128) {
        bits_.set(field - 1);
        if (field > 64) {
            bits_.set(0); // Set bit 1 (secondary bitmap present)
        }
    }
}

bool Bitmap::is_set(int field) const {
    if (field >= 1 && field <= 128) {
        return bits_.test(field - 1);
    }
    return false;
}

vector<uint8_t> Bitmap::to_bytes() const {
    size_t size = bits_.test(0) ? 16 : 8;
    vector<uint8_t> bytes(size, 0);

    for (size_t i = 0; i < size * 8; ++i) {
        if (bits_.test(i)) {
            bytes[i / 8] |= (1 << (7 - (i % 8)));
        }
    }
    return bytes;
}

Bitmap Bitmap::from_bytes(const vector<uint8_t>& bytes) {
    Bitmap bmp;
    size_t size = min(bytes.size(), (size_t)16);
    for (size_t i = 0; i < size * 8; ++i) {
        if (bytes[i / 8] & (1 << (7 - (i % 8)))) {
            bmp.bits_.set(i);
        }
    }
    return bmp;
}
