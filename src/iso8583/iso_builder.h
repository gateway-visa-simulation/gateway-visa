#pragma once
#include "iso_message.h"
#include <vector>
#include <cstdint>

class IsoBuilder {
public:
    static IsoMessage build_response(const IsoMessage& request, const std::string& response_code);
};
