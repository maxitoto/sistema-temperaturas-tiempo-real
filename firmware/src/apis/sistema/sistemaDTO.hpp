#pragma once
#include <stdint.h>
#include <string>

namespace Apis::Sistema {

struct EchoDto {
    std::string mensaje;
};

struct SumaDto {
    int a;
    int b;
};

} // namespace Apis::Sistema