// Archivo: ./src/stubs/serverExample.h
#pragma once
#include <Arduino.h>
#include <string.h>
#include "communication/layers/CapaAplicacion.h"
#include "state/EstadoSistema.h"
#include "debug/DebugOLED.h"

namespace ServerExampleStub {

    // =========================================================================
    // 1. CRIPTOGRAFÍA DEL PROCESO DE USUARIO (Canal Completo y Campo Sensible)
    // =========================================================================
    static const uint8_t CLAVE_CANAL_TLS[16] = {
        0x4A, 0x91, 0x2C, 0xE7, 0x55, 0xB3, 0x18, 0x8F,
        0xD2, 0x3E, 0x7A, 0xC1, 0x60, 0x09, 0xF4, 0x33
    };

    static const uint8_t CLAVE_CAMPO_SMTP[12] = {
        0x73, 0x6D, 0x74, 0x70, 0x5F, 0x73, 0x65, 0x63, 0x5F, 0x6B, 0x65, 0x79
    };

    // Hook de Capa 6: Encripta/Desencripta el mensaje MessagePack completo in-place (0 bytes extra)
    inline bool transformarMensajeCanalTLS(int, uint16_t, uint8_t* buf, size_t& len, size_t) {
        for (size_t i = 0; i < len; i++) {
            buf[i] ^= (uint8_t)(CLAVE_CANAL_TLS[i & 0x0F] ^ (uint8_t)(i * 29));
        }
        return true;
    }

    // Desencripta un campo sensible puntual (ej. password SMTP cifrada en Capa 7)
    inline void desencriptarCampoSensible(const uint8_t* cifrado, size_t len, char* salidaTexto, size_t capSalida) {
        size_t maxCopy = (len < capSalida - 1) ? len : (capSalida - 1);
        for (size_t i = 0; i < maxCopy; i++) {
            salidaTexto[i] = (char)(cifrado[i] ^ CLAVE_CAMPO_SMTP[i % sizeof(CLAVE_CAMPO_SMTP)] ^ (uint8_t)(i * 7));
        }
        salidaTexto[maxCopy] = '\0';
    }

    // =========================================================================
    // 2. DTOs (DATA TRANSFER OBJECTS) CON MARSHALLING / UNMARSHALLING EXPLÍCITO
    // =========================================================================
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

    // =========================================================================
    // 3. BASE DE DATOS Y ORM CASERO EN FIRMWARE (Estático en .bss, 0 Heap)
    // =========================================================================
    struct RegistroUsuarioDB {
        const char* username;
        const char* pinHash;
        const char* rol; // "ADMIN" | "OPERADOR"
        bool activo;
    };

    struct RegistroConfigAuditoriaDB {
        uint32_t idRegistro = 0;
        float tempMin = -18.0f;
        float tempMax = -12.0f;
        char smtpPasswordClara[32] = "sin_configurar";
        char modificadoPor[20] = "BOOT";
        char rolUsuario[12] = "SYSTEM";
        const char* medioUsado = "INTERNO";
        uint32_t timestampSeg = 0;

        // Método ORM para serializar la fila hacia el cliente
        bool serializarFila(EscritorMsgPack& w, uint32_t totalFilasDB) const {
            return w.iniciarArray(9) &&
                   w.escribirTexto("OK") &&
                   w.escribirUInt(idRegistro) &&
                   w.escribirFloat(tempMin) &&
                   w.escribirFloat(tempMax) &&
                   w.escribirTexto(smtpPasswordClara) &&
                   w.escribirTexto(modificadoPor) &&
                   w.escribirTexto(rolUsuario) &&
                   w.escribirTexto(medioUsado) &&
                   w.escribirUInt(totalFilasDB);
        }
    };

    class BaseDeDatosFirmware {
      public:
        static const size_t MAX_HISTORIAL_AUDITORIA = 8;
        RegistroConfigAuditoriaDB tablaAuditoria[MAX_HISTORIAL_AUDITORIA];
        size_t totalInserciones = 0;

        const RegistroUsuarioDB tablaUsuarios[2] = {
            { "silvana_admin", "HASH_ADMIN_2026", "ADMIN",    true },
            { "juan_operador", "HASH_OPER_2026",  "OPERADOR", true }
        };

        const RegistroUsuarioDB* buscarUsuario(const char* username) const {
            for (size_t i = 0; i < 2; i++) {
                if (tablaUsuarios[i].activo && strcmp(tablaUsuarios[i].username, username) == 0) {
                    return &tablaUsuarios[i];
                }
            }
            return nullptr;
        }

        const RegistroConfigAuditoriaDB& insertarConfiguracion(
            float tMin, float tMax, const char* passDesencriptada,
            const char* usuario, const char* rol, int idNodo) {

            size_t slot = totalInserciones % MAX_HISTORIAL_AUDITORIA;
            totalInserciones++;

            RegistroConfigAuditoriaDB& fila = tablaAuditoria[slot];
            fila.idRegistro = (uint32_t)totalInserciones;
            fila.tempMin = tMin;
            fila.tempMax = tMax;
            strncpy(fila.smtpPasswordClara, passDesencriptada, sizeof(fila.smtpPasswordClara) - 1);
            fila.smtpPasswordClara[sizeof(fila.smtpPasswordClara) - 1] = '\0';
            strncpy(fila.modificadoPor, usuario, sizeof(fila.modificadoPor) - 1);
            fila.modificadoPor[sizeof(fila.modificadoPor) - 1] = '\0';
            strncpy(fila.rolUsuario, rol, sizeof(fila.rolUsuario) - 1);
            fila.rolUsuario[sizeof(fila.rolUsuario) - 1] = '\0';
            fila.medioUsado = (idNodo == 0) ? "USB_FISICO" : "WIFI_TCP";
            fila.timestampSeg = gEstadoSistema.ultimoUnixTimestamp;
            return fila;
        }
    };

    static BaseDeDatosFirmware dbFirmware;

    // =========================================================================
    // 4. MIDDLEWARE DE POLÍTICA DE ACCESO (Certificado TLS + Rol + Rango)
    // =========================================================================
    inline bool evaluarPoliticaProcesoUsuario(
        const char* comando, size_t cantidadArgs, LectorMsgPack argsCopia,
        const MetaCall& meta, char* codigoRechazo, size_t capCodigo) {

        DebugOLED::registrarRx(meta.idNodo, comando, meta.reqId);

        // Regla específica para la ruta protegida RPC_GUARDAR_CONFIG_SEGURA
        if (strcmp(comando, "RPC_GUARDAR_CONFIG_SEGURA") == 0) {
            if (cantidadArgs < 7) {
                strncpy(codigoRechazo, "DTO_INCOMPLETO", capCodigo);
                DebugOLED::registrarError("P:DTO_INC");
                return false;
            }

            CertificadoLiteDTO cert;
            UsuarioCredencialDTO cred;
            ConfigHeladeraDTO cfg;

            if (!cert.deserializar(argsCopia) || !cred.deserializar(argsCopia) || !cfg.deserializar(argsCopia)) {
                strncpy(codigoRechazo, "FORMATO_DTO_INVALIDO", capCodigo);
                DebugOLED::registrarError("P:BAD_DTO");
                return false;
            }

            // 1. Validar pseudo-certificado TLS de la aplicación
            if (strcmp(cert.firmaHex, "CERT-TLS-UNTDF-2026-VALIDO") != 0) {
                strncpy(codigoRechazo, "CERT_TLS_INVALIDO", capCodigo);
                DebugOLED::registrarError("P:CERT_ERR");
                return false;
            }

            // 2. Autenticar Usuario y verificar Rol en la Base de Datos del Firmware
            const RegistroUsuarioDB* usr = dbFirmware.buscarUsuario(cred.username);
            if (!usr || strcmp(usr->pinHash, cred.pinHash) != 0) {
                strncpy(codigoRechazo, "USUARIO_O_PIN_INVALIDO", capCodigo);
                DebugOLED::registrarError("P:USR_ERR");
                return false;
            }

            if (strcmp(usr->rol, "ADMIN") != 0) {
                strncpy(codigoRechazo, "REQUIERE_ROL_ADMIN", capCodigo);
                DebugOLED::registrarError("P:NO_ADMIN");
                return false;
            }

            // 3. Regla de Dominio: Bloquear umbrales destructivos (ej. -126°C y 126°C)
            if (cfg.tempMin < -40.0f || cfg.tempMax > 20.0f || cfg.tempMin >= cfg.tempMax) {
                strncpy(codigoRechazo, "UMBRAL_FUERA_DE_RANGO_SEGURO", capCodigo);
                DebugOLED::registrarError("P:RANGO_ERR");
                return false;
            }
        }

        return true;
    }

    // =========================================================================
    // 5. PROCEDIMIENTOS DE NEGOCIO Y REGISTRO DE STUBS (CAPA 7)
    // =========================================================================
    struct RespuestaEstado {
        float resultado;
        const char* estado;
        uint32_t heapLibre;
    };

    inline RespuestaEstado ejecutarSumarYEstado(float base, float factor) {
        float resultado = base + (factor * 1.5f);
        gEstadoSistema.ultimoResultadoCalculado = resultado;
        gEstadoSistema.totalLlamadasRecibidas++;

        return {
            resultado,
            gEstadoSistema.relojSincronizado ? "SYNC_OK" : "ESTABLE",
            ESP.getFreeHeap()
        };
    }

    inline void registrarStubsServidor(CapaAplicacion& appRpc, CapaPresentacion& capaPres) {
        // 1. Activar Encriptación de Mensaje Completo en Capa 6
        capaPres.configurarTransformadores(transformarMensajeCanalTLS, transformarMensajeCanalTLS);

        // 2. Activar Middleware de Políticas en Capa 7 (Proceso de Usuario)
        appRpc.establecerEvaluadorPolitica(evaluarPoliticaProcesoUsuario);

        // Ruta 1: Ejemplo clásico de cálculo
        appRpc.on("RPC_SUMAR_Y_ESTADO", [](size_t cantArgs, LectorMsgPack& args,
                                           EscritorMsgPack& resp, const MetaCall&) -> bool {
            if (cantArgs < 2) return false;

            float base = 0.0f;
            float factor = 0.0f;
            if (!args.leerFloat(base) || !args.leerFloat(factor)) return false;

            RespuestaEstado out = ejecutarSumarYEstado(base, factor);

            return resp.iniciarArray(4) &&
                   resp.escribirTexto("OK") &&
                   resp.escribirFloat(out.resultado) &&
                   resp.escribirTexto(out.estado) &&
                   resp.escribirUInt(out.heapLibre);
        });

        // Ruta 2: Ejemplo completo con Certificado TLS + Usuario/Rol + Cifrado de Campo + DB/ORM
        appRpc.on("RPC_GUARDAR_CONFIG_SEGURA", [](size_t cantArgs, LectorMsgPack& args,
                                                  EscritorMsgPack& resp, const MetaCall& meta) -> bool {
            if (cantArgs < 7) return false;

            CertificadoLiteDTO cert;
            UsuarioCredencialDTO cred;
            ConfigHeladeraDTO dto;
            if (!cert.deserializar(args) || !cred.deserializar(args) || !dto.deserializar(args)) {
                return false;
            }

            // Desencriptamos el campo sensible (clave SMTP) recién en la lógica de negocio
            char passPlana[32] = {0};
            desencriptarCampoSensible(dto.smtpPasswordCifrada, dto.lenPasswordCifrada, passPlana, sizeof(passPlana));

            const RegistroUsuarioDB* usr = dbFirmware.buscarUsuario(cred.username);
            const char* rol = usr ? usr->rol : "ADMIN";

            // Guardamos en la Base de Datos del Firmware mediante el ORM estático
            const RegistroConfigAuditoriaDB& filaGuardada = dbFirmware.insertarConfiguracion(
                dto.tempMin,
                dto.tempMax,
                passPlana,
                cred.username,
                rol,
                meta.idNodo
            );

            gEstadoSistema.totalLlamadasRecibidas++;
            return filaGuardada.serializarFila(resp, (uint32_t)dbFirmware.totalInserciones);
        });
    }

}