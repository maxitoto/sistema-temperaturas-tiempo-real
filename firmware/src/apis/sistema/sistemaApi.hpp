#pragma once
#include "sistemaDTO.hpp"
#include "../../connections/CommunicationContainer.hpp"
#include "../../utils/TimeService.hpp"
#include "../../database/AuditService.hpp"

namespace Apis::Sistema {

inline void registrarSistemaApi() {
    auto& comm = Connections::comm;

    comm.srv.bind("system.echo", [](std::string msg) -> std::string {
        return msg;
    });

    comm.srv.bind("add", [](int a, int b) -> int {
        return a + b;
    });

    // Permite al cliente Electron inyectar su timestamp Unix (en s o ms) cuando no hay internet
    comm.srv.bind("system.set_time", [](uint64_t epochIn) -> bool {
        uint32_t prev = Time::nowUnix();
        bool ok = Time::timeSrv.establecerEpoch(epochIn);
        if (ok) {
            Audit::registrar(
                "TIME_SET_CLIENT", "ServidoresNtp",
                String(prev), String(Time::nowUnix()),
                "Hora sincronizada desde cliente conectado",
                CriticidadEvento::CRITICIDAD_BAJA,
                ResultadoAuditoria::RESULTADO_EXITO,
                Audit::detectarMedioActivo(), "CLIENT_RPC"
            );
        }
        return ok;
    });

    // Devuelve el timestamp Unix actual del firmware
    comm.srv.bind("system.get_time", []() -> uint32_t {
        return Time::nowUnix();
    });
}

} // namespace Apis::Sistema