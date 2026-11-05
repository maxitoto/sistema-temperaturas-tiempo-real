#pragma once
#include <Arduino.h>
#include "orm/ORM.hpp"
#include "EsquemaFisico.hpp"
#include "../rtos/SystemBus.hpp"
#include "../connections/CommunicationContainer.hpp"
#include "../utils/TimeService.hpp"

namespace Audit {

inline void asegurarUsuarioSistema() {
    auto lock = Rtos::sysBus.lockDb(3000);
    if (!lock.isLocked()) return;

    db.setSession("system_boot", ROL_ADMIN);
    if (!SGBD::ModuloDQL::getInstance().existeRegistro("Usuarios", "id_usuario", "1")) {
        uint32_t ts = Time::nowUnix();
        db.from("Usuarios").insert({
            {"id_usuario",         "1"},
            {"usuario",            "system"},
            {"salt",               "0000000000000000"},
            {"contrasena_hash",    "SYSTEM_INTERNAL_ACCOUNT"},
            {"rol",                String(static_cast<int>(ROL_ADMIN))},
            {"intentos_fallidos",  "0"},
            {"bloqueado",          "0"},
            {"activa",             "1"},
            {"fecha_creacion",     String(ts)},
            {"fecha_modificacion", String(ts)}
        });
    }
}

inline MedioConexion detectarMedioActivo() {
    auto& proxy = Connections::comm.proxy;
    if (proxy.isMediumConnected(Connections::MediumType::UART_S)) {
        return MedioConexion::MEDIO_USB;
    }
    if (proxy.isMediumConnected(Connections::MediumType::WIFI)) {
        return (WiFi.status() == WL_CONNECTED)
               ? MedioConexion::MEDIO_WIFI_STA
               : MedioConexion::MEDIO_WIFI_AP;
    }
    return MedioConexion::MEDIO_INTERNO;
}

inline void registrar(const char* accion,
                      const char* tablaAfectada,
                      const String& valorPrevio,
                      const String& valorNuevo,
                      const String& detalle,
                      CriticidadEvento criticidad = CriticidadEvento::CRITICIDAD_BAJA,
                      ResultadoAuditoria resultado = ResultadoAuditoria::RESULTADO_EXITO,
                      MedioConexion medio = MedioConexion::MEDIO_INTERNO,
                      const char* origenRemoto = "LOCAL",
                      int idUsuario = 1) {
    auto lock = Rtos::sysBus.lockDb(2500);
    if (!lock.isLocked()) return;

    db.setSession("system_audit", ROL_ADMIN);
    db.from("Auditoria").insert({
        {"id_usuario",     String(idUsuario)},
        {"accion",         String(accion)},
        {"tabla_afectada", String(tablaAfectada)},
        {"valor_previo",   valorPrevio.substring(0, 64)},
        {"valor_nuevo",    valorNuevo.substring(0, 64)},
        {"detalle",        detalle.substring(0, 120)},
        {"medio_conexion", String(static_cast<int>(medio))},
        {"origen_remoto",  String(origenRemoto)},
        {"resultado",      String(static_cast<int>(resultado))},
        {"criticidad",     String(static_cast<int>(criticidad))},
        {"activa",         "1"},
        {"fecha_creacion", String(Time::nowUnix())}
    });
}

} // namespace Audit