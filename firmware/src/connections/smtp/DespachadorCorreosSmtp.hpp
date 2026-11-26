#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ClienteSmtpTls.hpp"
#include "DiagnosticoEventosCorreo.hpp"
#include "../../database/repositories/repoConfiguracionSistema.hpp"
#include "../../database/repositories/repoConfiguracionSmtp.hpp"
#include "../../database/repositories/repoCorreos.hpp"
#include "../../database/repositories/repoAuditoriaSensores.hpp"
#include "../../database/repositories/repoAuditoriaSistema.hpp"
#include "../../database/repositories/repoAuditoriaAuth.hpp"
#include "../../database/repositories/repoSensor.hpp"
#include "../../database/repositories/repoCenso.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "../../rtos/RelojSistema.hpp"

namespace SmtpNet {

    struct ResumenDespachoSmtp {
        uint16_t codigoSmtp          = 0;
        bool     exito               = false;
        uint8_t  correosEnviados     = 0;
        uint16_t eventosActualizados = 0;
        char     detalle[128]        = "";
    };

    class DespachadorCorreosSmtp {
    private:
        inline static uint32_t ultimoEnvioExitoMs      = 0;
        inline static uint32_t ultimoEnvioCriticoMs    = 0;
        inline static uint32_t ultimoFalloSmtpMs       = 0;
        inline static uint64_t ultimoSeqAuthNotificado = 0;
        // Buffer HTML estático exclusivo de Core 1 (TaskAlertasDB) — 0 stack en BearSSL
        inline static char cuerpoBuf[10240]{};

        static void copiarTexto(char* dst, size_t cap, const char* src) {
            if (!dst || cap == 0) return;
            if (!src) { dst[0] = '\0'; return; }
            strncpy(dst, src, cap - 1);
            dst[cap - 1] = '\0';
        }

        static void anexarTexto(char* dst, size_t cap, size_t& pos, const char* src) {
            if (!dst || !src || pos + 1 >= cap) return;
            const size_t libre = cap - 1 - pos;
            const size_t n = strnlen(src, libre);
            memcpy(dst + pos, src, n);
            pos += n;
            dst[pos] = '\0';
        }

        // Formatea directo en cuerpoBuf sin buffers intermedios pequeños (cero truncamiento)
        static void anexarFmt(char* dst, size_t cap, size_t& pos, const char* fmt, ...) {
            if (!dst || !fmt || pos + 1 >= cap) return;
            const size_t libre = cap - pos;
            va_list args;
            va_start(args, fmt);
            const int escritos = vsnprintf(dst + pos, libre, fmt, args);
            va_end(args);
            if (escritos <= 0) return;
            if (static_cast<size_t>(escritos) >= libre) {
                pos = cap - 1;
                dst[pos] = '\0';
            } else {
                pos += static_cast<size_t>(escritos);
            }
        }

        static void anexarHtmlEscapado(char* dst, size_t cap, size_t& pos, const char* src) {
            if (!dst || !src) return;
            for (const char* p = src; *p && pos + 8 < cap; ++p) {
                switch (*p) {
                    case '&': anexarTexto(dst, cap, pos, "&amp;");  break;
                    case '<': anexarTexto(dst, cap, pos, "&lt;");   break;
                    case '>': anexarTexto(dst, cap, pos, "&gt;");   break;
                    case '"': anexarTexto(dst, cap, pos, "&quot;"); break;
                    default:
                        dst[pos++] = *p;
                        dst[pos] = '\0';
                        break;
                }
            }
        }

        static bool destinatarioAceptaCriticidad(TipoRecepcionCorreo filtro, CriticidadEvento crit) {
            if (filtro == TipoRecepcionCorreo::RECIBIR_TODOS) return true;
            if (filtro == TipoRecepcionCorreo::RECIBIR_EVENTOS) {
                return crit != CriticidadEvento::NULA;
            }
            return crit == CriticidadEvento::ALTA;
        }

        static CriticidadEvento criticidadMinimaGlobal(
            const std::vector<DbRuntime::Repositories::CorreoDTO>& dests
        ) {
            CriticidadEvento minC = CriticidadEvento::ALTA;
            for (const auto& d : dests) {
                if (d.tipoRecepcionMensajes == TipoRecepcionCorreo::RECIBIR_TODOS) {
                    return CriticidadEvento::NULA;
                }
                if (d.tipoRecepcionMensajes == TipoRecepcionCorreo::RECIBIR_EVENTOS &&
                    static_cast<uint8_t>(CriticidadEvento::BAJA) < static_cast<uint8_t>(minC)) {
                    minC = CriticidadEvento::BAJA;
                }
            }
            return minC;
        }

        // Paleta de alto contraste (legible en Gmail, Outlook Web/Desktop, modo claro y oscuro)
        static const char* colorAcentoPorCriticidad(CriticidadEvento c) {
            if (c == CriticidadEvento::ALTA)  return "#B91C1C"; // Rojo intenso
            if (c == CriticidadEvento::MEDIA) return "#B45309"; // Ámbar oscuro
            return "#1D4ED8";                                   // Azul fuerte
        }

        static ConfigSmtpEnvio armarConfig(const DbRuntime::Repositories::ConfiguracionSmtpDTO& dto) {
            ConfigSmtpEnvio c{};
            c.host            = dto.hostSmtp.c_str();
            c.puerto          = static_cast<uint16_t>(dto.puerto);
            c.usarTls         = dto.usarTls;
            c.usuario         = dto.usuario.c_str();
            c.password        = dto.passwordCifrado.c_str();
            c.emailRemitente  = dto.emailRemitente.c_str();
            c.nombreRemitente = dto.nombreRemitente.c_str();
            return c;
        }

        static void anexarCabeceraSeccion(char* dst, size_t cap, size_t& pos, const char* titulo) {
            anexarFmt(dst, cap, pos,
                "<tr><td style=\"padding:16px 0 8px 0;\">\n"
                "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" bgcolor=\"#1E293B\" style=\"background-color:#1E293B;\">\n"
                "<tr><td style=\"padding:8px 12px;font-family:Arial,Helvetica,sans-serif;font-size:12px;font-weight:bold;color:#FFFFFF;letter-spacing:0.5px;text-transform:uppercase;\">%s</td></tr>\n"
                "</table>\n"
                "</td></tr>\n",
                titulo);
        }

    public:
        // Prueba directa de conectividad SMTP (plantilla en tablas de alto contraste)
        static ResumenDespachoSmtp enviarCorreoDePrueba(const char* destinatarioOpcional = nullptr) {
            using namespace DbRuntime::Repositories;
            ResumenDespachoSmtp out{};

            ConfiguracionSmtpDTO smtpDto{};
            if (!RepoConfiguracionSmtp::obtenerOInicializar(smtpDto)) {
                copiarTexto(out.detalle, sizeof(out.detalle), "No se pudo leer ConfiguracionSmtp");
                return out;
            }
            if (smtpDto.hostSmtp.vacio() || smtpDto.passwordCifrado.vacio()) {
                copiarTexto(out.detalle, sizeof(out.detalle),
                            "Complete Host SMTP y App Password y guarde antes de probar");
                return out;
            }

            char destFinal[48]{};
            if (destinatarioOpcional && destinatarioOpcional[0] != '\0') {
                copiarTexto(destFinal, sizeof(destFinal), destinatarioOpcional);
            } else {
                auto activos = RepoCorreos::listarPaginados(1, 0, true);
                if (!activos.empty()) {
                    copiarTexto(destFinal, sizeof(destFinal), activos[0].correoElectronico.c_str());
                } else if (!smtpDto.emailRemitente.vacio()) {
                    copiarTexto(destFinal, sizeof(destFinal), smtpDto.emailRemitente.c_str());
                } else {
                    copiarTexto(destFinal, sizeof(destFinal), smtpDto.usuario.c_str());
                }
            }

            if (destFinal[0] == '\0') {
                copiarTexto(out.detalle, sizeof(out.detalle),
                            "Indique un correo destino o agregue un destinatario activo");
                return out;
            }

            const auto ahoraStr = RTOS::RelojSistema::fechaHoraActualPg();
            const uint32_t sdLibreKb = TarjetaSD::getInstance().estaInicializada()
                ? TarjetaSD::getInstance().espacioLibreKB() : 0;

            size_t pos = 0;
            cuerpoBuf[0] = '\0';
            anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                "<!DOCTYPE html>\n"
                "<html lang=\"es\"><head><meta charset=\"UTF-8\">\n"
                "<meta name=\"color-scheme\" content=\"light\">\n"
                "<meta name=\"supported-color-schemes\" content=\"light\">\n"
                "</head>\n"
                "<body bgcolor=\"#E5E7EB\" style=\"margin:0;padding:16px;background-color:#E5E7EB;font-family:Arial,Helvetica,sans-serif;color:#111827;\">\n"
                "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" bgcolor=\"#E5E7EB\">\n"
                "<tr><td align=\"center\">\n"
                "<table width=\"600\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" bgcolor=\"#FFFFFF\" style=\"max-width:600px;width:100%%;background-color:#FFFFFF;border:1px solid #CBD5E1;\">\n"
                "<tr><td bgcolor=\"#047857\" style=\"background-color:#047857;padding:18px 20px;color:#FFFFFF;\">\n"
                "<div style=\"font-size:11px;font-weight:bold;letter-spacing:0.8px;text-transform:uppercase;color:#D1FAE5;\">UNTDF &middot; Monitoreo Termico Industrial ESP32</div>\n"
                "<div style=\"font-size:20px;font-weight:bold;margin-top:6px;color:#FFFFFF;\">Prueba de Correo SMTP Exitosa</div>\n"
                "</td></tr>\n"
                "<tr><td style=\"padding:18px 20px;font-size:14px;line-height:1.5;color:#111827;\">\n"
                "<p style=\"margin:0 0 14px 0;color:#111827;\">El nodo ESP32 establecio conexion segura con el servidor SMTP y enviara alertas autonomamente ante incidentes.</p>\n"
                "<table width=\"100%%\" cellpadding=\"8\" cellspacing=\"0\" border=\"0\" bgcolor=\"#F8FAFC\" style=\"background-color:#F8FAFC;border:1px solid #CBD5E1;font-size:13px;color:#111827;\">\n"
                "<tr><td width=\"150\" style=\"font-weight:bold;color:#374151;border-bottom:1px solid #E2E8F0;\">Fecha / Hora</td><td style=\"color:#111827;border-bottom:1px solid #E2E8F0;\">%s</td></tr>\n"
                "<tr><td style=\"font-weight:bold;color:#374151;border-bottom:1px solid #E2E8F0;\">IP LAN / WiFi</td><td style=\"color:#111827;border-bottom:1px solid #E2E8F0;\">%s (%d dBm)</td></tr>\n"
                "<tr><td style=\"font-weight:bold;color:#374151;border-bottom:1px solid #E2E8F0;\">Servidor SMTP</td><td style=\"color:#111827;border-bottom:1px solid #E2E8F0;\">%s:%u (TLS %s)</td></tr>\n"
                "<tr><td style=\"font-weight:bold;color:#374151;border-bottom:1px solid #E2E8F0;\">Heap / MicroSD</td><td style=\"color:#111827;border-bottom:1px solid #E2E8F0;\">%u B libres &middot; %lu KB libres</td></tr>\n"
                "<tr><td style=\"font-weight:bold;color:#374151;\">Uptime</td><td style=\"color:#111827;\">%lu s</td></tr>\n"
                "</table>\n"
                "</td></tr></table>\n"
                "</td></tr></table></body></html>\n",
                ahoraStr.c_str(),
                WiFi.localIP().toString().c_str(),
                static_cast<int>(WiFi.RSSI()),
                smtpDto.hostSmtp.c_str(),
                static_cast<unsigned>(smtpDto.puerto),
                smtpDto.usarTls ? "ON" : "OFF",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned long>(sdLibreKb),
                static_cast<unsigned long>(millis() / 1000UL)
            );

            const char* listaDest[1] = { destFinal };
            bool aceptado[1] = { false };

            const ResultadoSmtp r = ClienteSmtpTls::enviar(
                armarConfig(smtpDto),
                listaDest,
                1,
                "[UNTDF ESP32] Prueba de conectividad SMTP OK",
                cuerpoBuf,
                aceptado,
                true
            );

            out.codigoSmtp      = r.codigoSmtp;
            out.exito           = r.exito;
            out.correosEnviados = r.destinatariosAceptados;
            copiarTexto(out.detalle, sizeof(out.detalle), r.detalle);

            CorreoDTO cDb{};
            if (RepoCorreos::obtenerPorEmail(destFinal, cDb)) {
                RepoCorreos::registrarEstadoEnvioPorEmail(
                    destFinal,
                    r.exito ? EstadoEnvioCorreo::ENVIADO : EstadoEnvioCorreo::FALLIDO
                );
            }
            return out;
        }

        // Despacho autónomo de eventos PENDIENTE y reintento de FALLIDO (HTML en tablas Outlook/Gmail)
        static ResumenDespachoSmtp despacharPendientes(bool ignorarCooldown = false) {
            using namespace DbRuntime::Repositories;
            ResumenDespachoSmtp out{};

            const uint32_t ahoraMs = millis();

            ConfiguracionSmtpDTO smtpDto{};
            if (!RepoConfiguracionSmtp::obtenerOInicializar(smtpDto) ||
                !smtpDto.activa ||
                smtpDto.hostSmtp.vacio() ||
                smtpDto.passwordCifrado.vacio()) {
                copiarTexto(out.detalle, sizeof(out.detalle), "SMTP inactivo o sin credenciales configuradas");
                return out;
            }

            ConfiguracionSistemaDTO cfgSis{};
            RepoConfiguracionSistema::obtenerOInicializar(cfgSis);

            static char destBuf[12][48]{};
            const char* listaDest[12]{};
            size_t      cantDest    = 0;
            int32_t     idsAudSens[10]{};
            size_t      cantAudSens = 0;
            int32_t     idsAudSis[8]{};
            size_t      cantAudSis  = 0;
            uint64_t    maxSeqAuthEnCorreo    = ultimoSeqAuthNotificado;
            bool        hayEventoCriticoNuevo = false;
            bool        hayPendienteNuevo     = false;
            CriticidadEvento critMaxGlobal    = CriticidadEvento::BAJA;
            char        asunto[120]{};

            {
                auto destinatariosDb = RepoCorreos::listarPaginados(12, 0, true);
                if (destinatariosDb.empty()) {
                    copiarTexto(out.detalle, sizeof(out.detalle), "No hay destinatarios activos en Correos");
                    return out;
                }

                const CriticidadEvento critMin = criticidadMinimaGlobal(destinatariosDb);
                const bool permitirReintentoFallidos =
                    ignorarCooldown || ultimoFalloSmtpMs == 0 || (ahoraMs - ultimoFalloSmtpMs) >= 60000UL;

                auto pendSensores = RepoAuditoriaSensores::listarNotificables(10, critMin, permitirReintentoFallidos);
                auto pendSistema  = RepoAuditoriaSistema::listarNotificables(8, critMin, permitirReintentoFallidos);

                struct AuthContextItem {
                    uint64_t seq;
                    char usuario[20];
                    char accion[28];
                    char medio[12];
                    char origen[20];
                    char fecha[20];
                    CriticidadEvento crit;
                };
                AuthContextItem authRecientes[3]{};
                size_t cantAuthRecientes = 0;
                bool hayAuthCriticoNuevo = false;

                RepoAuditoriaAuth::forEachReciente(16, 0, [&](const DbRuntime::Storage::PgRowView& r) -> bool {
                    const auto c = r.getEnum<CriticidadEvento>("criticidad");
                    if (static_cast<uint8_t>(c) < static_cast<uint8_t>(CriticidadEvento::MEDIA)) return true;
                    const uint64_t seq = r.getUInt64("seq");
                    if (cantAuthRecientes < 3) {
                        AuthContextItem& it = authRecientes[cantAuthRecientes++];
                        it.seq  = seq;
                        it.crit = c;
                        copiarTexto(it.usuario, sizeof(it.usuario), r.getCStr("usuario_intentado", "ANONIMO"));
                        copiarTexto(it.accion,  sizeof(it.accion),  r.getCStr("accion", "EVENTO"));
                        copiarTexto(it.medio,   sizeof(it.medio),   DbCommon::Dto::PgEnumTraits<MedioConexion>::toString(r.getEnum<MedioConexion>("medio_conexion")));
                        copiarTexto(it.origen,  sizeof(it.origen),  r.getCStr("origen_remoto", "LOCAL"));
                        const uint32_t ts = static_cast<uint32_t>(r.getInt64("fecha_creacion"));
                        copiarTexto(it.fecha,   sizeof(it.fecha),   RTOS::RelojSistema::desdeTimestampUnix(ts).c_str());
                    }
                    if (c == CriticidadEvento::ALTA && seq > ultimoSeqAuthNotificado) {
                        hayAuthCriticoNuevo = true;
                        if (seq > maxSeqAuthEnCorreo) maxSeqAuthEnCorreo = seq;
                    }
                    return cantAuthRecientes < 3;
                });

                if (pendSensores.empty() && pendSistema.empty() && !hayAuthCriticoNuevo) {
                    out.exito = true;
                    copiarTexto(out.detalle, sizeof(out.detalle), "Sin avisos pendientes o fallidos para enviar");
                    return out;
                }

                for (const auto& ev : pendSensores) {
                    if (static_cast<uint8_t>(ev.criticidadSensor) > static_cast<uint8_t>(critMaxGlobal)) {
                        critMaxGlobal = ev.criticidadSensor;
                    }
                    if (ev.estadoReporte == EstadoEnvioCorreo::PENDIENTE) {
                        hayPendienteNuevo = true;
                        if (ev.criticidadSensor == CriticidadEvento::ALTA) hayEventoCriticoNuevo = true;
                    }
                }
                for (const auto& ev : pendSistema) {
                    if (static_cast<uint8_t>(ev.criticidad) > static_cast<uint8_t>(critMaxGlobal)) {
                        critMaxGlobal = ev.criticidad;
                    }
                    if (ev.estadoReporte == EstadoEnvioCorreo::PENDIENTE) {
                        hayPendienteNuevo = true;
                        if (ev.criticidad == CriticidadEvento::ALTA) hayEventoCriticoNuevo = true;
                    }
                }
                if (hayAuthCriticoNuevo) {
                    critMaxGlobal = CriticidadEvento::ALTA;
                    hayEventoCriticoNuevo = true;
                    hayPendienteNuevo = true;
                }

                if (!ignorarCooldown) {
                    if (!hayPendienteNuevo && ultimoFalloSmtpMs != 0 && (ahoraMs - ultimoFalloSmtpMs) < 60000UL) {
                        copiarTexto(out.detalle, sizeof(out.detalle), "Esperando reintento de envios fallidos (backoff 60s)");
                        return out;
                    }
                    if (hayEventoCriticoNuevo) {
                        if (ultimoEnvioCriticoMs != 0 && (ahoraMs - ultimoEnvioCriticoMs) < 30000UL) {
                            copiarTexto(out.detalle, sizeof(out.detalle), "Agrupando alertas criticas (anti-flood 30s)");
                            return out;
                        }
                    } else if (ultimoEnvioExitoMs != 0 &&
                               cfgSis.correoCooldownS > 0 &&
                               (ahoraMs - ultimoEnvioExitoMs) < (cfgSis.correoCooldownS * 1000UL)) {
                        copiarTexto(out.detalle, sizeof(out.detalle), "En ventana de cooldown de correos");
                        return out;
                    }
                }

                bool destElegido[12]{};
                for (const auto& ev : pendSensores) {
                    for (size_t d = 0; d < destinatariosDb.size() && d < 12; ++d) {
                        if (destinatarioAceptaCriticidad(destinatariosDb[d].tipoRecepcionMensajes, ev.criticidadSensor)) {
                            destElegido[d] = true;
                        }
                    }
                }
                for (const auto& ev : pendSistema) {
                    for (size_t d = 0; d < destinatariosDb.size() && d < 12; ++d) {
                        if (destinatarioAceptaCriticidad(destinatariosDb[d].tipoRecepcionMensajes, ev.criticidad)) {
                            destElegido[d] = true;
                        }
                    }
                }
                if (hayAuthCriticoNuevo) {
                    for (size_t d = 0; d < destinatariosDb.size() && d < 12; ++d) {
                        destElegido[d] = true;
                    }
                }

                for (size_t d = 0; d < destinatariosDb.size() && d < 12; ++d) {
                    if (destElegido[d]) {
                        copiarTexto(destBuf[cantDest], sizeof(destBuf[cantDest]), destinatariosDb[d].correoElectronico.c_str());
                        listaDest[cantDest] = destBuf[cantDest];
                        cantDest++;
                    }
                }
                if (cantDest == 0) {
                    out.exito = true;
                    copiarTexto(out.detalle, sizeof(out.detalle), "Sin destinatarios elegibles para estos eventos");
                    return out;
                }

                // =============================================================
                // CONSTRUCCIÓN HTML EN TABLAS (ALTO CONTRASTE GMAIL / OUTLOOK)
                // =============================================================
                size_t pos = 0;
                cuerpoBuf[0] = '\0';

                const char* hdrColor  = colorAcentoPorCriticidad(critMaxGlobal);
                const char* tituloSev = (critMaxGlobal == CriticidadEvento::ALTA)
                    ? "ALERTA CRITICA DETECTADA"
                    : (critMaxGlobal == CriticidadEvento::MEDIA)
                        ? "ADVERTENCIA DEL SISTEMA"
                        : "REPORTE INFORMATIVO";
                const uint32_t sdLibreKb = TarjetaSD::getInstance().estaInicializada()
                    ? TarjetaSD::getInstance().espacioLibreKB() : 0;

                anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                    "<!DOCTYPE html>\n"
                    "<html lang=\"es\"><head><meta charset=\"UTF-8\">\n"
                    "<meta name=\"color-scheme\" content=\"light\">\n"
                    "<meta name=\"supported-color-schemes\" content=\"light\">\n"
                    "</head>\n"
                    "<body bgcolor=\"#E5E7EB\" style=\"margin:0;padding:12px;background-color:#E5E7EB;font-family:Arial,Helvetica,sans-serif;color:#111827;\">\n"
                    "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" bgcolor=\"#E5E7EB\">\n"
                    "<tr><td align=\"center\">\n"
                    "<table width=\"620\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" bgcolor=\"#FFFFFF\" style=\"max-width:620px;width:100%%;background-color:#FFFFFF;border:1px solid #CBD5E1;\">\n"
                    "<tr><td bgcolor=\"%s\" style=\"background-color:%s;padding:18px 20px;color:#FFFFFF;\">\n"
                    "<div style=\"font-size:11px;font-weight:bold;letter-spacing:0.8px;text-transform:uppercase;color:#F8FAFC;\">UNTDF &middot; Control Termico Industrial ESP32</div>\n"
                    "<div style=\"font-size:20px;font-weight:bold;margin-top:6px;color:#FFFFFF;\">%s</div>\n"
                    "<div style=\"font-size:12px;margin-top:6px;color:#F1F5F9;\">%s &nbsp;|&nbsp; IP %s &nbsp;|&nbsp; WiFi %d dBm</div>\n"
                    "</td></tr>\n"
                    "<tr><td style=\"padding:10px 18px 18px 18px;\">\n"
                    "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\">\n",
                    hdrColor, hdrColor,
                    tituloSev,
                    RTOS::RelojSistema::fechaHoraActualPg().c_str(),
                    WiFi.localIP().toString().c_str(),
                    static_cast<int>(WiFi.RSSI())
                );

                // --- BLOQUE 1: EVENTOS DE SENSORES DS18B20 ---
                if (!pendSensores.empty()) {
                    anexarCabeceraSeccion(cuerpoBuf, sizeof(cuerpoBuf), pos, "1. Eventos de Sensores DS18B20");

                    for (const auto& ev : pendSensores) {
                        if (cantAudSens >= 10) break;
                        idsAudSens[cantAudSens++] = ev.idAuditoriaSensor;
                        if (pos > sizeof(cuerpoBuf) - 1800) continue;

                        SensorDTO sInfo{};
                        const bool tieneS = RepoSensor::obtenerPorId(ev.idSensor, sInfo);
                        CensoDTO cInfo{};
                        const bool tieneC = (ev.idCenso > 0) && RepoCenso::obtenerPorId(ev.idCenso, cInfo);
                        const DescripcionDiagnostico diag = describirEstadoSensor(ev.estadoNuevoSensor);
                        const char* accent = colorAcentoPorCriticidad(ev.criticidadSensor);

                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<tr><td style=\"padding-bottom:12px;\">\n"
                            "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" bgcolor=\"#FFFFFF\" style=\"background-color:#FFFFFF;border:1px solid #CBD5E1;border-left:5px solid %s;\">\n"
                            "<tr><td bgcolor=\"#F8FAFC\" style=\"background-color:#F8FAFC;padding:9px 12px;border-bottom:1px solid #E2E8F0;\">\n"
                            "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\"><tr>\n"
                            "<td style=\"font-size:14px;font-weight:bold;color:#111827;\">",
                            accent);

                        anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            (tieneS && !sInfo.nombre.vacio()) ? sInfo.nombre.c_str() : "Sensor DS18B20");

                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            " <span style=\"font-size:12px;font-weight:normal;color:#374151;\">(#%ld &middot; %s)</span></td>\n"
                            "<td align=\"right\" style=\"white-space:nowrap;\">"
                            "<span style=\"display:inline-block;background-color:%s;color:#FFFFFF;font-size:11px;font-weight:bold;padding:3px 8px;\">%s%s</span>"
                            "</td></tr></table>\n"
                            "</td></tr>\n"
                            "<tr><td style=\"padding:8px 12px;\">\n"
                            "<table width=\"100%%\" cellpadding=\"4\" cellspacing=\"0\" border=\"0\" style=\"font-size:13px;line-height:1.45;color:#111827;\">\n",
                            static_cast<long>(ev.idSensor),
                            tieneS ? sInfo.mac.c_str() : "SIN_MAC",
                            accent,
                            DbCommon::Dto::PgEnumTraits<CriticidadEvento>::toString(ev.criticidadSensor),
                            (ev.estadoReporte == EstadoEnvioCorreo::FALLIDO) ? " - REINTENTO" : "");

                        if (tieneS && !sInfo.ubicacion.vacio()) {
                            anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                                "<tr><td width=\"135\" valign=\"top\" style=\"font-weight:bold;color:#374151;\">Ubicacion:</td><td style=\"color:#111827;\">");
                            anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos, sInfo.ubicacion.c_str());
                            anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, "</td></tr>\n");
                        }

                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<tr><td width=\"135\" valign=\"top\" style=\"font-weight:bold;color:#374151;\">Transicion:</td>"
                            "<td style=\"color:#111827;\">%s &rarr; <strong style=\"color:%s;\">%s</strong> (%s)</td></tr>\n",
                            GestorSensores::nombreEstadoSensor(ev.estadoPrevioSensor),
                            accent,
                            GestorSensores::nombreEstadoSensor(ev.estadoNuevoSensor),
                            diag.titulo);

                        if (tieneC && estadoTieneLecturaValida(ev.estadoNuevoSensor) &&
                            esTemperaturaCrudaDS18B20Valida(cInfo.temperaturaCruda)) {
                            anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                                "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Lectura:</td>"
                                "<td style=\"color:#111827;\"><strong style=\"font-size:15px;\">%.2f &deg;C</strong> "
                                "<span style=\"color:#374151;\">(Rango seguro: %.1f &deg;C a %.1f &deg;C)</span></td></tr>\n",
                                static_cast<double>(cInfo.temperaturaCorregida),
                                tieneS ? static_cast<double>(sInfo.umbralInf) : -55.0,
                                tieneS ? static_cast<double>(sInfo.umbralSup) : 126.0);
                        } else {
                            anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                                "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Lectura:</td>"
                                "<td style=\"color:#B91C1C;font-weight:bold;\">SIN LECTURA VALIDA (fallo de comunicacion/bus 1-Wire)</td></tr>\n");
                        }

                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Que significa:</td><td style=\"color:#111827;\">%s</td></tr>\n"
                            "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Accion sugerida:</td><td style=\"color:#111827;\">%s</td></tr>\n"
                            "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Referencia:</td><td style=\"font-size:12px;color:#374151;\">Evento #%ld &middot; %s &middot; Bus: %s</td></tr>\n"
                            "</table></td></tr></table>\n"
                            "</td></tr>\n",
                            diag.significado,
                            diag.accion,
                            static_cast<long>(ev.idAuditoriaSensor),
                            ev.fechaCreacion.c_str(),
                            GestorSensores::nombreEstadoBus(ev.estadoBus));
                    }
                }

                // --- BLOQUE 2: EVENTOS DEL SISTEMA Y BUS 1-WIRE ---
                if (!pendSistema.empty()) {
                    anexarCabeceraSeccion(cuerpoBuf, sizeof(cuerpoBuf), pos, "2. Eventos del Sistema y Bus 1-Wire");

                    for (const auto& ev : pendSistema) {
                        if (cantAudSis >= 8) break;
                        idsAudSis[cantAudSis++] = ev.idAuditoriaSistema;
                        if (pos > sizeof(cuerpoBuf) - 1400) continue;

                        const DescripcionDiagnostico diagBus = describirEstadoBus(ev.estadoBus);
                        const char* accent = colorAcentoPorCriticidad(ev.criticidad);

                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<tr><td style=\"padding-bottom:12px;\">\n"
                            "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\" bgcolor=\"#FFFFFF\" style=\"background-color:#FFFFFF;border:1px solid #CBD5E1;border-left:5px solid %s;\">\n"
                            "<tr><td bgcolor=\"#F8FAFC\" style=\"background-color:#F8FAFC;padding:9px 12px;border-bottom:1px solid #E2E8F0;\">\n"
                            "<table width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" border=\"0\"><tr>\n"
                            "<td style=\"font-size:14px;font-weight:bold;color:#111827;\">%s</td>\n"
                            "<td align=\"right\" style=\"white-space:nowrap;\">"
                            "<span style=\"display:inline-block;background-color:%s;color:#FFFFFF;font-size:11px;font-weight:bold;padding:3px 8px;\">%s%s</span>"
                            "</td></tr></table>\n"
                            "</td></tr>\n"
                            "<tr><td style=\"padding:8px 12px;\">\n"
                            "<table width=\"100%%\" cellpadding=\"4\" cellspacing=\"0\" border=\"0\" style=\"font-size:13px;line-height:1.45;color:#111827;\">\n"
                            "<tr><td width=\"135\" valign=\"top\" style=\"font-weight:bold;color:#374151;\">Detalle:</td><td style=\"color:#111827;\">",
                            accent,
                            diagBus.titulo,
                            accent,
                            DbCommon::Dto::PgEnumTraits<CriticidadEvento>::toString(ev.criticidad),
                            (ev.estadoReporte == EstadoEnvioCorreo::FALLIDO) ? " - REINTENTO" : "");

                        anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos, ev.detalle.c_str());

                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "</td></tr>\n"
                            "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Que significa:</td><td style=\"color:#111827;\">%s</td></tr>\n"
                            "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Accion sugerida:</td><td style=\"color:#111827;\">%s</td></tr>\n"
                            "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Ultimo reset:</td><td style=\"color:#111827;\">%s (%s)</td></tr>\n"
                            "<tr><td valign=\"top\" style=\"font-weight:bold;color:#374151;\">Referencia:</td><td style=\"font-size:12px;color:#374151;\">Evento #%ld &middot; %s &middot; Heap %lu B &middot; Uptime %lu s</td></tr>\n"
                            "</table></td></tr></table>\n"
                            "</td></tr>\n",
                            diagBus.significado,
                            diagBus.accion,
                            ev.resetReason.c_str(),
                            explicarMotivoReset(ev.resetReason.c_str()),
                            static_cast<long>(ev.idAuditoriaSistema),
                            ev.fechaCreacion.c_str(),
                            static_cast<unsigned long>(ev.heapLibreBytes),
                            static_cast<unsigned long>(ev.uptimeS));
                    }
                }

                // --- BLOQUE 3: ESTADO EN VIVO DE TODAS LAS SONDAS ---
                if (pos < sizeof(cuerpoBuf) - 1400) {
                    static SnapshotSensor vivos[GestorSensores::MAX_SENSORES];
                    const uint8_t cantVivos = gestorSensores.obtenerSensoresConectados(vivos);
                    anexarCabeceraSeccion(cuerpoBuf, sizeof(cuerpoBuf), pos, "3. Estado Actual de Todos los Sensores");

                    if (cantVivos == 0) {
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<tr><td style=\"padding:6px 0;font-size:13px;color:#374151;\">No se detectan sondas activas en el bus 1-Wire en este momento.</td></tr>\n");
                    } else {
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<tr><td style=\"padding-bottom:10px;\">\n"
                            "<table width=\"100%%\" cellpadding=\"7\" cellspacing=\"0\" border=\"0\" style=\"border:1px solid #CBD5E1;font-size:12px;color:#111827;\">\n"
                            "<tr bgcolor=\"#F1F5F9\" style=\"background-color:#F1F5F9;\">\n"
                            "<th align=\"left\" style=\"border-bottom:1px solid #CBD5E1;color:#111827;\">Sensor</th>\n"
                            "<th align=\"left\" style=\"border-bottom:1px solid #CBD5E1;color:#111827;\">Temp.</th>\n"
                            "<th align=\"left\" style=\"border-bottom:1px solid #CBD5E1;color:#111827;\">Rango Seguro</th>\n"
                            "<th align=\"left\" style=\"border-bottom:1px solid #CBD5E1;color:#111827;\">Estado</th></tr>\n");

                        for (uint8_t i = 0; i < cantVivos && pos < sizeof(cuerpoBuf) - 900; ++i) {
                            const bool ok     = (vivos[i].estado == EstadoSensor::OK);
                            const bool valida = estadoTieneLecturaValida(vivos[i].estado) &&
                                                esTemperaturaCrudaDS18B20Valida(vivos[i].tempCruda);
                            char tStr[24]{};
                            if (valida) {
                                snprintf(tStr, sizeof(tStr), "%.2f &deg;C", static_cast<double>(vivos[i].tempCorregida));
                            } else {
                                copiarTexto(tStr, sizeof(tStr), "SIN LECTURA");
                            }

                            const char* rowBg = (i % 2 == 1) ? "#F8FAFC" : "#FFFFFF";
                            const char* stCol = ok ? "#047857" : "#B91C1C";

                            anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                                "<tr bgcolor=\"%s\" style=\"background-color:%s;\">\n"
                                "<td style=\"border-bottom:1px solid #E2E8F0;font-family:monospace;color:#111827;\">#%ld (%s)</td>\n"
                                "<td style=\"border-bottom:1px solid #E2E8F0;font-weight:bold;color:%s;\">%s</td>\n"
                                "<td style=\"border-bottom:1px solid #E2E8F0;color:#374151;\">%.1f a %.1f &deg;C</td>\n"
                                "<td style=\"border-bottom:1px solid #E2E8F0;font-weight:bold;color:%s;\">%s</td></tr>\n",
                                rowBg, rowBg,
                                static_cast<long>(vivos[i].idSensorDB > 0 ? vivos[i].idSensorDB : (i + 1)),
                                vivos[i].mac2,
                                stCol, tStr,
                                static_cast<double>(vivos[i].umbral_inf),
                                static_cast<double>(vivos[i].umbral_sup),
                                stCol, GestorSensores::nombreEstadoSensor(vivos[i].estado));
                        }
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, "</table></td></tr>\n");
                    }
                }

                // --- BLOQUE 4: SEGURIDAD Y AUDITORIA AUTH RECIENTE ---
                if (cantAuthRecientes > 0 && pos < sizeof(cuerpoBuf) - 700) {
                    anexarCabeceraSeccion(cuerpoBuf, sizeof(cuerpoBuf), pos, "4. Seguridad y Accesos Recientes (Auth)");
                    for (size_t i = 0; i < cantAuthRecientes && pos < sizeof(cuerpoBuf) - 420; ++i) {
                        const auto& a = authRecientes[i];
                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<tr><td style=\"padding-bottom:6px;\">\n"
                            "<table width=\"100%%\" cellpadding=\"7\" cellspacing=\"0\" border=\"0\" bgcolor=\"#F8FAFC\" style=\"background-color:#F8FAFC;border:1px solid #CBD5E1;border-left:4px solid %s;font-size:12px;color:#111827;\">\n"
                            "<tr><td><strong>%s</strong> &middot; Usuario: <span style=\"font-family:monospace;\">",
                            colorAcentoPorCriticidad(a.crit),
                            a.accion);
                        anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos, a.usuario);
                        anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "</span> &middot; %s (%s) &middot; %s</td></tr></table>\n"
                            "</td></tr>\n",
                            a.medio, a.origen, a.fecha);
                    }
                }

                // Pie con salud general del nodo
                anexarFmt(cuerpoBuf, sizeof(cuerpoBuf), pos,
                    "<tr><td style=\"padding-top:14px;border-top:1px solid #CBD5E1;font-size:12px;color:#374151;line-height:1.5;\">\n"
                    "<strong>Salud del nodo:</strong> Bus %s &middot; Heap libre %u B &middot; MicroSD libre %lu KB &middot; Uptime %lu s<br/>\n"
                    "Mensaje generado automaticamente por el sistema ESP32 UNTDF.\n"
                    "</td></tr>\n"
                    "</table></td></tr></table>\n"
                    "</td></tr></table></body></html>\n",
                    GestorSensores::nombreEstadoBus(gestorSensores.obtenerEstadoBus()),
                    static_cast<unsigned>(ESP.getFreeHeap()),
                    static_cast<unsigned long>(sdLibreKb),
                    static_cast<unsigned long>(millis() / 1000UL));

                snprintf(asunto, sizeof(asunto),
                    "[UNTDF ESP32] %s: %u evento(s) (%u sens / %u sis)",
                    (critMaxGlobal == CriticidadEvento::ALTA) ? "ALERTA CRITICA" :
                    (critMaxGlobal == CriticidadEvento::MEDIA) ? "ADVERTENCIA" : "AVISO",
                    static_cast<unsigned>(cantAudSens + cantAudSis + (hayAuthCriticoNuevo ? 1 : 0)),
                    static_cast<unsigned>(cantAudSens),
                    static_cast<unsigned>(cantAudSis));
            } // Todos los std::vector se liberan antes de BearSSL

            if (WiFi.status() != WL_CONNECTED) {
                ultimoFalloSmtpMs = ahoraMs;
                for (size_t i = 0; i < cantAudSens; ++i) {
                    try { RepoAuditoriaSensores::actualizarEstadoReporte(idsAudSens[i], EstadoEnvioCorreo::FALLIDO); } catch (...) {}
                }
                for (size_t i = 0; i < cantAudSis; ++i) {
                    try { RepoAuditoriaSistema::actualizarEstadoReporte(idsAudSis[i], EstadoEnvioCorreo::FALLIDO); } catch (...) {}
                }
                copiarTexto(out.detalle, sizeof(out.detalle), "WiFi STA desconectado; eventos marcados FALLIDO para reintento");
                return out;
            }

            bool aceptado[12]{};
            const ResultadoSmtp r = ClienteSmtpTls::enviar(
                armarConfig(smtpDto),
                listaDest,
                cantDest,
                asunto,
                cuerpoBuf,
                aceptado,
                true
            );

            out.codigoSmtp      = r.codigoSmtp;
            out.exito           = r.exito;
            out.correosEnviados = r.destinatariosAceptados;

            for (size_t i = 0; i < cantDest; ++i) {
                try {
                    RepoCorreos::registrarEstadoEnvioPorEmail(
                        listaDest[i],
                        (r.exito && aceptado[i]) ? EstadoEnvioCorreo::ENVIADO : EstadoEnvioCorreo::FALLIDO
                    );
                } catch (...) {}
            }

            const EstadoEnvioCorreo estadoFinal = r.exito
                ? EstadoEnvioCorreo::ENVIADO
                : EstadoEnvioCorreo::FALLIDO;

            uint16_t marcados = 0;
            for (size_t i = 0; i < cantAudSens; ++i) {
                try {
                    RepoAuditoriaSensores::actualizarEstadoReporte(idsAudSens[i], estadoFinal);
                    marcados++;
                } catch (...) {}
            }
            for (size_t i = 0; i < cantAudSis; ++i) {
                try {
                    RepoAuditoriaSistema::actualizarEstadoReporte(idsAudSis[i], estadoFinal);
                    marcados++;
                } catch (...) {}
            }
            out.eventosActualizados = marcados;

            if (r.exito) {
                ultimoEnvioExitoMs = millis();
                if (hayEventoCriticoNuevo) {
                    ultimoEnvioCriticoMs = ultimoEnvioExitoMs;
                }
                ultimoFalloSmtpMs = 0;
                if (maxSeqAuthEnCorreo > ultimoSeqAuthNotificado) {
                    ultimoSeqAuthNotificado = maxSeqAuthEnCorreo;
                }
                snprintf(out.detalle, sizeof(out.detalle),
                         "SMTP 250 OK - %u evento(s) -> ENVIADO (%u dest.)",
                         static_cast<unsigned>(marcados),
                         static_cast<unsigned>(r.destinatariosAceptados));
            } else {
                ultimoFalloSmtpMs = millis();
                snprintf(out.detalle, sizeof(out.detalle),
                         "Fallo SMTP (%u) - %u ev. -> FALLIDO: %.55s",
                         static_cast<unsigned>(r.codigoSmtp),
                         static_cast<unsigned>(marcados),
                         r.detalle);
            }

            return out;
        }
    };

} // namespace SmtpNet