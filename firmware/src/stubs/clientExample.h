// Archivo: ./src/stubs/clientExample.h
#pragma once
#include <Arduino.h>
#include "communication/layers/CapaAplicacion.h"
#include "state/EstadoSistema.h"

namespace ClientExampleStub {

    inline uint16_t solicitarHoraAlBackend(CapaAplicacion& appRpc, const char* motivo) {
        const uint32_t uptimeActual = millis();
        const char* certificadoDispositivo = "CERT-TLS-UNTDF-2026-VALIDO";

        return appRpc.call(
            "RPC_SYNC_RELOJ",
            3,
            [uptimeActual, motivo, certificadoDispositivo](EscritorMsgPack& escritor) -> bool {
                return escritor.escribirUInt(uptimeActual) &&
                       escritor.escribirTexto(motivo) &&
                       escritor.escribirTexto(certificadoDispositivo);
            },
            [](bool exito, size_t cantDatos, LectorMsgPack& datos, const MetaCall&) {
                if (!exito || cantDatos < 2) return;

                uint32_t unixTimestamp = 0;
                if (datos.leerUInt32(unixTimestamp) &&
                    datos.leerTexto(gEstadoSistema.zonaHoraria, sizeof(gEstadoSistema.zonaHoraria))) {
                    gEstadoSistema.ultimoUnixTimestamp = unixTimestamp;
                    gEstadoSistema.relojSincronizado = true;
                }
            }
        );
    }

}