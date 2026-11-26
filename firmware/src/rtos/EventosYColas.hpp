#pragma once
#include <stdint.h>
#include <string.h>
#include "PrimitivasRTOS.hpp"
#include "RelojSistema.hpp"

namespace RTOS {

    enum class TipoSonidoEvento : uint8_t {
        NINGUNO = 0,
        BOOT_SISTEMA,
        CLIENTE_CONECTADO,
        CLIENTE_DESCONECTADO,
        SENSOR_CONECTADO,
        SENSOR_DESCONECTADO,
        TAREA_RPC_EJECUTADA
    };

    enum class TipoRegistroDB : uint8_t {
        LOTE_CENSOS_2_MINUTOS = 0,
        EVENTO_CAMBIO_SENSOR,
        EVENTO_SISTEMA_BUS,
        AUDITORIA_TRANSACCION,
        AUDITORIA_RED
    };

    // Estructura Plana (POD): Usa arreglos char[N] fijos en lugar de String o punteros
    // Estructura Plana (POD): Usa arreglos char[N] fijos en lugar de String o punteros
    struct RegistroPersistenciaPOD {
        TipoRegistroDB tipo;
        uint32_t       timestampUnix;
        PgDateTime     creadoEl;            // "YYYY-MM-DD HH:MM:SS" listo para PostgreSQL
        int32_t        idRelacionado;       // id_sensor o id_usuario real en BD
        int16_t        estadoPrevio;
        int16_t        estadoNuevo;
        uint8_t        resultadoAuditoria;  // ResultadoAuditoria (0 = EXITO, etc.)
        uint8_t        criticidadAuditoria; // CriticidadEvento (1 = BAJA, etc.)
        float          tempCruda;
        float          tempCorregida;
        char           usuarioIntentado[20];
        char           origenOClave[24];    // MAC del sensor o IP/Medio del cliente
        char           detalle[48];         // Acción o descripción
    };

    inline ColaSegura<TipoSonidoEvento, 12>        colaSonidos;
    inline ColaSegura<RegistroPersistenciaPOD, 16> colaPersistenciaDB;

    inline void encolarSonido(TipoSonidoEvento sonido) {
        colaSonidos.enviar(sonido, 0);
    }

    inline void encolarPersistencia(const RegistroPersistenciaPOD& reg) {
        colaPersistenciaDB.enviar(reg, 0);
    }

    // Helper directo para encolar una Auditoría desde cualquier RPC o Middleware
    inline void encolarAuditoria(
        TipoRegistroDB tipoAuditoria,
        int32_t idUsuarioReal,
        const char* usuarioIntentado,
        const char* origenIpOMedio,
        const char* detalleAccion,
        uint8_t resultado = 0,
        uint8_t criticidad = 1
    ) {
        RegistroPersistenciaPOD reg{};
        reg.tipo                = tipoAuditoria;
        reg.timestampUnix       = RelojSistema::obtenerTimestampActual();
        reg.creadoEl            = RelojSistema::fechaHoraActualPg();
        reg.idRelacionado       = idUsuarioReal;
        reg.resultadoAuditoria  = resultado;
        reg.criticidadAuditoria = criticidad;
        if (usuarioIntentado) {
            strncpy(reg.usuarioIntentado, usuarioIntentado, sizeof(reg.usuarioIntentado) - 1);
        }
        if (origenIpOMedio) {
            strncpy(reg.origenOClave, origenIpOMedio, sizeof(reg.origenOClave) - 1);
        }
        if (detalleAccion) {
            strncpy(reg.detalle, detalleAccion, sizeof(reg.detalle) - 1);
        }
        colaPersistenciaDB.enviar(reg, 0);
    }

} // namespace RTOS