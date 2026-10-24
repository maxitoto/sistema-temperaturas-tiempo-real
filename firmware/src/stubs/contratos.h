// ===== src/stubs/contratos.h =====
#pragma once
#include <Arduino.h>
#include "communication/layers/CapaAplicacion.h"

namespace Contratos {

    // ---------- Nombres de procedimientos ----------
    static const char* const RPC_SUMAR_Y_ESTADO         = "RPC_SUMAR_Y_ESTADO";
    static const char* const RPC_GUARDAR_CONFIG_SEGURA  = "RPC_GUARDAR_CONFIG_SEGURA";
    static const char* const RPC_SYNC_RELOJ             = "RPC_SYNC_RELOJ";

    // ---------- DTOs de entrada ----------
    struct CertificadoLiteDTO {
        char emisor[24];
        char firmaHex[36];
        bool deserializar(LectorMsgPack& r) {
            return r.leerTexto(emisor, sizeof(emisor)) &&
                   r.leerTexto(firmaHex, sizeof(firmaHex));
        }
    };

    struct UsuarioCredencialDTO {
        char username[20];
        char pinHash[24];
        bool deserializar(LectorMsgPack& r) {
            return r.leerTexto(username, sizeof(username)) &&
                   r.leerTexto(pinHash, sizeof(pinHash));
        }
    };

    struct ConfigHeladeraDTO {
        float tempMin;
        float tempMax;
        uint8_t smtpPasswordCifrada[48];
        size_t lenPasswordCifrada = 0;
        bool deserializar(LectorMsgPack& r) {
            return r.leerFloat(tempMin) &&
                   r.leerFloat(tempMax) &&
                   r.leerBinario(smtpPasswordCifrada, sizeof(smtpPasswordCifrada), lenPasswordCifrada);
        }
    };

    // ---------- DTOs de salida ----------
    struct RespuestaEstado {
        float resultado;
        const char* estado;
        uint32_t heapLibre;
        bool serializar(EscritorMsgPack& w) const {
            return w.iniciarArray(4) &&
                   w.escribirTexto("OK") &&
                   w.escribirFloat(resultado) &&
                   w.escribirTexto(estado) &&
                   w.escribirUInt(heapLibre);
        }
    };

    struct RespuestaConfigDTO {
        uint32_t idRegistro;
        float tempMin;
        float tempMax;
        char smtpPasswordClara[32];
        char modificadoPor[20];
        char rolUsuario[12];
        const char* medioUsado;
        uint32_t totalFilas;
        bool serializar(EscritorMsgPack& w) const {
            return w.iniciarArray(9) &&
                   w.escribirTexto("OK") &&
                   w.escribirUInt(idRegistro) &&
                   w.escribirFloat(tempMin) &&
                   w.escribirFloat(tempMax) &&
                   w.escribirTexto(smtpPasswordClara) &&
                   w.escribirTexto(modificadoPor) &&
                   w.escribirTexto(rolUsuario) &&
                   w.escribirTexto(medioUsado) &&
                   w.escribirUInt(totalFilas);
        }
    };

    // ---------- Contrato: servicio que EXPONE el firmware (backend lo llama) ----------
    class IFirmware {
      public:
        virtual ~IFirmware() = default;
        virtual RespuestaEstado sumarYEstado(float base, float factor) = 0;
        virtual bool guardarConfig(const UsuarioCredencialDTO& cred,
                                   const ConfigHeladeraDTO& cfg,
                                   int idNodo,
                                   RespuestaConfigDTO& out) = 0;
    };

    // ---------- Contrato: servicio que EXPONE el backend (firmware lo llama) ----------
    class IBackend {
      public:
        virtual ~IBackend() = default;
        // asíncrono: devuelve reqId (0 = no se pudo enviar)
        virtual uint16_t sincronizarReloj(const char* motivo) = 0;
    };

}