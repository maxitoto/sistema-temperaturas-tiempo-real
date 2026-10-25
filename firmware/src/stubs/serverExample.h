// Archivo: ./src/stubs/serverExample.h
#pragma once
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include "communication/layers/CapaAplicacion.h"
#include "state/EstadoSistema.h"
#include "debug/DebugOLED.h"
#include "stubs/contratos.h"
#include "database/orm/ClienteORM.h"
#include "database/repositories/FirmwareRepositories.h"

namespace ServerExampleStub {

    using namespace Contratos;
    using namespace EsquemaFisicoDB;
    using namespace SGBD;
    using namespace ORM;
    using namespace Repositories;

    inline ClienteORM*& ormActivo() {
        static ClienteORM* ptr = nullptr;
        return ptr;
    }

    inline ContenedorRepositoriosFirmware& repositorios() {
        static ContenedorRepositoriosFirmware inst;
        return inst;
    }

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

    inline bool transformarMensajeCanalTLS(int, uint16_t, uint8_t* buf, size_t& len, size_t) {
        for (size_t i = 0; i < len; i++) {
            buf[i] ^= (uint8_t)(CLAVE_CANAL_TLS[i & 0x0F] ^ (uint8_t)(i * 29));
        }
        return true;
    }

    inline void desencriptarCampoSensible(const uint8_t* cifrado, size_t len, char* salidaTexto, size_t capSalida) {
        size_t maxCopy = (len < capSalida - 1) ? len : (capSalida - 1);
        for (size_t i = 0; i < maxCopy; i++) {
            salidaTexto[i] = (char)(cifrado[i] ^ CLAVE_CAMPO_SMTP[i % sizeof(CLAVE_CAMPO_SMTP)] ^ (uint8_t)(i * 7));
        }
        salidaTexto[maxCopy] = '\0';
    }

    inline bool verificarPinHashUsuario(const FilaUsuario& usr, const char* pinRecibido) {
        if (!pinRecibido || pinRecibido[0] == '\0') return false;
        return strncmp(reinterpret_cast<const char*>(usr.contrasena),
                       pinRecibido,
                       sizeof(usr.contrasena)) == 0;
    }

    inline ContextoSesionSGBD construirSesionDesdeCredencial(const UsuarioCredencialDTO& cred) {
        ClienteORM* orm = ormActivo();
        if (!orm) return { 0, ROL_REGULAR, false };
        FilaUsuario usr = {};
        if (!orm->buscarUsuarioPorNombre(cred.username, usr) || !verificarPinHashUsuario(usr, cred.pinHash)) {
            return { 0, ROL_REGULAR, false };
        }
        return { usr.id_usuario, usr.rol, true };
    }

    // =========================================================================
    // 2. SEEDING INICIAL EN LA MICROSD (Si las tablas están recién creadas)
    // =========================================================================
    inline bool inicializarBaseDeDatosORM(ClienteORM& orm) {
        ormActivo() = &orm;
        repositorios().vincularORM(orm);

        if (!orm.inicializar()) {
            DebugOLED::registrarError("SD INIT ERR");
            return false;
        }

        const uint32_t tsBoot = gEstadoSistema.ultimoUnixTimestamp;

        // A. Semilla de Certificado LTS (/db/lts.bin)
        FilaLTS certActual = {};
        if (!orm.lts.obtener(certActual)) {
            FilaLTS nuevoCert = {};
            strncpy(nuevoCert.nombre_certificado, "CERT_UNTDF_LAB", sizeof(nuevoCert.nombre_certificado) - 1);
            strncpy(nuevoCert.certificado, "CERT-TLS-UNTDF-2026-VALIDO", sizeof(nuevoCert.certificado) - 1);
            strncpy(nuevoCert.cadena_certificados, "CA-UNTDF-ROOT", sizeof(nuevoCert.cadena_certificados) - 1);
            orm.lts.guardar(nuevoCert, tsBoot);
        }

        // B. Semilla de Usuarios (/db/usuarios.bin)
        if (orm.usuarios.contarTotal() == 0) {
            FilaUsuario admin = {};
            strncpy(admin.usuario, "silvana_admin", sizeof(admin.usuario) - 1);
            strncpy(reinterpret_cast<char*>(admin.contrasena), "HASH_ADMIN_2026", sizeof(admin.contrasena) - 1);
            admin.rol = ROL_ADMIN;
            orm.usuarios.crear(admin, tsBoot);

            FilaUsuario oper = {};
            strncpy(oper.usuario, "juan_operador", sizeof(oper.usuario) - 1);
            strncpy(reinterpret_cast<char*>(oper.contrasena), "HASH_OPER_2026", sizeof(oper.contrasena) - 1);
            oper.rol = ROL_REGULAR;
            orm.usuarios.crear(oper, tsBoot);
        }

        // C. Semilla de Sensor por defecto (/db/sensor.bin)
        if (orm.sensor.contarTotal() == 0) {
            FilaSensor sensorInicial = {};
            strncpy(sensorInicial.hexadecimal, "28FF641E82160450", sizeof(sensorInicial.hexadecimal) - 1);
            sensorInicial.error_inferior  = 0.0f;
            sensorInicial.error_superior  = 0.0f;
            sensorInicial.umbral_inferior = -18.0f;
            sensorInicial.umbral_superior = -12.0f;
            orm.sensor.crear(sensorInicial, tsBoot);
        }

        // D. Semilla de Servidor NTP por defecto (/db/ntp.bin)
        if (orm.servidoresNtp.contarTotal() == 0) {
            FilaServidorNTP ntp = {};
            strncpy(ntp.server, "pool.ntp.org", sizeof(ntp.server) - 1);
            orm.servidoresNtp.crear(ntp, tsBoot);
        }

        return true;
    }

    // =========================================================================
    // 3. MIDDLEWARE DE POLÍTICA DE ACCESO (Certificado TLS + Usuario + Rol DCL)
    // =========================================================================
    inline bool requiereRolAdmin(const char* comando) {
        return strcmp(comando, RPC_GUARDAR_CONFIG_SEGURA) == 0 ||
               strcmp(comando, RPC_DB_GUARDAR_SENSOR) == 0 ||
               strcmp(comando, RPC_DB_CREAR_CORREO) == 0 ||
               strcmp(comando, RPC_DB_GUARDAR_RED) == 0;
    }

    inline bool evaluarPoliticaProcesoUsuario(
        const char* comando, size_t cantidadArgs, LectorMsgPack argsCopia,
        const MetaCall& meta, char* codigoRechazo, size_t capCodigo) {

        DebugOLED::registrarRx(meta.idNodo, comando, meta.reqId);

        // RPC_SUMAR_Y_ESTADO es de prueba general; todos los demás exigen CredencialRpcDTO (4 campos iniciales)
        if (strcmp(comando, RPC_SUMAR_Y_ESTADO) == 0) {
            return true;
        }

        ClienteORM* orm = ormActivo();
        if (!orm) {
            strncpy(codigoRechazo, "ORM_SD_NO_DISPONIBLE", capCodigo);
            DebugOLED::registrarError("P:NO_SD");
            return false;
        }

        if (cantidadArgs < 4) {
            strncpy(codigoRechazo, "CREDENCIAL_INCOMPLETA", capCodigo);
            DebugOLED::registrarError("P:CRED_INC");
            return false;
        }

        CredencialRpcDTO auth;
        if (!auth.deserializar(argsCopia)) {
            strncpy(codigoRechazo, "FORMATO_CRED_INVALIDO", capCodigo);
            DebugOLED::registrarError("P:BAD_CRED");
            return false;
        }

        // 1. Verificar Certificado TLS contra /db/lts.bin
        FilaLTS ltsGuardado = {};
        const char* firmaEsperada = orm->lts.obtener(ltsGuardado)
                                      ? ltsGuardado.certificado
                                      : "CERT-TLS-UNTDF-2026-VALIDO";

        if (strcmp(auth.cert.firmaHex, firmaEsperada) != 0) {
            strncpy(codigoRechazo, "CERT_TLS_INVALIDO", capCodigo);
            DebugOLED::registrarError("P:CERT_ERR");
            return false;
        }

        // 2. Autenticar Usuario contra /db/usuarios.bin
        FilaUsuario usr = {};
        if (!orm->buscarUsuarioPorNombre(auth.usuario.username, usr) ||
            !verificarPinHashUsuario(usr, auth.usuario.pinHash)) {
            strncpy(codigoRechazo, "USUARIO_O_PIN_INVALIDO", capCodigo);
            DebugOLED::registrarError("P:USR_ERR");
            return false;
        }

        // 3. Verificar Autorización de Rol (DML exige ADMIN; DQL permite REGULAR y ADMIN)
        if (requiereRolAdmin(comando) && usr.rol != ROL_ADMIN) {
            strncpy(codigoRechazo, "REQUIERE_ROL_ADMIN", capCodigo);
            DebugOLED::registrarError("P:NO_ADMIN");
            return false;
        }

        // 4. Validaciones de Rango de Dominio para cambios de umbrales
        if (strcmp(comando, RPC_GUARDAR_CONFIG_SEGURA) == 0) {
            ConfigHeladeraDTO cfg;
            if (!cfg.deserializar(argsCopia)) {
                strncpy(codigoRechazo, "FORMATO_DTO_INVALIDO", capCodigo);
                return false;
            }
            if (cfg.tempMin < -40.0f || cfg.tempMax > 20.0f || cfg.tempMin >= cfg.tempMax) {
                strncpy(codigoRechazo, "UMBRAL_FUERA_DE_RANGO_SEGURO", capCodigo);
                DebugOLED::registrarError("P:RANGO_ERR");
                return false;
            }
        } else if (strcmp(comando, RPC_DB_GUARDAR_SENSOR) == 0) {
            GuardarSensorInputDTO in;
            if (!in.deserializar(argsCopia)) {
                strncpy(codigoRechazo, "FORMATO_SENSOR_INVALIDO", capCodigo);
                return false;
            }
            if (in.umbralInferior < -40.0f || in.umbralSuperior > 20.0f || in.umbralInferior >= in.umbralSuperior) {
                strncpy(codigoRechazo, "UMBRAL_FUERA_DE_RANGO_SEGURO", capCodigo);
                DebugOLED::registrarError("P:RANGO_ERR");
                return false;
            }
        }

        return true;
    }

    // =========================================================================
    // 4. IMPLEMENTACIÓN REAL DE IFirmware USANDO LOS REPOSITORIOS
    // =========================================================================
    class FirmwareImpl : public IFirmware {
      public:
        RespuestaEstado sumarYEstado(float base, float factor) override {
            float resultado = base + (factor * 1.5f);
            gEstadoSistema.ultimoResultadoCalculado = resultado;
            gEstadoSistema.totalLlamadasRecibidas++;

            repositorios().sensos.registrarLectura(
                1, base, resultado, ESTADO_OK,
                gEstadoSistema.ultimoUnixTimestamp,
                ContextoSesionSGBD::sistemaInterno()
            );

            return {
                resultado,
                gEstadoSistema.relojSincronizado ? "SYNC_OK" : "ESTABLE",
                ESP.getFreeHeap()
            };
        }

        bool guardarConfig(const UsuarioCredencialDTO& cred,
                           const ConfigHeladeraDTO& cfg,
                           int idNodo,
                           RespuestaConfigDTO& out) override {
            ClienteORM* orm = ormActivo();
            if (!orm) return false;

            const uint32_t ahoraSeg = gEstadoSistema.ultimoUnixTimestamp;
            ContextoSesionSGBD sesionUsr = construirSesionDesdeCredencial(cred);
            if (!sesionUsr.autenticado) return false;

            char passPlana[32] = {0};
            desencriptarCampoSensible(cfg.smtpPasswordCifrada, cfg.lenPasswordCifrada,
                                      passPlana, sizeof(passPlana));

            FilaSensor sensorFila = {};
            if (repositorios().sensores.buscarPorId(1, sensorFila, sesionUsr)) {
                repositorios().sensores.guardarSensor(
                    sensorFila.hexadecimal,
                    sensorFila.error_superior,
                    sensorFila.error_inferior,
                    cfg.tempMax,
                    cfg.tempMin,
                    ahoraSeg,
                    sensorFila,
                    sesionUsr
                );
            }

            FilaConfiguracionSMTP smtpFila = {};
            strncpy(smtpFila.host_smtp, "smtp.untdf.edu.ar", sizeof(smtpFila.host_smtp) - 1);
            smtpFila.puerto = 587;
            strncpy(smtpFila.usuario, cred.username, sizeof(smtpFila.usuario) - 1);
            strncpy(smtpFila.password, passPlana, sizeof(smtpFila.password) - 1);
            strncpy(smtpFila.email_remitente, "alertas@untdf.edu.ar", sizeof(smtpFila.email_remitente) - 1);
            if (!repositorios().configuraciones.guardarSmtp(smtpFila, ahoraSeg, sesionUsr)) {
                return false;
            }

            char detalleCambio[24] = {0};
            snprintf(detalleCambio, sizeof(detalleCambio), "%.1f a %.1f (%s)",
                     cfg.tempMin, cfg.tempMax, (idNodo == 0) ? "USB" : "WIFI");

            if (!repositorios().auditorias.registrar(sesionUsr.idUsuario, "CFG_UMBRAL_SMTP", detalleCambio, ahoraSeg, sesionUsr)) {
                return false;
            }

            out.idRegistro = orm->auditoria.obtenerUltimoId();
            out.tempMin    = cfg.tempMin;
            out.tempMax    = cfg.tempMax;
            strncpy(out.smtpPasswordClara, passPlana, sizeof(out.smtpPasswordClara) - 1);
            out.smtpPasswordClara[sizeof(out.smtpPasswordClara) - 1] = '\0';
            strncpy(out.modificadoPor, cred.username, sizeof(out.modificadoPor) - 1);
            out.modificadoPor[sizeof(out.modificadoPor) - 1] = '\0';
            strncpy(out.rolUsuario, (sesionUsr.rol == ROL_ADMIN) ? "ADMIN" : "REGULAR", sizeof(out.rolUsuario) - 1);
            out.rolUsuario[sizeof(out.rolUsuario) - 1] = '\0';
            out.medioUsado = (idNodo == 0) ? "USB_FISICO" : "WIFI_TCP";
            out.totalFilas = (uint32_t)orm->auditoria.recorrerActivos([](const FilaAuditoria&) { return true; });

            gEstadoSistema.totalLlamadasRecibidas++;
            return true;
        }

        size_t listarSensores(const UsuarioCredencialDTO& cred, FilaSensor* out, size_t maxItems) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            gEstadoSistema.totalLlamadasRecibidas++;
            return repositorios().sensores.listarActivos(out, maxItems, sesion);
        }

        bool guardarSensor(const UsuarioCredencialDTO& cred, const GuardarSensorInputDTO& in, FilaSensor& out) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            uint32_t ahora = gEstadoSistema.ultimoUnixTimestamp;
            if (!repositorios().sensores.guardarSensor(
                    in.hexadecimal, in.errorSuperior, in.errorInferior,
                    in.umbralSuperior, in.umbralInferior, ahora, out, sesion)) {
                return false;
            }
            char det[24] = {0};
            snprintf(det, sizeof(det), "S#%u:%.1f..%.1f", (unsigned)out.id_sensor, in.umbralInferior, in.umbralSuperior);
            repositorios().auditorias.registrar(sesion.idUsuario, "UPSERT_SENSOR", det, ahora, sesion);
            gEstadoSistema.totalLlamadasRecibidas++;
            return true;
        }

        size_t listarUltimosSensos(const UsuarioCredencialDTO& cred, uint16_t idSensor, FilaSenso* out, size_t maxItems) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            gEstadoSistema.totalLlamadasRecibidas++;
            return repositorios().sensos.listarUltimos(out, maxItems, idSensor, sesion);
        }

        size_t listarUltimosEventos(const UsuarioCredencialDTO& cred, uint16_t idSensor, FilaEventoSensor* out, size_t maxItems) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            gEstadoSistema.totalLlamadasRecibidas++;
            return repositorios().eventos.listarUltimos(out, maxItems, idSensor, sesion);
        }

        size_t listarUltimasAuditorias(const UsuarioCredencialDTO& cred, FilaAuditoria* out, size_t maxItems) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            gEstadoSistema.totalLlamadasRecibidas++;
            return repositorios().auditorias.listarUltimas(out, maxItems, sesion);
        }

        size_t listarCorreos(const UsuarioCredencialDTO& cred, FilaCorreo* out, size_t maxItems) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            gEstadoSistema.totalLlamadasRecibidas++;
            return repositorios().correos.listarActivos(out, maxItems, sesion);
        }

        bool crearCorreo(const UsuarioCredencialDTO& cred, const CrearCorreoInputDTO& in, FilaCorreo& out) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            uint32_t ahora = gEstadoSistema.ultimoUnixTimestamp;
            if (!repositorios().correos.crearCorreo(in.nombre, in.apellido, in.email, (uint8_t)in.tipoRecepcion, ahora, out, sesion)) {
                return false;
            }
            repositorios().auditorias.registrar(sesion.idUsuario, "ALTA_CORREO", in.email, ahora, sesion);
            gEstadoSistema.totalLlamadasRecibidas++;
            return true;
        }

        bool obtenerConfigSmtp(const UsuarioCredencialDTO& cred, FilaConfiguracionSMTP& out) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            gEstadoSistema.totalLlamadasRecibidas++;
            return repositorios().configuraciones.obtenerSmtp(out, sesion);
        }

        bool obtenerConfigRedYNtp(const UsuarioCredencialDTO& cred, FilaConfiguracionConectividad& outRed,
                                  FilaServidorNTP* outNtp, size_t& totalNtp) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            repositorios().configuraciones.obtenerConectividad(outRed, sesion);
            totalNtp = repositorios().configuraciones.listarServidoresNtp(outNtp, 4, sesion);
            gEstadoSistema.totalLlamadasRecibidas++;
            return true;
        }

        bool guardarConfigRed(const UsuarioCredencialDTO& cred, const GuardarRedInputDTO& in,
                              FilaConfiguracionConectividad& outRed) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            uint32_t ahora = gEstadoSistema.ultimoUnixTimestamp;
            outRed = {};
            strncpy(outRed.SSID_STA, in.ssidSta, sizeof(outRed.SSID_STA) - 1);
            desencriptarCampoSensible(in.passStaCifrada, in.lenPassSta, outRed.STA_password, sizeof(outRed.STA_password));
            strncpy(outRed.SSID_AP, in.ssidAp, sizeof(outRed.SSID_AP) - 1);
            desencriptarCampoSensible(in.passApCifrada, in.lenPassAp, outRed.AP_password, sizeof(outRed.AP_password));

            if (!repositorios().configuraciones.guardarConectividad(outRed, ahora, sesion)) return false;
            repositorios().auditorias.registrar(sesion.idUsuario, "CAMBIO_WIFI", outRed.SSID_STA, ahora, sesion);
            gEstadoSistema.totalLlamadasRecibidas++;
            return true;
        }
    };

    static FirmwareImpl firmwareImpl;

    // =========================================================================
    // 5. SKELETON RPC: Conecta los comandos de red con IFirmware
    // =========================================================================
    inline IFirmware*& servicioFirmware() {
        static IFirmware* p = nullptr;
        return p;
    }

    inline void registrarSkeletonFirmware(CapaAplicacion& appRpc, IFirmware& impl) {
        servicioFirmware() = &impl;

        appRpc.on(RPC_SUMAR_Y_ESTADO, [](size_t cantArgs, LectorMsgPack& args,
                                         EscritorMsgPack& resp, const MetaCall&) -> bool {
            float base = 0.0f, factor = 0.0f;
            if (cantArgs < 2 || !args.leerFloat(base) || !args.leerFloat(factor)) return false;
            return servicioFirmware()->sumarYEstado(base, factor).serializar(resp);
        });

        appRpc.on(RPC_GUARDAR_CONFIG_SEGURA, [](size_t cantArgs, LectorMsgPack& args,
                                                EscritorMsgPack& resp, const MetaCall& meta) -> bool {
            CredencialRpcDTO auth;
            ConfigHeladeraDTO cfg;
            if (cantArgs < 7 || !auth.deserializar(args) || !cfg.deserializar(args)) return false;

            RespuestaConfigDTO out;
            if (!servicioFirmware()->guardarConfig(auth.usuario, cfg, meta.idNodo, out)) return false;
            return out.serializar(resp);
        });

        // 1. DQL: Listar Sensores (Hasta 6 sensores por llamada = ~270B < 492B)
        appRpc.on(RPC_DB_LISTAR_SENSORES, [](size_t cantArgs, LectorMsgPack& args,
                                             EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            if (cantArgs < 4 || !auth.deserializar(args)) return false;

            FilaSensor lista[6];
            size_t n = servicioFirmware()->listarSensores(auth.usuario, lista, 6);
            if (!resp.iniciarArray(2) || !resp.escribirTexto("OK") || !resp.iniciarArray(n)) return false;
            for (size_t i = 0; i < n; i++) {
                if (!serializarFilaSensor(resp, lista[i])) return false;
            }
            return true;
        });

        // 2. DML: Crear o Actualizar Sensor
        appRpc.on(RPC_DB_GUARDAR_SENSOR, [](size_t cantArgs, LectorMsgPack& args,
                                            EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            GuardarSensorInputDTO in;
            if (cantArgs < 9 || !auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaSensor guardado = {};
            if (!servicioFirmware()->guardarSensor(auth.usuario, in, guardado)) return false;
            return resp.iniciarArray(2) && resp.escribirTexto("OK") && serializarFilaSensor(resp, guardado);
        });

        // 3. DQL: Listar Últimos Sensos (Mediciones)
        appRpc.on(RPC_DB_ULTIMOS_SENSOS, [](size_t cantArgs, LectorMsgPack& args,
                                            EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            uint32_t idSensor = 0, limite = 8;
            if (cantArgs < 6 || !auth.deserializar(args) || !args.leerUInt32(idSensor) || !args.leerUInt32(limite)) {
                return false;
            }
            if (limite > 8) limite = 8;

            FilaSenso lista[8];
            size_t n = servicioFirmware()->listarUltimosSensos(auth.usuario, (uint16_t)idSensor, lista, limite);
            if (!resp.iniciarArray(2) || !resp.escribirTexto("OK") || !resp.iniciarArray(n)) return false;
            for (size_t i = 0; i < n; i++) {
                if (!serializarFilaSenso(resp, lista[i])) return false;
            }
            return true;
        });

        // 4. DQL: Listar Últimos Eventos de Sensor
        appRpc.on(RPC_DB_ULTIMOS_EVENTOS, [](size_t cantArgs, LectorMsgPack& args,
                                             EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            uint32_t idSensor = 0, limite = 8;
            if (cantArgs < 6 || !auth.deserializar(args) || !args.leerUInt32(idSensor) || !args.leerUInt32(limite)) {
                return false;
            }
            if (limite > 8) limite = 8;

            FilaEventoSensor lista[8];
            size_t n = servicioFirmware()->listarUltimosEventos(auth.usuario, (uint16_t)idSensor, lista, limite);
            if (!resp.iniciarArray(2) || !resp.escribirTexto("OK") || !resp.iniciarArray(n)) return false;
            for (size_t i = 0; i < n; i++) {
                if (!serializarFilaEvento(resp, lista[i])) return false;
            }
            return true;
        });

        // 5. DQL: Listar Últimas Auditorías del Firmware
        appRpc.on(RPC_DB_ULTIMAS_AUDITORIAS, [](size_t cantArgs, LectorMsgPack& args,
                                                EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            uint32_t limite = 5;
            if (cantArgs < 5 || !auth.deserializar(args) || !args.leerUInt32(limite)) return false;
            if (limite > 5) limite = 5;

            FilaAuditoria lista[5];
            size_t n = servicioFirmware()->listarUltimasAuditorias(auth.usuario, lista, limite);
            if (!resp.iniciarArray(2) || !resp.escribirTexto("OK") || !resp.iniciarArray(n)) return false;
            for (size_t i = 0; i < n; i++) {
                if (!serializarFilaAuditoria(resp, lista[i])) return false;
            }
            return true;
        });

        // 6. DQL: Listar Destinatarios de Correo
        appRpc.on(RPC_DB_LISTAR_CORREOS, [](size_t cantArgs, LectorMsgPack& args,
                                            EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            if (cantArgs < 4 || !auth.deserializar(args)) return false;

            FilaCorreo lista[4];
            size_t n = servicioFirmware()->listarCorreos(auth.usuario, lista, 4);
            if (!resp.iniciarArray(2) || !resp.escribirTexto("OK") || !resp.iniciarArray(n)) return false;
            for (size_t i = 0; i < n; i++) {
                if (!serializarFilaCorreo(resp, lista[i])) return false;
            }
            return true;
        });

        // 7. DML: Crear Destinatario de Correo
        appRpc.on(RPC_DB_CREAR_CORREO, [](size_t cantArgs, LectorMsgPack& args,
                                          EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            CrearCorreoInputDTO in;
            if (cantArgs < 8 || !auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaCorreo creado = {};
            if (!servicioFirmware()->crearCorreo(auth.usuario, in, creado)) return false;
            return resp.iniciarArray(2) && resp.escribirTexto("OK") && serializarFilaCorreo(resp, creado);
        });

        // 8. DQL: Obtener Configuración SMTP (Singleton PK = 1)
        appRpc.on(RPC_DB_OBTENER_SMTP, [](size_t cantArgs, LectorMsgPack& args,
                                          EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            if (cantArgs < 4 || !auth.deserializar(args)) return false;

            FilaConfiguracionSMTP smtp = {};
            bool existe = servicioFirmware()->obtenerConfigSmtp(auth.usuario, smtp);
            return resp.iniciarArray(7) &&
                   resp.escribirTexto("OK") &&
                   resp.escribirBool(existe) &&
                   resp.escribirTexto(smtp.host_smtp) &&
                   resp.escribirUInt(smtp.puerto) &&
                   resp.escribirTexto(smtp.usuario) &&
                   resp.escribirTexto(smtp.email_remitente) &&
                   resp.escribirUInt(smtp.fecha_modificacion);
        });

        // 9. DQL: Obtener Configuración de Conectividad WiFi + Lista de Servidores NTP
        appRpc.on(RPC_DB_OBTENER_RED_Y_NTP, [](size_t cantArgs, LectorMsgPack& args,
                                               EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            if (cantArgs < 4 || !auth.deserializar(args)) return false;

            FilaConfiguracionConectividad red = {};
            FilaServidorNTP ntp[4];
            size_t totalNtp = 0;
            servicioFirmware()->obtenerConfigRedYNtp(auth.usuario, red, ntp, totalNtp);

            if (!resp.iniciarArray(5) ||
                !resp.escribirTexto("OK") ||
                !resp.escribirTexto(red.SSID_STA) ||
                !resp.escribirTexto(red.SSID_AP) ||
                !resp.escribirUInt(red.fecha_modificacion) ||
                !resp.iniciarArray(totalNtp)) {
                return false;
            }
            for (size_t i = 0; i < totalNtp; i++) {
                if (!resp.escribirTexto(ntp[i].server)) return false;
            }
            return true;
        });

        // 10. DML: Guardar Configuración de Red WiFi (STA y AP con contraseñas cifradas)
        appRpc.on(RPC_DB_GUARDAR_RED, [](size_t cantArgs, LectorMsgPack& args,
                                          EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            GuardarRedInputDTO in;
            if (cantArgs < 8 || !auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaConfiguracionConectividad red = {};
            if (!servicioFirmware()->guardarConfigRed(auth.usuario, in, red)) return false;
            return resp.iniciarArray(4) &&
                   resp.escribirTexto("OK") &&
                   resp.escribirTexto(red.SSID_STA) &&
                   resp.escribirTexto(red.SSID_AP) &&
                   resp.escribirUInt(red.fecha_modificacion);
        });
    }

    inline void registrarStubsServidor(CapaAplicacion& appRpc,
                                       CapaPresentacion& capaPres,
                                       ClienteORM& orm) {
        inicializarBaseDeDatosORM(orm);
        capaPres.configurarTransformadores(transformarMensajeCanalTLS, transformarMensajeCanalTLS);
        appRpc.establecerEvaluadorPolitica(evaluarPoliticaProcesoUsuario);
        registrarSkeletonFirmware(appRpc, firmwareImpl);
    }

}