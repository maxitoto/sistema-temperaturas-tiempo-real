#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <stdio.h>
#include <string.h>

#include "ClienteSmtpTls.hpp"
#include "../../database/repositories/repoConfiguracionSistema.hpp"
#include "../../database/repositories/repoConfiguracionSmtp.hpp"
#include "../../database/repositories/repoCorreos.hpp"
#include "../../database/repositories/repoAuditoriaSensores.hpp"
#include "../../database/repositories/repoAuditoriaSistema.hpp"
#include "../../database/repositories/repoSensor.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
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
        inline static uint32_t ultimoEnvioExitoMs = 0;
        inline static uint32_t ultimoFalloSmtpMs  = 0;
        // Buffer estático exclusivo de Core 1 (TaskAlertasDB) para no consumir stack en TLS
        inline static char cuerpoBuf[1280]{};

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

        static bool destinatarioAceptaCriticidad(TipoRecepcionCorreo filtro, CriticidadEvento crit) {
            if (filtro == TipoRecepcionCorreo::RECIBIR_TODOS) return true;
            if (filtro == TipoRecepcionCorreo::RECIBIR_EVENTOS) {
                return crit != CriticidadEvento::NULA;
            }
            return crit == CriticidadEvento::ALTA;
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
        // Prueba directa de configuración SMTP (usada desde el botón en Electron)
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

            const RTOS::PgDateTime ahoraStr = RTOS::RelojSistema::fechaHoraActualPg();
            snprintf(
                cuerpoBuf,
                sizeof(cuerpoBuf),
                "Prueba de conectividad SMTP exitosa desde nodo ESP32 UNTDF.\n\n"
                "Fecha / Hora ESP32: %s\n"
                "IP LAN (STA):       %s\n"
                "Servidor SMTP:      %s:%u (TLS=%s)\n"
                "Heap Libre:         %u B\n"
                "Uptime:             %lu s\n",
                ahoraStr.c_str(),
                WiFi.localIP().toString().c_str(),
                smtpDto.hostSmtp.c_str(),
                static_cast<unsigned>(smtpDto.puerto),
                smtpDto.usarTls ? "SI" : "NO",
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned long>(millis() / 1000UL)
            );

            const char* listaDest[1] = { destFinal };
            bool aceptado[1] = { false };

            // Envío por red SIN tomar MUTEX_SD
            const ResultadoSmtp r = ClienteSmtpTls::enviar(
                armarConfig(smtpDto),
                listaDest,
                1,
                "[UNTDF ESP32] Prueba de Correo SMTP",
                cuerpoBuf,
                aceptado
            );

            out.codigoSmtp      = r.codigoSmtp;
            out.exito           = r.exito;
            out.correosEnviados = r.destinatariosAceptados;
            copiarTexto(out.detalle, sizeof(out.detalle), r.detalle);

            // Si el destino figura en la tabla Correos, actualiza su último estado de envío
            CorreoDTO cDb{};
            if (RepoCorreos::obtenerPorEmail(destFinal, cDb)) {
                RepoCorreos::registrarEstadoEnvioPorEmail(
                    destFinal,
                    r.exito ? EstadoEnvioCorreo::ENVIADO : EstadoEnvioCorreo::FALLIDO
                );
            }
            return out;
        }

        // Despacha alertas PENDIENTE de AuditoriaSensores y AuditoriaSistema en 1 sesión TLS
        // Despacha alertas PENDIENTE de AuditoriaSensores y AuditoriaSistema en 1 sesión TLS
        static ResumenDespachoSmtp despacharPendientes(bool ignorarCooldown = false) {
            using namespace DbRuntime::Repositories;
            ResumenDespachoSmtp out{};

            if (WiFi.status() != WL_CONNECTED) {
                copiarTexto(out.detalle, sizeof(out.detalle), "WiFi STA desconectado (sin internet)");
                return out;
            }

            const uint32_t ahoraMs = millis();
            if (!ignorarCooldown && ultimoFalloSmtpMs != 0 && (ahoraMs - ultimoFalloSmtpMs) < 60000UL) {
                copiarTexto(out.detalle, sizeof(out.detalle), "Esperando backoff tras ultimo fallo SMTP");
                return out;
            }

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

            if (!ignorarCooldown &&
                ultimoEnvioExitoMs != 0 &&
                cfgSis.correoCooldownS > 0 &&
                (ahoraMs - ultimoEnvioExitoMs) < (cfgSis.correoCooldownS * 1000UL)) {
                copiarTexto(out.detalle, sizeof(out.detalle), "En ventana de cooldown de correos");
                return out;
            }

            // Buffers fijos (0 heap) para destruir todos los std::vector antes de iniciar TLS
            static char destBuf[12][48]{};
            const char* listaDest[12]{};
            size_t      cantDest    = 0;
            int32_t     idsAudSens[6]{};
            size_t      cantAudSens = 0;
            int32_t     idsAudSis[4]{};
            size_t      cantAudSis  = 0;
            char        asunto[96]{};

            {
                auto destinatariosDb = RepoCorreos::listarPaginados(12, 0, true);
                if (destinatariosDb.empty()) {
                    copiarTexto(out.detalle, sizeof(out.detalle), "No hay destinatarios activos en Correos");
                    return out;
                }

                auto pendSensores = RepoAuditoriaSensores::listarPendientesReporte(12);
                auto pendSistema  = RepoAuditoriaSistema::listarPendientesReporte(8);
                if (pendSensores.empty() && pendSistema.empty()) {
                    out.exito = true;
                    copiarTexto(out.detalle, sizeof(out.detalle), "No hay avisos PENDIENTE para enviar");
                    return out;
                }

                bool destElegido[12]{};
                size_t pos = 0;
                cuerpoBuf[0] = '\0';
                char lin[160]{};

                snprintf(lin, sizeof(lin),
                         "Reporte automatico de eventos - Nodo ESP32 UNTDF\nFecha: %s | IP: %s\n\n",
                         RTOS::RelojSistema::fechaHoraActualPg().c_str(),
                         WiFi.localIP().toString().c_str());
                anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, lin);

                bool cabeceraSensEscrita = false;
                for (const auto& ev : pendSensores) {
                    if (cantAudSens >= 6) break;
                    bool algunDest = false;
                    for (size_t d = 0; d < destinatariosDb.size() && d < 12; ++d) {
                        if (destinatarioAceptaCriticidad(destinatariosDb[d].tipoRecepcionMensajes, ev.criticidadSensor)) {
                            destElegido[d] = true;
                            algunDest = true;
                        }
                    }
                    if (!algunDest) continue;

                    if (!cabeceraSensEscrita) {
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, "--- EVENTOS DE SENSORES ---\n");
                        cabeceraSensEscrita = true;
                    }
                    idsAudSens[cantAudSens++] = ev.idAuditoriaSensor;

                    SensorDTO sInfo{};
                    const bool tieneS = RepoSensor::obtenerPorId(ev.idSensor, sInfo);
                    snprintf(lin, sizeof(lin),
                             "- #%ld Sensor #%ld (%s): %s -> %s [%s] (%s)\n",
                             static_cast<long>(ev.idAuditoriaSensor),
                             static_cast<long>(ev.idSensor),
                             tieneS && !sInfo.nombre.vacio() ? sInfo.nombre.c_str() : "DS18B20",
                             GestorSensores::nombreEstadoSensor(ev.estadoPrevioSensor),
                             GestorSensores::nombreEstadoSensor(ev.estadoNuevoSensor),
                             DbCommon::Dto::PgEnumTraits<CriticidadEvento>::toString(ev.criticidadSensor),
                             ev.fechaCreacion.c_str());
                    anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, lin);
                }
                if (cabeceraSensEscrita) {
                    anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, "\n");
                }

                bool cabeceraSisEscrita = false;
                for (const auto& ev : pendSistema) {
                    if (cantAudSis >= 4) break;
                    bool algunDest = false;
                    for (size_t d = 0; d < destinatariosDb.size() && d < 12; ++d) {
                        if (destinatarioAceptaCriticidad(destinatariosDb[d].tipoRecepcionMensajes, ev.criticidad)) {
                            destElegido[d] = true;
                            algunDest = true;
                        }
                    }
                    if (!algunDest) continue;

                    if (!cabeceraSisEscrita) {
                        anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, "--- EVENTOS DEL SISTEMA ---\n");
                        cabeceraSisEscrita = true;
                    }
                    idsAudSis[cantAudSis++] = ev.idAuditoriaSistema;

                    snprintf(lin, sizeof(lin),
                             "- #%ld [%s] %s | Bus:%s | Reset:%s (%s)\n",
                             static_cast<long>(ev.idAuditoriaSistema),
                             DbCommon::Dto::PgEnumTraits<CriticidadEvento>::toString(ev.criticidad),
                             ev.detalle.c_str(),
                             GestorSensores::nombreEstadoBus(ev.estadoBus),
                             ev.resetReason.c_str(),
                             ev.fechaCreacion.c_str());
                    anexarTexto(cuerpoBuf, sizeof(cuerpoBuf), pos, lin);
                }

                if (cantAudSens == 0 && cantAudSis == 0) {
                    out.exito = true;
                    copiarTexto(out.detalle, sizeof(out.detalle),
                                "Pendientes existentes no coinciden con filtros de criticidad");
                    return out;
                }

                for (size_t d = 0; d < destinatariosDb.size() && d < 12; ++d) {
                    if (destElegido[d]) {
                        copiarTexto(destBuf[cantDest], sizeof(destBuf[cantDest]), destinatariosDb[d].correoElectronico.c_str());
                        listaDest[cantDest] = destBuf[cantDest];
                        cantDest++;
                    }
                }
            } // Aquí se liberan todos los std::vector antes de mbedTLS

            if (cantDest == 0) {
                copiarTexto(out.detalle, sizeof(out.detalle), "Sin destinatarios elegibles");
                return out;
            }

            snprintf(asunto, sizeof(asunto),
                     "[UNTDF ESP32] Alerta: %u evento(s) pendiente(s)",
                     static_cast<unsigned>(cantAudSens + cantAudSis));

            // Envío SMTP en 1 sola sesión TLS con el máximo Heap disponible
            bool aceptado[12]{};
            const ResultadoSmtp r = ClienteSmtpTls::enviar(
                armarConfig(smtpDto),
                listaDest,
                cantDest,
                asunto,
                cuerpoBuf,
                aceptado
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

            if (r.exito) {
                if (!ignorarCooldown) {
                    ultimoEnvioExitoMs = millis();
                }
                ultimoFalloSmtpMs = 0;

                uint16_t marcados = 0;
                for (size_t i = 0; i < cantAudSens; ++i) {
                    try {
                        RepoAuditoriaSensores::actualizarEstadoReporte(idsAudSens[i], EstadoEnvioCorreo::ENVIADO);
                        marcados++;
                    } catch (...) {}
                }
                for (size_t i = 0; i < cantAudSis; ++i) {
                    try {
                        RepoAuditoriaSistema::actualizarEstadoReporte(idsAudSis[i], EstadoEnvioCorreo::ENVIADO);
                        marcados++;
                    } catch (...) {}
                }
                out.eventosActualizados = marcados;
                snprintf(out.detalle, sizeof(out.detalle),
                         "SMTP 250 OK · %u aviso(s) -> ENVIADO (%u dest.)",
                         static_cast<unsigned>(marcados),
                         static_cast<unsigned>(r.destinatariosAceptados));
            } else {
                ultimoFalloSmtpMs = millis();
                copiarTexto(out.detalle, sizeof(out.detalle), r.detalle);
            }

            return out;
        }
    };

} // namespace SmtpNet