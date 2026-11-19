#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_system.h>
#include "../PrimitivasRTOS.hpp"
#include "../EventosYColas.hpp"
#include "../../hardware/driver/buzz/Buzz.hpp"
#include "../../hardware/driver/buzz/frecuencias.hpp"
#include "../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "../../database/repositories/RepositorioBase.hpp"
#include "../../database/repositories/repoSensor.hpp"
#include "../../database/repositories/repoSenso.hpp"
#include "../../database/repositories/repoAuditoriaSensores.hpp"
#include "../../database/repositories/repoAuditoriaSistema.hpp"
#include "../../database/repositories/repoAuditoriaAuth.hpp"
#include "../../database/repositories/repoConfiguracionSistema.hpp"
#include "../../database/repositories/repoConfiguracionSmtp.hpp"
#include "../../database/repositories/repoCorreos.hpp"

#include "../../connections/middlewares/ColaTrabajoRPC.hpp"
#include "../../connections/middlewares/middleware_sesion.hpp"

namespace RTOS::Tasks {

    class TaskAlertasYPersistencia {
    private:
        inline static TareaSegura tarea;
        inline static uint32_t    ultimoMantenimientoMs  = 0;
        inline static uint32_t    ultimoChequeoCorreoMs  = 0;
        inline static uint32_t    ultimoEnvioSmtpExitoMs = 0;
        inline static ColaTrabajoRPC*    refColaRpc           = nullptr;
        inline static MiddlewareSesion*  refL2                = nullptr;
        inline static MiddlewareMessage* refL3                = nullptr;
        inline static uint32_t           ultimoCicloAlertasMs = 0;
        inline static uint32_t           ultimoStreamingMs    = 0;
        inline static uint32_t           ultimaExpSesionMs    = 0;

        static void procesarTrabajoRpcEnCore1() {
            if (!refColaRpc || !refL2) return;

            // 1. Limpiar sesiones/streams de slots desconectados por Core 0
            const uint16_t limpiezas = refColaRpc->extraerLimpiezasSesionPendientes();
            if (limpiezas != 0) {
                for (uint8_t s = 0; s <= 8; ++s) {
                    if (limpiezas & (1u << s)) {
                        refL2->forzarCierreSesionPorDesconexion(s);
                    }
                }
            }

            // 2. Despachar 1 petición RPC (espera hasta 15 ms para responder con baja latencia)
            uint8_t idx = 0;
            PeticionEncoladaRPC* pet = nullptr;
            if (!refColaRpc->esperarSiguientePeticion(idx, pet, pdMS_TO_TICKS(15))) {
                return;
            }

            if (pet->generacionSlot == refColaRpc->obtenerGeneracionSlot(pet->slotClienteOrigen)) {
                SobreMensajeRPC sobre{
                    pet->slotClienteOrigen,
                    pet->idProcedimiento,
                    pet->idTicket,
                    pet->hashPeticion,
                    RolUsuario::NINGUNO,
                    pet->payloadEntrada,
                    pet->longitudEntrada
                };
                refL2->procesarMensajeEntrante(sobre);
            }

            refColaRpc->liberarPeticion(idx);
        }
        static constexpr uint32_t INTERVALO_MANTENIMIENTO_MS  = 3600000UL; // 1 hora
        static constexpr uint32_t INTERVALO_CHEQUEO_CORREO_MS = 30000UL;   // 30 segundos

        static const char* textoMotivoReset() {
            switch (esp_reset_reason()) {
                case ESP_RST_POWERON:   return "POWERON_RESET";
                case ESP_RST_SW:        return "SW_RESTART";
                case ESP_RST_PANIC:     return "EXCEPTION_PANIC";
                case ESP_RST_INT_WDT:   return "INT_WDT";
                case ESP_RST_TASK_WDT:  return "TASK_WDT";
                case ESP_RST_WDT:       return "OTHER_WDT";
                case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
                case ESP_RST_BROWNOUT:  return "BROWNOUT";
                default:                return "UNKNOWN_RESET";
            }
        }

        static void clasificarEventoSensor(
            EstadoSensor estadoNuevo,
            TipoEvento& outTipo,
            CriticidadEvento& outCriticidad
        ) {
            if (estadoNuevo == EstadoSensor::OK) {
                outTipo       = TipoEvento::INFORMATIVO;
                outCriticidad = CriticidadEvento::BAJA;
            } else if (estadoNuevo == EstadoSensor::ALARMA_SUPERIOR ||
                       estadoNuevo == EstadoSensor::ALARMA_INFERIOR) {
                outTipo       = TipoEvento::ADVERTENCIA;
                outCriticidad = CriticidadEvento::ALTA;
            } else if (estadoNuevo == EstadoSensor::CORTOCIRCUITO_VDD ||
                       estadoNuevo == EstadoSensor::CORTOCIRCUITO_GND ||
                       estadoNuevo == EstadoSensor::CIRCUITO_ABIERTO) {
                outTipo       = TipoEvento::EXCEPCION;
                outCriticidad = CriticidadEvento::ALTA;
            } else {
                outTipo       = TipoEvento::EXCEPCION;
                outCriticidad = CriticidadEvento::MEDIA;
            }
        }

        static void procesarColaSonidos() {
            // Si el buzzer ya está reproduciendo un tono de evento, espera al próximo ciclo para no solapar
            if (buzz.buzzEstaEncendido() && !buzz.hayAlarmaFisicaActiva()) {
                return;
            }

            TipoSonidoEvento ev = TipoSonidoEvento::NINGUNO;
            if (!colaSonidos.recibir(ev, 0)) {
                return;
            }

            switch (ev) {
                case TipoSonidoEvento::BOOT_SISTEMA:
                    buzz.sonarBuzz(&sonidoBoot, 0.45f);
                    break;
                case TipoSonidoEvento::CLIENTE_CONECTADO:
                    buzz.sonarBuzz(&sonidoClienteConectado, 0.26f);
                    break;
                case TipoSonidoEvento::CLIENTE_DESCONECTADO:
                    buzz.sonarBuzz(&sonidoClienteDesconectado, 0.26f);
                    break;
                case TipoSonidoEvento::SENSOR_CONECTADO:
                    buzz.sonarBuzz(&sonidoSensorConectado, 0.22f);
                    break;
                case TipoSonidoEvento::SENSOR_DESCONECTADO:
                    buzz.sonarBuzz(&sonidoSensorDesconectado, 0.30f);
                    break;
                case TipoSonidoEvento::TAREA_RPC_EJECUTADA:
                    buzz.sonarBuzz(&sonidoMuteAck, 0.08f);
                    break;
                default:
                    break;
            }
        }

        static void persistirLoteSensos() {
            using namespace DbRuntime::Repositories;
            static SnapshotSensor lista[GestorSensores::MAX_SENSORES];
            uint8_t cant = gestorSensores.obtenerSensoresConectados(lista);
            if (cant == 0) return;

            TransaccionBatch tx;
            const int64_t tsAhora = static_cast<int64_t>(RelojSistema::obtenerTimestampActual());
            for (uint8_t i = 0; i < cant; ++i) {
                int32_t idSensor = lista[i].idSensorDB;
                if (idSensor <= 0 && strlen(lista[i].mac) == 16) {
                    idSensor = RepoSensor::registrarOAsegurarPorMac(lista[i].mac).idSensor;
                }
                if (idSensor <= 0) continue;

                const bool lecturaValida = estadoTieneLecturaValida(lista[i].estado);
                RepoSenso::registrarLectura(
                    idSensor,
                    lecturaValida ? Sensor::limitarTemp(lista[i].tempCruda)     : 0.0f,
                    lecturaValida ? Sensor::limitarTemp(lista[i].tempCorregida) : 0.0f,
                    lista[i].estado,
                    tsAhora
                );
            }
            tx.confirmar();
        }

        static void persistirCambioSensor(const RegistroPersistenciaPOD& reg) {
            using namespace DbRuntime::Repositories;
            int32_t idSensor = reg.idRelacionado;
            if (idSensor <= 0 && strlen(reg.origenOClave) == 16) {
                idSensor = RepoSensor::registrarOAsegurarPorMac(reg.origenOClave).idSensor;
            }
            if (idSensor <= 0) return;

            const EstadoSensor estadoPrevio = static_cast<EstadoSensor>(reg.estadoPrevio);
            const EstadoSensor estadoNuevo  = static_cast<EstadoSensor>(reg.estadoNuevo);
            const bool valida               = estadoTieneLecturaValida(estadoNuevo);
            const int64_t tsEvento          = (reg.timestampUnix > 0)
                ? static_cast<int64_t>(reg.timestampUnix)
                : static_cast<int64_t>(RelojSistema::obtenerTimestampActual());

            SensoDTO s = RepoSenso::registrarLectura(
                idSensor,
                valida ? Sensor::limitarTemp(reg.tempCruda)     : 0.0f,
                valida ? Sensor::limitarTemp(reg.tempCorregida) : 0.0f,
                estadoNuevo,
                tsEvento
            );

            TipoEvento tipo{};
            CriticidadEvento criticidad{};
            clasificarEventoSensor(estadoNuevo, tipo, criticidad);

            RepoAuditoriaSensores::registrarEvento(
                idSensor,
                s.idSenso,
                estadoPrevio,
                estadoNuevo,
                gestorSensores.obtenerEstadoBus(),
                tipo,
                criticidad
            );
        }

        static void persistirEventoSistemaBus(const RegistroPersistenciaPOD& reg) {
            using namespace DbRuntime::Repositories;
            static AuditoriaSistemaDTO a{};
            a = AuditoriaSistemaDTO{};
            a.estadoBus = static_cast<EstadoBus>(reg.estadoNuevo);
            a.detalle   = (reg.detalle[0] != '\0')
                ? reg.detalle
                : GestorSensores::nombreEstadoBus(a.estadoBus);

            if (a.estadoBus == EstadoBus::OK) {
                a.tipoEventoSistema = TipoEvento::INFORMATIVO;
                a.criticidad        = CriticidadEvento::BAJA;
            } else if (a.estadoBus == EstadoBus::TOPOLOGIA_MODIFICADA) {
                a.tipoEventoSistema = TipoEvento::ADVERTENCIA;
                a.criticidad        = CriticidadEvento::MEDIA;
            } else {
                a.tipoEventoSistema = TipoEvento::EXCEPCION;
                a.criticidad        = CriticidadEvento::ALTA;
            }

            a.espacioSdLibreKb = 0;
            a.heapLibreBytes   = ESP.getFreeHeap();
            a.resetReason      = textoMotivoReset();
            a.versionFirmware  = "1.0.0";
            a.internet         = (WiFi.status() == WL_CONNECTED);
            a.estadoReporte    = EstadoEnvioCorreo::PENDIENTE;
            a.bootId           = 1;
            a.uptimeS          = static_cast<uint32_t>(millis() / 1000UL);
            a.fuenteTiempo     = RelojSistema::estaSincronizado()
                ? FuenteTiempo::NTP
                : FuenteTiempo::SIN_SINCRONIZAR;

            RepoAuditoriaSistema::registrarEvento(a);
        }

        static void persistirAuditoriaAuth(const RegistroPersistenciaPOD& reg) {
            using namespace DbRuntime::Repositories;
            static AuditoriaAuthDTO a{};
            a = AuditoriaAuthDTO{};
            a.idUsuario        = (reg.idRelacionado > 0) ? reg.idRelacionado : 0;
            a.usuarioIntentado = reg.usuarioIntentado;
            a.origenRemoto     = (reg.origenOClave[0] != '\0') ? reg.origenOClave : "LOCAL";
            a.accion           = (reg.detalle[0] != '\0')      ? reg.detalle      : "EVENTO_SISTEMA";
            a.detalle          = reg.detalle;

            if (reg.tipo == TipoRegistroDB::AUDITORIA_RED) {
                if (strncmp(reg.origenOClave, "UART", 4) == 0) {
                    a.medioConexion = MedioConexion::UART_S;
                } else if (strncmp(reg.origenOClave, "192.168.4.", 10) == 0) {
                    a.medioConexion = MedioConexion::WIFI_AP;
                } else {
                    a.medioConexion = MedioConexion::WIFI_STA;
                }
            } else {
                a.medioConexion = MedioConexion::INTERNO;
            }

            a.resultado     = static_cast<ResultadoAuditoria>(reg.resultadoAuditoria);
            a.criticidad    = static_cast<CriticidadEvento>(reg.criticidadAuditoria);
            a.bootId        = 1;
            a.uptimeS       = static_cast<uint32_t>(millis() / 1000UL);
            a.fuenteTiempo  = RelojSistema::estaSincronizado()
                ? FuenteTiempo::NTP
                : FuenteTiempo::SIN_SINCRONIZAR;
            a.fechaCreacion = (reg.timestampUnix > 0)
                ? static_cast<int64_t>(reg.timestampUnix)
                : static_cast<int64_t>(RelojSistema::obtenerTimestampActual());

            RepoAuditoriaAuth::registrarEvento(a);
        }

        static void procesarColaPersistenciaDB() {
            static RegistroPersistenciaPOD reg{};
            // Procesa máximo 2 registros por ciclo para nunca retener el mutex de la MicroSD frente a un Login RPC
            for (uint8_t n = 0; n < 2 && colaPersistenciaDB.recibir(reg, 0); ++n) {
                try {
                    switch (reg.tipo) {
                        case TipoRegistroDB::LOTE_SENSOS_2_MINUTOS:
                            persistirLoteSensos();
                            break;
                        case TipoRegistroDB::EVENTO_CAMBIO_SENSOR:
                            persistirCambioSensor(reg);
                            break;
                        case TipoRegistroDB::EVENTO_SISTEMA_BUS:
                            persistirEventoSistemaBus(reg);
                            break;
                        case TipoRegistroDB::AUDITORIA_TRANSACCION:
                        case TipoRegistroDB::AUDITORIA_RED:
                            persistirAuditoriaAuth(reg);
                            break;
                    }
                } catch (...) {}
            }
        }

        static void ejecutarMantenimientoPeriodicoDB() {
            using namespace DbRuntime::Repositories;

            const uint32_t ahoraMs = millis();
            if (ahoraMs - ultimoMantenimientoMs < INTERVALO_MANTENIMIENTO_MS) {
                return;
            }
            ultimoMantenimientoMs = ahoraMs;

            if (!RelojSistema::estaSincronizado()) {
                return;
            }

            try {
                static ConfiguracionSistemaDTO cfg{};
                if (!RepoConfiguracionSistema::obtenerOInicializar(cfg) || !cfg.activa) {
                    return;
                }

                const uint32_t ahoraEpoch = RelojSistema::obtenerTimestampActual();

                if (cfg.retencionSensoDias > 0) {
                    const int64_t limiteSenso = static_cast<int64_t>(ahoraEpoch) - (static_cast<int64_t>(cfg.retencionSensoDias) * 86400LL);
                    RepoSenso::purgarAnterioresA(limiteSenso);
                }

                if (cfg.retencionEventosDias > 0) {
                    const uint32_t limiteEvEpoch = ahoraEpoch - (cfg.retencionEventosDias * 86400UL);
                    const PgDateTime fechaIsoEv  = RelojSistema::desdeTimestampUnix(limiteEvEpoch);
                    RepoAuditoriaSensores::purgarAnterioresA(fechaIsoEv.c_str());
                    RepoAuditoriaSistema::purgarAnterioresA(fechaIsoEv.c_str());
                }

                if (cfg.retencionAuditoriaDias > 0) {
                    const int64_t limiteAud = static_cast<int64_t>(ahoraEpoch) - (static_cast<int64_t>(cfg.retencionAuditoriaDias) * 86400LL);
                    RepoAuditoriaAuth::purgarAnterioresA(limiteAud);
                }
            } catch (...) {}
        }

        static void ejecutarCiclo(void*) {
            procesarTrabajoRpcEnCore1();

            const uint32_t ahoraMs = millis();

            if (ahoraMs - ultimoStreamingMs >= 500) {
                ultimoStreamingMs = ahoraMs;
                if (refL3) {
                    refL3->emitirStreamsActivos();
                }
            }

            if (ahoraMs - ultimaExpSesionMs >= 500) {
                ultimaExpSesionMs = ahoraMs;
                if (refL2) {
                    refL2->verificarExpiracionSesionesPorOcio();
                }
            }

            if (ahoraMs - ultimoCicloAlertasMs >= 100) {
                ultimoCicloAlertasMs = ahoraMs;

                EstadoSensor estados[GestorSensores::MAX_SENSORES];
                EstadoBus estadoBus = EstadoBus::OK;
                gestorSensores.obtenerEstadosParaAlarma(estados, estadoBus);
                buzz.actualizarAlarmaDesdeSensores(estados, estadoBus);

                procesarColaSonidos();
                procesarColaPersistenciaDB();
                ejecutarMantenimientoPeriodicoDB();
            }
        }

    public:
        static bool iniciar(
            ColaTrabajoRPC& colaRpc,
            MiddlewareSesion& l2,
            MiddlewareMessage& l3
        ) {
            refColaRpc           = &colaRpc;
            refL2                = &l2;
            refL3                = &l3;
            ultimoMantenimientoMs = millis();
            ultimoChequeoCorreoMs = millis();
            ultimoCicloAlertasMs  = millis();
            ultimoStreamingMs     = millis();
            ultimaExpSesionMs     = millis();
            encolarSonido(TipoSonidoEvento::BOOT_SISTEMA);

            // Core 1, prioridad 2, stack 8 KB: dueño exclusivo de MicroSD, ORM y RPC L2/L3
            return tarea.iniciarEnNucleo("TaskAlertasDB", &ejecutarCiclo, 5, 8192, 2, 1);
        }

        static TareaSegura& obtenerTarea() { return tarea; }
    };

} // namespace RTOS::Tasks