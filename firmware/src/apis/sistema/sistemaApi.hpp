#pragma once
#include "sistemaDTO.hpp"
#include "../../connections/CommunicationContainer.hpp"

namespace Apis::Sistema {

inline void registrarSistemaApi() {
    auto& comm = Connections::comm;

    comm.srv.bind("system.echo", [](std::string msg) -> std::string {
        return msg;
    });

    comm.srv.bind("add", [](int a, int b) -> int {
        return a + b;
    });
}

} // namespace Apis::Sistema