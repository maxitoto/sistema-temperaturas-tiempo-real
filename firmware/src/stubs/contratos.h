// Archivo: ./src/stubs/contratos.h
#pragma once
#include <Arduino.h>
#include "communication/layers/CapaAplicacion.h"
#include "database/EsquemaFisico.h"

namespace Contratos {

    using namespace EsquemaFisicoDB;

    // ---------- Nombres de procedimientos RPC ----------
    static const char* const RPC_DATOS_EN_VIVO           = "RPC_DATOS_EN_VIVO";
    static const char* const RPC_SUMAR_Y_ESTADO          = "RPC_SUMAR_Y_ESTADO";
    static const char* const RPC_GUARDAR_CONFIG_SEGURA   = "RPC_GUARDAR_CONFIG_SEGURA";
    static const char* const RPC_SYNC_RELOJ              = "RPC_SYNC_RELOJ";

    // Endpoints RPC de Base de Datos (DQL y DML autenticados)
    static const char* const RPC_DB_LISTAR_SENSORES      = "RPC_DB_LISTAR_SENSORES";
    static const char* const RPC_DB_GUARDAR_SENSOR       = "RPC_DB_GUARDAR_SENSOR";
    static const char* const RPC_DB_ULTIMOS_SENSOS       = "RPC_DB_ULTIMOS_SENSOS";
    static const char* const RPC_DB_ULTIMOS_EVENTOS      = "RPC_DB_ULTIMOS_EVENTOS";
    static const char* const RPC_DB_ULTIMAS_AUDITORIAS   = "RPC_DB_ULTIMAS_AUDITORIAS";
    static const char* const RPC_DB_LISTAR_CORREOS       = "RPC_DB_LISTAR_CORREOS";
    static const char* const RPC_DB_CREAR_CORREO         = "RPC_DB_CREAR_CORREO";
    static const char* const RPC_DB_OBTENER_SMTP         = "RPC_DB_OBTENER_SMTP";
    static const char* const RPC_DB_OBTENER_RED_Y_NTP    = "RPC_DB_OBTENER_RED_Y_NTP";
    static const char* const RPC_DB_GUARDAR_RED          = "RPC_DB_GUARDAR_RED";
    static const char* const RPC_DB_LISTAR_USUARIOS      = "RPC_DB_LISTAR_USUARIOS";
    static const char* const RPC_DB_ACTUALIZAR_USUARIO   = "RPC_DB_ACTUALIZAR_USUARIO";

    // ---------- Cabecera de Seguridad unificada (Certificado TLS + Credencial Usuario) ----------
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
        char pinHash[32];
        bool deserializar(LectorMsgPack& r) {
            return r.leerTexto(username, sizeof(username)) &&
                   r.leerTexto(pinHash, sizeof(pinHash));
        }
    };

    struct CredencialRpcDTO {
        CertificadoLiteDTO   cert;
        UsuarioCredencialDTO usuario;
        bool deserializar(LectorMsgPack& r) {
            return cert.deserializar(r) && usuario.deserializar(r);
        }
    };

    // ---------- DTOs de Entrada para Mutaciones DML ----------
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

    struct GuardarSensorInputDTO {
        char  hexadecimal[17];
        float errorSuperior;
        float errorInferior;
        float umbralSuperior;
        float umbralInferior;
        bool deserializar(LectorMsgPack& r) {
            return r.leerTexto(hexadecimal, sizeof(hexadecimal)) &&
                   r.leerFloat(errorSuperior) &&
                   r.leerFloat(errorInferior) &&
                   r.leerFloat(umbralSuperior) &&
                   r.leerFloat(umbralInferior);
        }
    };

    struct CrearCorreoInputDTO {
        char     nombre[24];
        char     apellido[24];
        char     email[48];
        uint32_t tipoRecepcion = 0;
        bool deserializar(LectorMsgPack& r) {
            return r.leerTexto(nombre, sizeof(nombre)) &&
                   r.leerTexto(apellido, sizeof(apellido)) &&
                   r.leerTexto(email, sizeof(email)) &&
                   r.leerUInt32(tipoRecepcion);
        }
    };

    struct GuardarRedInputDTO {
        char    ssidSta[33];
        uint8_t passStaCifrada[64];
        size_t  lenPassSta = 0;
        char    ssidAp[33];
        uint8_t passApCifrada[64];
        size_t  lenPassAp = 0;
        bool deserializar(LectorMsgPack& r) {
            return r.leerTexto(ssidSta, sizeof(ssidSta)) &&
                   r.leerBinario(passStaCifrada, sizeof(passStaCifrada), lenPassSta) &&
                   r.leerTexto(ssidAp, sizeof(ssidAp)) &&
                   r.leerBinario(passApCifrada, sizeof(passApCifrada), lenPassAp);
        }
    };

    struct ActualizarUsuarioInputDTO {
        uint32_t idUsuarioObjetivo = 0;
        char     nuevoUsername[20];
        uint8_t  nuevaPasswordCifrada[32];
        size_t   lenNuevaPassword = 0;
        bool deserializar(LectorMsgPack& r) {
            return r.leerUInt32(idUsuarioObjetivo) &&
                   r.leerTexto(nuevoUsername, sizeof(nuevoUsername)) &&
                   r.leerBinario(nuevaPasswordCifrada, sizeof(nuevaPasswordCifrada), lenNuevaPassword);
        }
    };

    // ---------- DTOs de Salida y Serializadores de Filas ----------
    struct RespuestaDatosEnVivoDTO {
        float numeroRandom;
        bool serializar(EscritorMsgPack& w) const {
            return w.iniciarArray(2) &&
                   w.escribirTexto("OK") &&
                   w.escribirFloat(numeroRandom);
        }
    };

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

    inline bool serializarFilaSensor(EscritorMsgPack& w, const FilaSensor& f) {
        return w.iniciarArray(9) &&
               w.escribirUInt(f.id_sensor) &&
               w.escribirTexto(f.hexadecimal) &&
               w.escribirFloat(f.error_superior) &&
               w.escribirFloat(f.error_inferior) &&
               w.escribirFloat(f.umbral_superior) &&
               w.escribirFloat(f.umbral_inferior) &&
               w.escribirUInt(f.activa) &&
               w.escribirUInt(f.fecha_creacion) &&
               w.escribirUInt(f.fecha_modificacion);
    }

    inline bool serializarFilaSenso(EscritorMsgPack& w, const FilaSenso& f) {
        return w.iniciarArray(6) &&
               w.escribirUInt(f.id_senso) &&
               w.escribirUInt(f.id_sensor) &&
               w.escribirFloat(f.temperatura_cruda) &&
               w.escribirFloat(f.temperatura_corregida) &&
               w.escribirUInt(f.estado_sensor) &&
               w.escribirUInt(f.fecha_creacion);
    }

    inline bool serializarFilaEvento(EscritorMsgPack& w, const FilaEventoSensor& f) {
        return w.iniciarArray(7) &&
               w.escribirUInt(f.id_evento_sensor) &&
               w.escribirUInt(f.id_sensor) &&
               w.escribirUInt(f.id_senso) &&
               w.escribirUInt(f.estado_previo_sensor) &&
               w.escribirUInt(f.evento) &&
               w.escribirUInt(f.criticidad) &&
               w.escribirUInt(f.fecha_creacion);
    }

    inline bool serializarFilaAuditoria(EscritorMsgPack& w, const FilaAuditoria& f) {
        return w.iniciarArray(5) &&
               w.escribirUInt(f.id_auditoria) &&
               w.escribirUInt(f.id_usuario) &&
               w.escribirTexto(f.accion) &&
               w.escribirTexto(f.detalle) &&
               w.escribirUInt(f.fecha_creacion);
    }

    inline bool serializarFilaCorreo(EscritorMsgPack& w, const FilaCorreo& f) {
        return w.iniciarArray(7) &&
               w.escribirUInt(f.id_correo) &&
               w.escribirTexto(f.nombre) &&
               w.escribirTexto(f.apellido) &&
               w.escribirTexto(f.correo_electronico) &&
               w.escribirUInt(f.tipo_recepcion_mensajes) &&
               w.escribirUInt(f.estado_envio) &&
               w.escribirUInt(f.fecha_modificacion);
    }

    // Serializa un usuario sin exponer jamás el campo contrasena por red
    inline bool serializarFilaUsuarioResumen(EscritorMsgPack& w, const FilaUsuario& f) {
        return w.iniciarArray(5) &&
               w.escribirUInt(f.id_usuario) &&
               w.escribirTexto(f.usuario) &&
               w.escribirUInt(f.rol) &&
               w.escribirUInt(f.activa) &&
               w.escribirUInt(f.fecha_modificacion);
    }

    // ---------- Contrato: servicio que EXPONE el firmware (backend lo llama) ----------
    class IFirmware {
      public:
        virtual ~IFirmware() = default;
        virtual RespuestaDatosEnVivoDTO leerDatosEnVivo() = 0;
        virtual RespuestaEstado sumarYEstado(float base, float factor) = 0;
        virtual bool guardarConfig(const UsuarioCredencialDTO& cred,
                                   const ConfigHeladeraDTO& cfg,
                                   int idNodo,
                                   RespuestaConfigDTO& out) = 0;

        virtual size_t listarSensores(const UsuarioCredencialDTO& cred, FilaSensor* out, size_t maxItems) = 0;
        virtual bool   guardarSensor(const UsuarioCredencialDTO& cred, const GuardarSensorInputDTO& in, FilaSensor& out) = 0;
        virtual size_t listarUltimosSensos(const UsuarioCredencialDTO& cred, uint16_t idSensor, FilaSenso* out, size_t maxItems) = 0;
        virtual size_t listarUltimosEventos(const UsuarioCredencialDTO& cred, uint16_t idSensor, FilaEventoSensor* out, size_t maxItems) = 0;
        virtual size_t listarUltimasAuditorias(const UsuarioCredencialDTO& cred, FilaAuditoria* out, size_t maxItems) = 0;
        virtual size_t listarCorreos(const UsuarioCredencialDTO& cred, FilaCorreo* out, size_t maxItems) = 0;
        virtual bool   crearCorreo(const UsuarioCredencialDTO& cred, const CrearCorreoInputDTO& in, FilaCorreo& out) = 0;
        virtual bool   obtenerConfigSmtp(const UsuarioCredencialDTO& cred, FilaConfiguracionSMTP& out) = 0;
        virtual bool   obtenerConfigRedYNtp(const UsuarioCredencialDTO& cred, FilaConfiguracionConectividad& outRed, FilaServidorNTP* outNtp, size_t& totalNtp) = 0;
        virtual bool   guardarConfigRed(const UsuarioCredencialDTO& cred, const GuardarRedInputDTO& in, FilaConfiguracionConectividad& outRed) = 0;

        // Gestión de los 3 Usuarios del Servidor
        virtual size_t listarUsuarios(const UsuarioCredencialDTO& cred, FilaUsuario* out, size_t maxItems) = 0;
        virtual bool   actualizarUsuario(const UsuarioCredencialDTO& cred, const ActualizarUsuarioInputDTO& in, FilaUsuario& out) = 0;
    };

    // ---------- Contrato: servicio que EXPONE el backend (firmware lo llama) ----------
    class IBackend {
      public:
        virtual ~IBackend() = default;
        virtual uint16_t sincronizarReloj(const char* motivo) = 0;
    };

}