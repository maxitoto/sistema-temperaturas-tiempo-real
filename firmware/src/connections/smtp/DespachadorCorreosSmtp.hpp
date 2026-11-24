#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <vector>
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
        inline static uint32_t ultimoEnvioExitoMs       = 0;
        inline static uint32_t ultimoEnvioCriticoMs     = 0;
        inline static uint32_t ultimoFalloSmtpMs        = 0;
        inline static uint64_t ultimoSeqAuthNotificado  = 0;
        // Buffer HTML estático exclusivo de Core 1 (TaskAlertasDB) — 0 stack en BearSSL
        inline static char cuerpoBuf[7168]{};

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

        static const char* colorFondoPorCriticidad(CriticidadEvento c) {
            if (c == CriticidadEvento::ALTA)  return "#dc2626";
            if (c == CriticidadEvento::MEDIA) return "#d97706";
            return "#2563eb";
        }

        static const char* colorSuavePorCriticidad(CriticidadEvento c) {
            if (c == CriticidadEvento::ALTA)  return "#fef2f2";
            if (c == CriticidadEvento::MEDIA) return "#fffbeb";
            return "#eff6ff";
        }

        static const char* colorBordePorCriticidad(CriticidadEvento c) {
            if (c == CriticidadEvento::ALTA)  return "#fca5a5";
            if (c == CriticidadEvento::MEDIA) return "#fcd34d";
            return "#93c5fd";
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

    public:
        // Prueba directa de conectividad SMTP (para verificar credenciales al configurar)
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
            const uint32_t sdLibreKb  = TarjetaSD::getInstance().estaInicializada()
                ? TarjetaSD::getInstance().espacioLibreKB() : 0;

            snprintf(
                cuerpoBuf,
                sizeof(cuerpoBuf),
                "<!DOCTYPE html><html><body style=\"margin:0;padding:12px;background:#f3f4f6;font-family:Arial,Helvetica,sans-serif;color:#111827;\">"
                "<div style=\"max-width:560px;margin:0 auto;background:#ffffff;border-radius:10px;overflow:hidden;border:1px solid #e5e7eb;\">"
                "<div style=\"background:#059669;color:#ffffff;padding:16px 20px;\">"
                "<div style=\"font-size:12px;letter-spacing:0.6px;text-transform:uppercase;opacity:0.9;\">UNTDF · Monitoreo Termico ESP32</div>"
                "<div style=\"font-size:18px;font-weight:bold;margin-top:4px;\">Prueba de Correo SMTP Exitosa</div>"
                "</div>"
                "<div style=\"padding:16px 20px;font-size:14px;line-height:1.5;\">"
                "<p style=\"margin:0 0 12px;\">El nodo ESP32 establecio conexion TLS con el servidor SMTP y enviara alertas autonomamente ante cualquier incidente.</p>"
                "<div style=\"background:#f9fafb;border:1px solid #e5e7eb;border-radius:8px;padding:12px;font-size:13px;\">"
                "<div><b>Fecha / Hora:</b> %s</div>"
                "<div><b>IP LAN:</b> %s &nbsp;·&nbsp; <b>WiFi:</b> %d dBm</div>"
                "<div><b>Servidor SMTP:</b> %s:%u (TLS %s)</div>"
                "<div><b>Heap Libre:</b> %u B &nbsp;·&nbsp; <b>MicroSD Libre:</b> %lu KB</div>"
                "<div><b>Uptime:</b> %lu s</div>"
                "</div></div></div></body></html>",
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

        // Despacho autónomo de eventos PENDIENTE y reintento de FALLIDO (con HTML responsivo y contexto completo)
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

            // Buffers fijos (0 heap) para liberar todos los std::vector antes de abrir BearSSL
            static char destBuf[12][48]{};
            const char* listaDest[12]{};
            size_t      cantDest    = 0;
            int32_t     idsAudSens[10]{};
            size_t      cantAudSens = 0;
            int32_t     idsAudSis[8]{};
            size_t      cantAudSis  = 0;
            uint64_t    maxSeqAuthEnCorreo = ultimoSeqAuthNotificado;
            bool        hayEventoCriticoNuevo = false;
            bool        hayPendienteNuevo     = false;
            CriticidadEvento critMaxGlobal = CriticidadEvento::BAJA;
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

                // Revisar últimos eventos de seguridad Auth (MEDIA/ALTA) para contexto y aviso de bloqueos
                struct AuthContextItem {
                    uint64_t seq;
                    char usuario[20];
                    char accion[28];
                    char medio[12];
                    char origen[20];
                    char fecha[20];
                    CriticidadEvento crit;
                    ResultadoAuditoria res;
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
                        it.res  = r.getEnum<ResultadoAuditoria>("resultado");
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

                // Evaluar severidad máxima y si hay alertas urgentes
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

                // Cooldown inteligente:
                // - Si hubo fallo previo y NO hay alerta nueva, respeta backoff de 60 s.
                // - Si hay alerta crítica (ALTA) nueva, solo respeta 30 s anti-tormenta (no espera 15 min).
                // - Para eventos normales (MEDIA/BAJA), respeta correoCooldownS.
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

                // Determinar destinatarios elegibles según criticidad
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

                // Construir cuerpo HTML responsivo (legible en celular y PC)
                size_t pos = 0;
                cuerpoBuf[0] = '\0';
                char tmp[320]{};

                const char* hdrColor   = colorFondoPorCriticidad(critMaxGlobal);
                const char* tituloSev  = (critMaxGlobal == CriticidadEvento::ALTA)
                    ? "ALERTA CRITICA DETECTADA"
                    : (critMaxGlobal == CriticidadEvento::MEDIA)
                        ? "ADVERTENCIA DEL SISTEMA"
                        : "REPORTE INFORMATIVO";
                const uint32_t sdLibreKb = TarjetaSD::getInstance().estaInicializada()
                    ? TarjetaSD::getInstance().espacioLibreKB() : 0;

                snprintf(tmp, sizeof(tmp),
                    "<!DOCTYPE html><html><body style=\"margin:0;padding:10px;background:#f3f4f6;font-family:Arial,Helvetica,sans-serif;color:#111827;\">"
                    "<div style=\"max-width:600px;margin:0 auto;background:#ffffff;border-radius:10px;overflow:hidden;border:1px solid #d1d5db;\">"
                    "<div style=\"background:%s;color:#ffffff;padding:16px 18px;\">"
                    "<div style=\"font-size:11px;letter-spacing:0.8px;text-transform:uppercase;opacity:0.9;\">UNTDF · Control Termico Industrial ESP32</div>"
                    "<div style=\"font-size:19px;font-weight:bold;margin-top:4px;\">%s</div>"
                    "<div style=\"font-size:12px;margin-top:6px;opacity:0.95;\">%s &nbsp;|&nbsp; IP %s &nbsp;|&nbsp; WiFi %d dBm</div>"
                    "</div><div style=\"padding:14px 16px;\">",
                    hdrColor,
                    tituloSev,
                    RTOS::RelojSistema::fechaHoraActualPg().c_str(),
                    WiFi.localIP().toString().c_str(),
                    static_cast<int>(WiFi.RSSI())
                );
                anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);

                // --- BLOQUE 1: EVENTOS DE SENSORES ---
                if (!pendSensores.empty()) {
                    anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                        "<div style=\"font-size:13px;font-weight:bold;color:#374151;text-transform:uppercase;margin:4px 0 8px;border-bottom:1px solid #e5e7eb;padding-bottom:4px;\">"
                        "Eventos de Sensores DS18B20</div>");

                    for (const auto& ev : pendSensores) {
                        if (cantAudSens >= 10) break;
                        idsAudSens[cantAudSens++] = ev.idAuditoriaSensor;
                        if (pos > sizeof(cuerpoBuf) - 1400) continue;

                        SensorDTO sInfo{};
                        const bool tieneS = RepoSensor::obtenerPorId(ev.idSensor, sInfo);
                        CensoDTO cInfo{};
                        const bool tieneC = (ev.idCenso > 0) && RepoCenso::obtenerPorId(ev.idCenso, cInfo);
                        const DescripcionDiagnostico diag = describirEstadoSensor(ev.estadoNuevoSensor);

                        snprintf(tmp, sizeof(tmp),
                            "<div style=\"background:%s;border:1px solid %s;border-left:4px solid %s;border-radius:8px;padding:10px 12px;margin-bottom:10px;\">"
                            "<div style=\"font-size:14px;font-weight:bold;color:#111827;\">",
                            colorSuavePorCriticidad(ev.criticidadSensor),
                            colorBordePorCriticidad(ev.criticidadSensor),
                            colorFondoPorCriticidad(ev.criticidadSensor));
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);

                        anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            (tieneS && !sInfo.nombre.vacio()) ? sInfo.nombre.c_str() : "Sensor DS18B20");

                        snprintf(tmp, sizeof(tmp),
                            " <span style=\"font-size:11px;font-weight:normal;color:#4b5563;\">(#%ld · %s)</span>"
                            "<span style=\"float:right;background:%s;color:#ffffff;font-size:10px;padding:2px 7px;border-radius:10px;\">%s%s</span></div>",
                            static_cast<long>(ev.idSensor),
                            tieneS ? sInfo.mac.c_str() : "SIN_MAC",
                            colorFondoPorCriticidad(ev.criticidadSensor),
                            DbCommon::Dto::PgEnumTraits<CriticidadEvento>::toString(ev.criticidadSensor),
                            (ev.estadoReporte == EstadoEnvioCorreo::FALLIDO) ? " · REINTENTO" : "");
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);

                        if (tieneS && !sInfo.ubicacion.vacio()) {
                            anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                                "<div style=\"font-size:12px;color:#4b5563;margin-top:2px;\">Ubicacion: <b>");
                            anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos, sInfo.ubicacion.c_str());
                            anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, "</b></div>");
                        }

                        snprintf(tmp, sizeof(tmp),
                            "<div style=\"font-size:13px;margin-top:6px;\"><b>Cambio:</b> %s &rarr; <b>%s</b> (%s)</div>",
                            GestorSensores::nombreEstadoSensor(ev.estadoPrevioSensor),
                            GestorSensores::nombreEstadoSensor(ev.estadoNuevoSensor),
                            diag.titulo);
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);

                        if (tieneC && estadoTieneLecturaValida(ev.estadoNuevoSensor)) {
                            snprintf(tmp, sizeof(tmp),
                                "<div style=\"font-size:13px;margin-top:4px;\"><b>Lectura:</b> <span style=\"font-size:15px;font-weight:bold;\">%.2f &deg;C</span>"
                                " &nbsp;<span style=\"color:#4b5563;\">(Rango seguro: %.1f &deg;C a %.1f &deg;C)</span></div>",
                                static_cast<double>(cInfo.temperaturaCorregida),
                                tieneS ? static_cast<double>(sInfo.umbralInf) : -55.0,
                                tieneS ? static_cast<double>(sInfo.umbralSup) : 126.0);
                            anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);
                        }

                        snprintf(tmp, sizeof(tmp),
                            "<div style=\"font-size:12px;color:#1f2937;margin-top:6px;\"><b>Que significa:</b> %s</div>"
                            "<div style=\"font-size:12px;color:#374151;margin-top:3px;\"><b>Accion recomendada:</b> %s</div>"
                            "<div style=\"font-size:11px;color:#6b7280;margin-top:5px;\">Evento #%ld · %s · Bus 1-Wire: %s</div></div>",
                            diag.significado,
                            diag.accion,
                            static_cast<long>(ev.idAuditoriaSensor),
                            ev.fechaCreacion.c_str(),
                            GestorSensores::nombreEstadoBus(ev.estadoBus));
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);
                    }
                }

                // --- BLOQUE 2: EVENTOS DEL SISTEMA Y BUS ---
                if (!pendSistema.empty()) {
                    anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                        "<div style=\"font-size:13px;font-weight:bold;color:#374151;text-transform:uppercase;margin:12px 0 8px;border-bottom:1px solid #e5e7eb;padding-bottom:4px;\">"
                        "Eventos del Sistema y Bus 1-Wire</div>");

                    for (const auto& ev : pendSistema) {
                        if (cantAudSis >= 8) break;
                        idsAudSis[cantAudSis++] = ev.idAuditoriaSistema;
                        if (pos > sizeof(cuerpoBuf) - 1100) continue;

                        const DescripcionDiagnostico diagBus = describirEstadoBus(ev.estadoBus);

                        snprintf(tmp, sizeof(tmp),
                            "<div style=\"background:%s;border:1px solid %s;border-left:4px solid %s;border-radius:8px;padding:10px 12px;margin-bottom:10px;\">"
                            "<div style=\"font-size:13px;font-weight:bold;\">%s"
                            "<span style=\"float:right;background:%s;color:#ffffff;font-size:10px;padding:2px 7px;border-radius:10px;\">%s%s</span></div>"
                            "<div style=\"font-size:12px;margin-top:4px;\"><b>Detalle:</b> ",
                            colorSuavePorCriticidad(ev.criticidad),
                            colorBordePorCriticidad(ev.criticidad),
                            colorFondoPorCriticidad(ev.criticidad),
                            diagBus.titulo,
                            colorFondoPorCriticidad(ev.criticidad),
                            DbCommon::Dto::PgEnumTraits<CriticidadEvento>::toString(ev.criticidad),
                            (ev.estadoReporte == EstadoEnvioCorreo::FALLIDO) ? " · REINTENTO" : "");
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);

                        anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos, ev.detalle.c_str());

                        snprintf(tmp, sizeof(tmp),
                            "</div>"
                            "<div style=\"font-size:12px;color:#1f2937;margin-top:4px;\"><b>Que significa:</b> %s</div>"
                            "<div style=\"font-size:12px;color:#374151;margin-top:3px;\"><b>Accion recomendada:</b> %s</div>"
                            "<div style=\"font-size:11px;color:#4b5563;margin-top:5px;\"><b>Ultimo reset:</b> %s (%s)</div>"
                            "<div style=\"font-size:11px;color:#6b7280;margin-top:3px;\">Evento #%ld · %s · Heap: %lu B · Uptime: %lu s</div></div>",
                            diagBus.significado,
                            diagBus.accion,
                            ev.resetReason.c_str(),
                            explicarMotivoReset(ev.resetReason.c_str()),
                            static_cast<long>(ev.idAuditoriaSistema),
                            ev.fechaCreacion.c_str(),
                            static_cast<unsigned long>(ev.heapLibreBytes),
                            static_cast<unsigned long>(ev.uptimeS));
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);
                    }
                }

                // --- BLOQUE 3: CONTEXTO EXTRA — ESTADO EN VIVO DE TODAS LAS SONDAS ---
                if (pos < sizeof(cuerpoBuf) - 950) {
                    static SnapshotSensor vivos[GestorSensores::MAX_SENSORES];
                    const uint8_t cantVivos = gestorSensores.obtenerSensoresConectados(vivos);
                    anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                        "<div style=\"font-size:13px;font-weight:bold;color:#374151;text-transform:uppercase;margin:12px 0 6px;border-bottom:1px solid #e5e7eb;padding-bottom:4px;\">"
                        "Estado Actual de Todos los Sensores</div>");

                    if (cantVivos == 0) {
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<div style=\"font-size:12px;color:#6b7280;margin-bottom:8px;\">No se detectan sondas activas en el bus 1-Wire en este momento.</div>");
                    } else {
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                            "<table style=\"width:100%;border-collapse:collapse;font-size:12px;margin-bottom:10px;\">"
                            "<tr style=\"background:#f3f4f6;text-align:left;\">"
                            "<th style=\"padding:6px;border-bottom:1px solid #e5e7eb;\">Sensor</th>"
                            "<th style=\"padding:6px;border-bottom:1px solid #e5e7eb;\">Temp</th>"
                            "<th style=\"padding:6px;border-bottom:1px solid #e5e7eb;\">Rango</th>"
                            "<th style=\"padding:6px;border-bottom:1px solid #e5e7eb;\">Estado</th></tr>");

                        for (uint8_t i = 0; i < cantVivos && pos < sizeof(cuerpoBuf) - 650; ++i) {
                            const bool ok = (vivos[i].estado == EstadoSensor::OK);
                            const bool valida = estadoTieneLecturaValida(vivos[i].estado);
                            char tStr[16]{};
                            if (valida) snprintf(tStr, sizeof(tStr), "%.2f &deg;C", static_cast<double>(vivos[i].tempCorregida));
                            else copiarTexto(tStr, sizeof(tStr), "FALLO");

                            snprintf(tmp, sizeof(tmp),
                                "<tr><td style=\"padding:5px 6px;border-bottom:1px solid #f3f4f6;font-family:monospace;\">#%ld %s</td>"
                                "<td style=\"padding:5px 6px;border-bottom:1px solid #f3f4f6;font-weight:bold;color:%s;\">%s</td>"
                                "<td style=\"padding:5px 6px;border-bottom:1px solid #f3f4f6;color:#6b7280;\">%.1f..%.1f&deg;</td>"
                                "<td style=\"padding:5px 6px;border-bottom:1px solid #f3f4f6;color:%s;font-weight:bold;\">%s</td></tr>",
                                static_cast<long>(vivos[i].idSensorDB > 0 ? vivos[i].idSensorDB : (i + 1)),
                                vivos[i].mac2,
                                ok ? "#059669" : "#dc2626",
                                tStr,
                                static_cast<double>(vivos[i].umbral_inf),
                                static_cast<double>(vivos[i].umbral_sup),
                                ok ? "#059669" : "#dc2626",
                                GestorSensores::nombreEstadoSensor(vivos[i].estado));
                            anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);
                        }
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, "</table>");
                    }
                }

                // --- BLOQUE 4: CONTEXTO EXTRA — SEGURIDAD Y AUDITORIA AUTH RECIENTE ---
                if (cantAuthRecientes > 0 && pos < sizeof(cuerpoBuf) - 550) {
                    anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos,
                        "<div style=\"font-size:13px;font-weight:bold;color:#374151;text-transform:uppercase;margin:10px 0 6px;border-bottom:1px solid #e5e7eb;padding-bottom:4px;\">"
                        "Alertas Recientes de Seguridad / Accesos (Auth)</div>");

                    for (size_t i = 0; i < cantAuthRecientes && pos < sizeof(cuerpoBuf) - 320; ++i) {
                        const auto& a = authRecientes[i];
                        snprintf(tmp, sizeof(tmp),
                            "<div style=\"font-size:12px;background:#f9fafb;border-left:3px solid %s;padding:6px 8px;margin-bottom:5px;border-radius:4px;\">"
                            "<b>%s</b> · Usuario: <code>",
                            colorFondoPorCriticidad(a.crit),
                            a.accion);
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);
                        anexarHtmlEscapado(cuerpoBuf, sizeof(cuerpoBuf), pos, a.usuario);
                        snprintf(tmp, sizeof(tmp),
                            "</code> · %s (%s) · %s</div>",
                            a.medio, a.origen, a.fecha);
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);
                    }
                }

                // Pie con salud general del nodo
                snprintf(tmp, sizeof(tmp),
                    "<div style=\"margin-top:14px;padding-top:10px;border-top:1px solid #e5e7eb;font-size:11px;color:#6b7280;\">"
                    "Salud del nodo: Bus <b>%s</b> · Heap libre <b>%u B</b> · MicroSD libre <b>%lu KB</b> · Uptime <b>%lu s</b><br/>"
                    "Mensaje generado automaticamente por el nodo ESP32 UNTDF."
                    "</div></div></div></body></html>",
                    GestorSensores::nombreEstadoBus(gestorSensores.obtenerEstadoBus()),
                    static_cast<unsigned>(ESP.getFreeHeap()),
                    static_cast<unsigned long>(sdLibreKb),
                    static_cast<unsigned long>(millis() / 1000UL));
                anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, tmp);

                snprintf(asunto, sizeof(asunto),
                    "[UNTDF ESP32] %s: %u evento(s) (%u sens / %u sis)",
                    (critMaxGlobal == CriticidadEvento::ALTA) ? "ALERTA CRITICA" :
                    (critMaxGlobal == CriticidadEvento::MEDIA) ? "ADVERTENCIA" : "AVISO",
                    static_cast<unsigned>(cantAudSens + cantAudSis + (hayAuthCriticoNuevo ? 1 : 0)),
                    static_cast<unsigned>(cantAudSens),
                    static_cast<unsigned>(cantAudSis));
            } // Todos los std::vector se liberan antes de BearSSL

            // Si WiFi está caído, marcar los PENDIENTE como FALLIDO para reflejarlo y reintentar luego
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

            // Envío SMTP en 1 sola sesión TLS
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
                         "SMTP 250 OK · %u evento(s) -> ENVIADO (%u dest.)",
                         static_cast<unsigned>(marcados),
                         static_cast<unsigned>(r.destinatariosAceptados));
            } else {
                ultimoFalloSmtpMs = millis();
                snprintf(out.detalle, sizeof(out.detalle),
                         "Fallo SMTP (%u) · %u evento(s) -> FALLIDO: %.64s",
                         static_cast<unsigned>(r.codigoSmtp),
                         static_cast<unsigned>(marcados),
                         r.detalle);
            }

            return out;
        }
    };

} // namespace SmtpNet