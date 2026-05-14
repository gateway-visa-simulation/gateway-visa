#pragma once
#include "iso_message.h"
#include <vector>
#include <cstdint>

using namespace std;

class IsoBuilder {
public:
    static IsoMessage build_response(const IsoMessage& request, const string& response_code);
};
