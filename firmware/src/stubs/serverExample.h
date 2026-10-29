// Archivo: ./src/stubs/serverExample.h
#pragma once
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <esp_system.h>
#include <mbedtls/gcm.h>
#include <esp_timer.h>
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
    // 1. CRIPTOGRAFÍA HARDWARE ESP32: AES-256-GCM (CAPA 6) + CAMPO SENSIBLE (CAPA 7)
    // =========================================================================
    static constexpr size_t GCM_NONCE_LEN = 12;
    static constexpr size_t GCM_TAG_LEN   = 16;
    static constexpr size_t GCM_OVERHEAD  = GCM_NONCE_LEN + GCM_TAG_LEN; // 28 bytes

    // Clave maestra AES-256 (32 bytes = 256 bits) persistida en /db/lts.bin
    static const uint8_t CLAVE_DEFECTO_AES256[32] = {
        'U','N','T','D','F','_','O','S','I','_','A','E','S','2','5','6',
        '_','G','C','M','_','K','E','Y','_','2','0','2','6','!','!','!'
    };

    static const uint8_t AAD_CANAL_OSI[16] = {
        'U','N','T','D','F','-','O','S','I','-','G','C','M','-','V','1'
    };

    inline uint8_t* claveCanalActiva() {
        static uint8_t clave[32] = {0};
        static bool inicializada = false;
        if (!inicializada) {
            memcpy(clave, CLAVE_DEFECTO_AES256, 32);
            inicializada = true;
        }
        return clave;
    }

    static const uint8_t CLAVE_CAMPO_SMTP[12] = {
        0x73, 0x6D, 0x74, 0x70, 0x5F, 0x73, 0x65, 0x63, 0x5F, 0x6B, 0x65, 0x79
    };

    // Hook TX Capa 6: Encripta y firma con AES-256-GCM acelerado por hardware en el ESP32
    // Estructura resultante: [Nonce: 12B][Ciphertext: len B][Tag GCM: 16B]
    inline bool encriptarCanalGCM_TX(int, uint16_t, uint8_t* buf, size_t& len, size_t capMax) {
        if (!buf || len == 0 || (len + GCM_OVERHEAD) > capMax) return false;

        uint8_t nonce[GCM_NONCE_LEN];
        esp_fill_random(nonce, GCM_NONCE_LEN);

        uint8_t cifrado[LimitesPila::MAX_PAYLOAD_APP];
        uint8_t tag[GCM_TAG_LEN];

        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, claveCanalActiva(), 256);
        if (ret == 0) {
            ret = mbedtls_gcm_crypt_and_tag(
                &ctx, MBEDTLS_GCM_ENCRYPT, len,
                nonce, GCM_NONCE_LEN,
                AAD_CANAL_OSI, sizeof(AAD_CANAL_OSI),
                buf, cifrado,
                GCM_TAG_LEN, tag
            );
        }
        mbedtls_gcm_free(&ctx);
        if (ret != 0) return false;

        memcpy(buf, nonce, GCM_NONCE_LEN);
        memcpy(buf + GCM_NONCE_LEN, cifrado, len);
        memcpy(buf + GCM_NONCE_LEN + len, tag, GCM_TAG_LEN);
        len += GCM_OVERHEAD;
        return true;
    }

    // Hook RX Capa 6: Verifica autenticidad (Tag 16B) y desencripta con AES-256-GCM en el ESP32
    inline bool desencriptarCanalGCM_RX(int, uint16_t, uint8_t* buf, size_t& len, size_t capMax) {
        if (!buf || len <= GCM_OVERHEAD || len > capMax) return false;

        size_t lenCifrado = len - GCM_OVERHEAD;
        const uint8_t* nonce   = buf;
        const uint8_t* cifrado = buf + GCM_NONCE_LEN;
        const uint8_t* tag     = buf + GCM_NONCE_LEN + lenCifrado;

        uint8_t claro[LimitesPila::MAX_PAYLOAD_APP];

        mbedtls_gcm_context ctx;
        mbedtls_gcm_init(&ctx);
        int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, claveCanalActiva(), 256);
        if (ret == 0) {
            ret = mbedtls_gcm_auth_decrypt(
                &ctx, lenCifrado,
                nonce, GCM_NONCE_LEN,
                AAD_CANAL_OSI, sizeof(AAD_CANAL_OSI),
                tag, GCM_TAG_LEN,
                cifrado, claro
            );
        }
        mbedtls_gcm_free(&ctx);

        if (ret != 0) {
            DebugOLED::registrarError("L6 GCM AUTH ERR");
            return false;
        }

        memcpy(buf, claro, lenCifrado);
        len = lenCifrado;
        return true;
    }

    inline void desencriptarCampoSensible(const uint8_t* cifrado, size_t len, char* salidaTexto, size_t capSalida) {
        size_t maxCopy = (len < capSalida - 1) ? len : (capSalida - 1);
        for (size_t i = 0; i < maxCopy; i++) {
            salidaTexto[i] = (char)(cifrado[i] ^ CLAVE_CAMPO_SMTP[i % sizeof(CLAVE_CAMPO_SMTP)] ^ (uint8_t)(i * 7));
        }
        salidaTexto[maxCopy] = '\0';
    }

    // Comparación en tiempo constante (previene ataques de timing sobre contraseñas)
    inline bool verificarPinHashUsuario(const FilaUsuario& usr, const char* pinRecibido) {
        if (!pinRecibido || pinRecibido[0] == '\0') return false;
        const char* guardado = reinterpret_cast<const char*>(usr.contrasena);
        size_t lenG = strnlen(guardado, sizeof(usr.contrasena));
        size_t lenR = strnlen(pinRecibido, sizeof(usr.contrasena));
        uint8_t diff = (uint8_t)(lenG ^ lenR);
        for (size_t i = 0; i < sizeof(usr.contrasena); i++) {
            char cG = (i < lenG) ? guardado[i] : 0;
            char cR = (i < lenR) ? pinRecibido[i] : 0;
            diff |= (uint8_t)(cG ^ cR);
        }
        return (lenG > 0) && (diff == 0);
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

    static void reiniciarEnCallback(void*) {
        Serial.println("[RPC] Reinicio remoto solicitado. Reiniciando...");
        Serial.flush();
        DebugOLED::registrarError("REBOOT RPC");
        delay(50);          // margen para que el UART drene
        esp_restart();      // no retorna
    }

    // =========================================================================
    // 2. SEEDING INICIAL EN LA MICROSD (Certificado + 3 Usuarios + Sensor + NTP)
    // =========================================================================
    inline bool inicializarBaseDeDatosORM(ClienteORM& orm) {
        ormActivo() = &orm;
        repositorios().vincularORM(orm);

        if (!orm.inicializar()) {
            DebugOLED::registrarError("SD INIT ERR");
            return false;
        }

        const uint32_t tsBoot = gEstadoSistema.ultimoUnixTimestamp;

        // A. Semilla de Certificado LTS y Clave Privada AES-256 (/db/lts.bin)
        FilaLTS certActual = {};
        if (!orm.lts.obtener(certActual) || certActual.clave_privada[0] == 0) {
            FilaLTS nuevoCert = {};
            strncpy(nuevoCert.nombre_certificado, "CERT_UNTDF_LAB", sizeof(nuevoCert.nombre_certificado) - 1);
            strncpy(nuevoCert.certificado, "CERT-TLS-UNTDF-2026-VALIDO", sizeof(nuevoCert.certificado) - 1);
            memcpy(nuevoCert.clave_privada, CLAVE_DEFECTO_AES256, 32);
            strncpy(nuevoCert.cadena_certificados, "CA-UNTDF-ROOT", sizeof(nuevoCert.cadena_certificados) - 1);
            orm.lts.guardar(nuevoCert, tsBoot);
            certActual = nuevoCert;
        }
        memcpy(claveCanalActiva(), certActual.clave_privada, 32);

        // B. Semilla de los 3 Usuarios requeridos (/db/usuarios.bin): admin, encargado1, encargado2
        if (orm.usuarios.contarTotal() == 0) {
            auto sembrarUsuario = [&](const char* user, const char* pass, uint8_t rol) {
                FilaUsuario u = {};
                strncpy(u.usuario, user, sizeof(u.usuario) - 1);
                strncpy(reinterpret_cast<char*>(u.contrasena), pass, sizeof(u.contrasena) - 1);
                u.rol = rol;
                orm.usuarios.crear(u, tsBoot);
            };
            sembrarUsuario("admin",      "Admin1234",     ROL_ADMIN);
            sembrarUsuario("encargado1", "Encargado1234", ROL_REGULAR);
            sembrarUsuario("encargado2", "Encargado1234", ROL_REGULAR);
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
               strcmp(comando, RPC_DB_GUARDAR_RED) == 0 ||
               strcmp(comando, RPC_REINICIAR_FIRMWARE) == 0;
    }

    inline bool evaluarPoliticaProcesoUsuario(
        const char* comando, size_t cantidadArgs, LectorMsgPack argsCopia,
        const MetaCall& meta, char* codigoRechazo, size_t capCodigo) {

        DebugOLED::registrarRx(meta.idNodo, comando, meta.reqId);

        if (strcmp(comando, RPC_SUMAR_Y_ESTADO) == 0 ||
            strcmp(comando, RPC_DATOS_EN_VIVO) == 0) {
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

        // 3. Verificar Autorización de Rol (DML exige ADMIN; en ACTUALIZAR_USUARIO cada uno puede editarse a sí mismo o el ADMIN a cualquiera)
        if (requiereRolAdmin(comando) && usr.rol != ROL_ADMIN) {
            strncpy(codigoRechazo, "REQUIERE_ROL_ADMIN", capCodigo);
            DebugOLED::registrarError("P:NO_ADMIN");
            return false;
        }

        if (strcmp(comando, RPC_DB_ACTUALIZAR_USUARIO) == 0) {
            ActualizarUsuarioInputDTO in;
            if (!in.deserializar(argsCopia)) {
                strncpy(codigoRechazo, "FORMATO_USUARIO_INVALIDO", capCodigo);
                return false;
            }
            if (usr.rol != ROL_ADMIN && usr.id_usuario != (uint16_t)in.idUsuarioObjetivo) {
                strncpy(codigoRechazo, "SOLO_PUEDE_EDITAR_PROPIO_USR", capCodigo);
                DebugOLED::registrarError("P:USR_FORBID");
                return false;
            }
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

        bool reiniciarFirmware(const UsuarioCredencialDTO& cred) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            if (!sesion.autenticado) return false;

            gEstadoSistema.totalLlamadasRecibidas++;

            // Best-effort: dejar rastro en auditoría antes de apagar
            repositorios().auditorias.registrar(
                sesion.idUsuario, "REINICIO_RPC",
                "Reboot remoto via RPC", gEstadoSistema.ultimoUnixTimestamp, sesion);

            // Programar reinicio asíncrono a 800ms. La respuesta RPC ya salió del handler,
            // así que cuando dispare, el frontend ya recibió ["OK", true].
            esp_timer_handle_t timer = nullptr;
            esp_timer_create_args_t args = {};
            args.callback        = &reiniciarEnCallback;
            args.arg             = nullptr;
            args.dispatch_method = ESP_TIMER_TASK;
            args.name            = "rpc_reboot";

            if (esp_timer_create(&args, &timer) != ESP_OK) return false;
            if (esp_timer_start_once(timer, 800ULL * 1000ULL) != ESP_OK) {
                esp_timer_delete(timer);
                return false;
            }
            return true;
        }

        RespuestaDatosEnVivoDTO leerDatosEnVivo() override {
            gEstadoSistema.totalLlamadasRecibidas++;
            float valorRandom = -20.0f + ((float)(esp_random() % 101) * 0.1f);
            return { valorRandom };
        }

        RespuestaEstado sumarYEstado(float base, float factor) override {
            float resultado = base + (factor * 1.5f);
            gEstadoSistema.ultimoResultadoCalculado = resultado;
            gEstadoSistema.totalLlamadasRecibidas++;

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

        size_t listarUsuarios(const UsuarioCredencialDTO& cred, FilaUsuario* out, size_t maxItems) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            gEstadoSistema.totalLlamadasRecibidas++;
            return repositorios().usuarios.listarActivos(out, maxItems, sesion);
        }

        bool actualizarUsuario(const UsuarioCredencialDTO& cred,
                               const ActualizarUsuarioInputDTO& in,
                               FilaUsuario& out) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            if (!sesion.autenticado) return false;

            char nuevaPassPlana[32] = {0};
            desencriptarCampoSensible(in.nuevaPasswordCifrada, in.lenNuevaPassword,
                                      nuevaPassPlana, sizeof(nuevaPassPlana));

            uint32_t ahora = gEstadoSistema.ultimoUnixTimestamp;
            if (!repositorios().usuarios.actualizarCredenciales(
                    (uint16_t)in.idUsuarioObjetivo,
                    in.nuevoUsername,
                    nuevaPassPlana,
                    ahora,
                    out,
                    sesion)) {
                return false;
            }

            char detalle[24] = {0};
            snprintf(detalle, sizeof(detalle), "U#%u->%s", (unsigned)out.id_usuario, out.usuario);
            repositorios().auditorias.registrar(sesion.idUsuario, "CAMBIO_CREDENCIAL", detalle, ahora, sesion);
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

        appRpc.onStream(RPC_DATOS_EN_VIVO, [](size_t, LectorMsgPack&,
                                              EscritorMsgPack& resp, const MetaCall&) -> bool {
            return servicioFirmware()->leerDatosEnVivo().serializar(resp);
        });

        appRpc.onRequest(RPC_SUMAR_Y_ESTADO, [](size_t cantArgs, LectorMsgPack& args,
                                                EscritorMsgPack& resp, const MetaCall&) -> bool {
            float base = 0.0f, factor = 0.0f;
            if (cantArgs < 2 || !args.leerFloat(base) || !args.leerFloat(factor)) return false;
            return servicioFirmware()->sumarYEstado(base, factor).serializar(resp);
        });

        appRpc.onRequest(RPC_GUARDAR_CONFIG_SEGURA, [](size_t cantArgs, LectorMsgPack& args,
                                                       EscritorMsgPack& resp, const MetaCall& meta) -> bool {
            CredencialRpcDTO auth;
            ConfigHeladeraDTO cfg;
            if (cantArgs < 7 || !auth.deserializar(args) || !cfg.deserializar(args)) return false;

            RespuestaConfigDTO out;
            if (!servicioFirmware()->guardarConfig(auth.usuario, cfg, meta.idNodo, out)) return false;
            return out.serializar(resp);
        });

        appRpc.onRequest(RPC_DB_LISTAR_SENSORES, [](size_t cantArgs, LectorMsgPack& args,
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

        appRpc.onRequest(RPC_DB_GUARDAR_SENSOR, [](size_t cantArgs, LectorMsgPack& args,
                                                   EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            GuardarSensorInputDTO in;
            if (cantArgs < 9 || !auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaSensor guardado = {};
            if (!servicioFirmware()->guardarSensor(auth.usuario, in, guardado)) return false;
            return resp.iniciarArray(2) && resp.escribirTexto("OK") && serializarFilaSensor(resp, guardado);
        });

        appRpc.onRequest(RPC_DB_ULTIMOS_SENSOS, [](size_t cantArgs, LectorMsgPack& args,
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

        appRpc.onRequest(RPC_DB_ULTIMOS_EVENTOS, [](size_t cantArgs, LectorMsgPack& args,
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

        appRpc.onRequest(RPC_DB_ULTIMAS_AUDITORIAS, [](size_t cantArgs, LectorMsgPack& args,
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

        appRpc.onRequest(RPC_DB_LISTAR_CORREOS, [](size_t cantArgs, LectorMsgPack& args,
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

        appRpc.onRequest(RPC_DB_CREAR_CORREO, [](size_t cantArgs, LectorMsgPack& args,
                                                 EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            CrearCorreoInputDTO in;
            if (cantArgs < 8 || !auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaCorreo creado = {};
            if (!servicioFirmware()->crearCorreo(auth.usuario, in, creado)) return false;
            return resp.iniciarArray(2) && resp.escribirTexto("OK") && serializarFilaCorreo(resp, creado);
        });

        appRpc.onRequest(RPC_DB_OBTENER_SMTP, [](size_t cantArgs, LectorMsgPack& args,
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

        appRpc.onRequest(RPC_DB_OBTENER_RED_Y_NTP, [](size_t cantArgs, LectorMsgPack& args,
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

        appRpc.onRequest(RPC_DB_GUARDAR_RED, [](size_t cantArgs, LectorMsgPack& args,
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

        // 11. DQL: Listar los 3 Usuarios del Servidor (sin exponer contraseñas)
        appRpc.onRequest(RPC_DB_LISTAR_USUARIOS, [](size_t cantArgs, LectorMsgPack& args,
                                                    EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            if (cantArgs < 4 || !auth.deserializar(args)) return false;

            FilaUsuario lista[4];
            size_t n = servicioFirmware()->listarUsuarios(auth.usuario, lista, 4);
            if (!resp.iniciarArray(2) || !resp.escribirTexto("OK") || !resp.iniciarArray(n)) return false;
            for (size_t i = 0; i < n; i++) {
                if (!serializarFilaUsuarioResumen(resp, lista[i])) return false;
            }
            return true;
        });

        // 12. DML: Cambiar nombre de usuario y contraseña (propio o por Admin)
        appRpc.onRequest(RPC_DB_ACTUALIZAR_USUARIO, [](size_t cantArgs, LectorMsgPack& args,
                                                       EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            ActualizarUsuarioInputDTO in;
            if (cantArgs < 7 || !auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaUsuario actualizado = {};
            if (!servicioFirmware()->actualizarUsuario(auth.usuario, in, actualizado)) return false;
            return resp.iniciarArray(2) &&
                   resp.escribirTexto("OK") &&
                   serializarFilaUsuarioResumen(resp, actualizado);
        });
        
        appRpc.onRequest(RPC_REINICIAR_FIRMWARE, [](size_t cantArgs, LectorMsgPack& args,
                                            EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            if (cantArgs < 4 || !auth.deserializar(args)) return false;

            bool ok = servicioFirmware()->reiniciarFirmware(auth.usuario);
            return resp.iniciarArray(2) &&
                resp.escribirTexto("OK") &&
                resp.escribirBool(ok);
        });
    }

    inline void registrarStubsServidor(CapaAplicacion& appRpc,
                                       CapaPresentacion& capaPres,
                                       ClienteORM& orm) {
        inicializarBaseDeDatosORM(orm);
        // Conectamos AES-256-GCM acelerado por hardware en TX (encriptar) y RX (desencriptar)
        capaPres.configurarTransformadores(encriptarCanalGCM_TX, desencriptarCanalGCM_RX);
        appRpc.establecerEvaluadorPolitica(evaluarPoliticaProcesoUsuario);
        registrarSkeletonFirmware(appRpc, firmwareImpl);
    }

}