// src/stubs/clientExample.h
#pragma once
#include <Arduino.h>
#include "communication/layers/CapaAplicacion.h"
#include "state/EstadoSistema.h"
#include "stubs/contratos.h"

// STUB CLIENTE: implementa IBackend llamando por red
class BackendStub : public Contratos::IBackend {
  public:
    explicit BackendStub(CapaAplicacion& appRpc) : _appRpc(appRpc) {}

    uint16_t sincronizarReloj(const char* motivo) override {
        const uint32_t uptimeActual = millis();
        const char* certificadoDispositivo = "CERT-TLS-UNTDF-2026-VALIDO";

        return _appRpc.call(
            Contratos::RPC_SYNC_RELOJ,
            3,
            [uptimeActual, motivo, certificadoDispositivo](EscritorMsgPack& w) -> bool {
                return w.escribirUInt(uptimeActual) &&
                       w.escribirTexto(motivo) &&
                       w.escribirTexto(certificadoDispositivo);
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

  private:
    CapaAplicacion& _appRpc;
};

// uso (desde cualquier módulo del firmware):
//   BackendStub backend(pila.appRpc);
//   backend.sincronizarReloj("BOOT");