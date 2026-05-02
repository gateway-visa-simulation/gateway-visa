#include "iso_builder.h"

IsoMessage IsoBuilder::build_response(const IsoMessage& request, const std::string& response_code) {
    IsoMessage response;

    // MTI
    if (request.mti == "0100") {
        response.mti = "0110";
    } else if (request.mti == "0200") {
        response.mti = "0210";
    } else if (request.mti == "0400") {
        response.mti = "0410";
    } else {
        response.mti = "0" + request.mti.substr(1, 1) + "1" + request.mti.substr(3, 1);
    }

    // Copy mandatory fields
    int mandatory_fields[] = {2, 3, 4, 7, 11, 12, 13, 22, 41, 42, 49};
    for (int field : mandatory_fields) {
        if (request.has_field(field)) {
            response.set_field(field, request.get_field(field));
        }
    }

    // Set response code
    response.set_field(39, response_code);

    return response;
}
