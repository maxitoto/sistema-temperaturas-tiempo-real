#ifndef RP_SISTEMA_H
#define RP_SISTEMA_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../middlewares/interfaces/WiFi/WiFiBase/WiFiInicializador.hpp"
#include "../../../../hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "../../../../hardware/driver/led/Led.hpp"
#include "../../../../hardware/driver/buzz/Buzz.hpp"
#include "../../../../hardware/driver/oled/Oled.hpp"
#include "../../../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../../../rtos/PrimitivasRTOS.hpp"
#include "../../../../rtos/tasks/TaskBiosRTOS.hpp"

// ============================================================================
// 1. DTOs COMPATIBLES EXISTENTES
// ============================================================================

struct ConfiguracionWiFiDTO {
    char ssidSta[32];   // 0..31
    char claveSta[64];  // 32..95
    char ssidAp[32];    // 96..127
    char claveAp[64];   // 128..191
};
static_assert(sizeof(ConfiguracionWiFiDTO) == 192, "ConfiguracionWiFiDTO debe medir 192 bytes");

struct ResConfiguracionWiFiDTO {
    char ssidSta[32];          // 0..31
    char ssidAp[32];           // 32..63
    char ipSta[16];            // 64..79
    char ipAp[16];             // 80..95
    uint8_t conectadoARouter;  // 96
    uint8_t puntoAccesoActivo; // 97
};
static_assert(sizeof(ResConfiguracionWiFiDTO) == 98, "ResConfiguracionWiFiDTO debe medir 98 bytes");

struct DiagnosticoSistemaDTO {
    uint32_t heapLibreBytes;
    uint32_t uptimeSegundos;
};

// ============================================================================
// 2. DTOs PARA NO_ADMIN (Red sin claves, MicroSD, Alarmas activas)
// ============================================================================

struct InfoRedPublicaDTO {
    PgChar<16> ipSta;
    PgChar<16> ipAp;
    PgBool     conectadoARouter;
    PgBool     puntoAccesoActivo;
    PgInt      rssiDbm;
    PgUInt8    clientesAutenticados;
};

struct EspacioMicroSdDTO {
    PgPosInt totalKb;
    PgPosInt usadoKb;
    PgPosInt libreKb;
    PgBool   inicializada;
};

struct SensorEnAlarmaItemDTO {
    PgUInt8      slot;
    PgAuto       idSensor;
    PgChar<17>   mac;
    PgFloat      temperaturaCorregida;
    PgFloat      umbralSuperior;
    PgFloat      umbralInferior;
    EstadoSensor estado;
};

struct EstadoAlarmasActivasDTO {
    PgBool       hayAlarmaActiva;
    PgBool       bocinasonando;
    PgBool       ledAlarmaEncendido;
    EstadoBus    estadoBus;
    PgUInt8      totalSensoresEnAlarma;
    SensorEnAlarmaItemDTO sensoresAfectados[GestorSensores::MAX_SENSORES];
};

// ============================================================================
// 3. DTOs PARA ADMIN (Sistema ESP32 nativo, Tareas RTOS, Hardware y help.log)
// ============================================================================

struct InfoCompletaEspDTO {
    PgPosInt   heapLibreBytes;
    PgPosInt   heapMinimoHistoricoBytes;
    PgPosInt   heapMaxBloqueBytes;
    PgPosInt   heapTotalBytes;
    PgPosInt   frecuenciaCpuMhz;
    PgPosInt   flashTamanoBytes;
    PgPosInt   uptimeSegundos;
    PgPosInt   incidentesBios;
    PgUInt8    revisionChip;
    PgChar<24> modeloChip;
    PgChar<24> motivoUltimoReset;
};

struct TelemetriaTareaRtosDTO {
    PgChar<16>    nombre;
    PgInt8        nucleo;
    PgUInt8       prioridad;
    PgPosInt      periodoMs;
    PgPosInt      stackReservadoBytes;
    PgPosInt      stackLibreMinBytes;
    PgPosInt      ciclosEjecutados;
    PgPosInt      maxTiempoEjecucionUs;
    PgPosInt      msDesdeUltimoLatido;
    PgPosSmallInt reiniciosBios;
    PgBool        activa;
    PgBool        enEjecucionPaso;
};

using ListaTareasRtosDTO = PgList<TelemetriaTareaRtosDTO, 6>;

struct EstadoHardwareDirectoDTO {
    PgBool    sdInicializada;
    PgBool    oledInicializada;
    PgBool    ledEncendido;
    PgBool    buzzerEncendido;
    PgBool    alarmaFisicaActiva;
    EstadoBus estadoBus1Wire;
    PgUInt8   sensoresDetectadosBus;
    PgUInt8   sensoresAsignados;
};

enum class AccionHardwareDirecto : uint8_t {
    NINGUNA                = 0,
    LED_ENCENDER           = 1,
    LED_APAGAR             = 2,
    BUZZ_SILENCIAR_ALARMA  = 3,
    BUZZ_TONO_PRUEBA       = 4,
    OLED_PUBLICAR_MENSAJE  = 5,
    OLED_LIMPIAR           = 6,
    SENSORES_REESCANEAR    = 7
};

struct ComandoHardwareDirectoDTO {
    AccionHardwareDirecto accion;
    PgPosSmallInt         frecuenciaHz;
    PgPosSmallInt         duracionMs;
    PgChar<22>            lineaOled1;
    PgChar<22>            lineaOled2;
};

struct ReporteHelpLogBiosDTO {
    PgPosInt    incidentesTotalesBios;
    PgChar<512> ultimosRegistrosLog;
};

struct ResApagadoSeguroDTO {
    PgBool     apagadoIniciado;
    PgBool     alarmaSilenciada;
    PgBool     ledApagado;
    PgBool     tareaDeCierreLanzada;
    PgChar<48> motivo;
};

namespace RPSistema {

    inline const char* obtenerTextoMotivoReset() {
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

    inline TelemetriaTareaRtosDTO empaquetarTarea(const RTOS::TareaSegura& t) {
        TelemetriaTareaRtosDTO dto{};
        dto.nombre               = t.obtenerNombre();
        dto.nucleo               = t.obtenerNucleo();
        dto.prioridad            = t.obtenerPrioridad();
        dto.periodoMs            = t.obtenerPeriodoMs();
        dto.stackReservadoBytes  = t.obtenerStackTotal();
        dto.stackLibreMinBytes   = t.obtenerStackLibreMin();
        dto.ciclosEjecutados     = t.obtenerCiclos();
        dto.maxTiempoEjecucionUs = t.obtenerMaxTiempoUs();
        dto.msDesdeUltimoLatido  = millis() - t.obtenerUltimoLatidoMs();
        dto.reiniciosBios        = t.obtenerReiniciosBios();
        dto.activa               = t.estaActiva();
        dto.enEjecucionPaso      = t.estaEjecutandoPaso();
        return dto;
    }

    inline EstadoHardwareDirectoDTO construirEstadoHardware() {
        EstadoHardwareDirectoDTO out{};
        out.sdInicializada        = TarjetaSD::getInstance().estaInicializada();
        out.oledInicializada      = oled.estaInicializada();
        out.ledEncendido          = led.ledEstaEncendido();
        out.buzzerEncendido       = buzz.buzzEstaEncendido();
        out.alarmaFisicaActiva    = buzz.hayAlarmaFisicaActiva();
        out.estadoBus1Wire        = gestorSensores.obtenerEstadoBus();
        out.sensoresDetectadosBus = gestorSensores.obtenerTotalEnBus();
        out.sensoresAsignados     = gestorSensores.obtenerTotalAsignados();
        return out;
    }

    // ========================================================================
    // PROCEDIMIENTOS PARA NO_ADMIN (Consultas operativas sin datos sensibles)
    // ========================================================================

    // 1. Información de red pública (solo IPs y estado, sin SSID ni contraseñas)
    inline auto obtenerInfoRedPublica = llamadoSemantico.alMenosUnaVez<void, InfoRedPublicaDTO>(
        "obtenerInfoRedPublica",
        []() -> InfoRedPublicaDTO {
            InfoRedPublicaDTO out{};
            if (WiFiInicializador::instanciaActiva != nullptr) {
                const EstadoRedWiFi& est = WiFiInicializador::instanciaActiva->obtenerEstadoRed();
                out.ipSta             = est.ipEstacionLAN;
                out.ipAp              = est.ipPuntoAcceso;
                out.conectadoARouter  = est.conectadoARouter;
                out.puntoAccesoActivo = est.puntoAccesoActivo;
            }
            out.rssiDbm = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
            uint8_t authCount = 0;
            for (uint8_t s = 0; s <= 8; ++s) {
                if (RPAuth::estaClienteAutenticado(s)) authCount++;
            }
            out.clientesAutenticados = authCount;
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 2. Espacio de la tarjeta MicroSD (Total, Usado y Libre en KB)
    inline auto obtenerEspacioMicroSD = llamadoSemantico.alMenosUnaVez<void, EspacioMicroSdDTO>(
        "obtenerEspacioMicroSD",
        []() -> EspacioMicroSdDTO {
            TarjetaSD& sd = TarjetaSD::getInstance();
            EspacioMicroSdDTO out{};
            out.inicializada = sd.estaInicializada();
            if (out.inicializada) {
                out.totalKb = static_cast<uint32_t>(sd.espacioTotal() / 1024ULL);
                out.usadoKb = static_cast<uint32_t>(sd.espacioUsado() / 1024ULL);
                out.libreKb = sd.espacioLibreKB();
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 3. Qué sensor o bus está produciendo la alarma activa
    inline auto obtenerEstadoAlarmasActivas = llamadoSemantico.alMenosUnaVez<void, EstadoAlarmasActivasDTO>(
        "obtenerEstadoAlarmasActivas",
        []() -> EstadoAlarmasActivasDTO {
            EstadoAlarmasActivasDTO out{};
            out.hayAlarmaActiva    = buzz.hayAlarmaFisicaActiva();
            out.bocinasonando      = buzz.buzzEstaEncendido();
            out.ledAlarmaEncendido = led.ledEstaEncendido();
            out.estadoBus          = gestorSensores.obtenerEstadoBus();

            SnapshotSensor conectados[GestorSensores::MAX_SENSORES];
            const uint8_t cant = gestorSensores.obtenerSensoresConectados(conectados);
            uint8_t enAlarma = 0;

            for (uint8_t i = 0; i < cant && enAlarma < GestorSensores::MAX_SENSORES; ++i) {
                if (conectados[i].estado != EstadoSensor::OK) {
                    SensorEnAlarmaItemDTO& item = out.sensoresAfectados[enAlarma++];
                    item.slot                 = conectados[i].slot;
                    item.idSensor             = conectados[i].idSensorDB;
                    item.mac                  = conectados[i].mac;
                    item.temperaturaCorregida = conectados[i].tempCorregida;
                    item.umbralSuperior       = conectados[i].umbral_sup;
                    item.umbralInferior       = conectados[i].umbral_inf;
                    item.estado               = conectados[i].estado;
                }
            }
            out.totalSensoresEnAlarma = enAlarma;
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // 4. Diagnóstico rápido de Heap y Uptime (permitido para NO_ADMIN y ADMIN)
    inline auto ejecutarDiagnosticoSistema = llamadoSemantico.alMenosUnaVez<void, DiagnosticoSistemaDTO>(
        "ejecutarDiagnosticoSistema",
        []() -> DiagnosticoSistemaDTO {
            return DiagnosticoSistemaDTO{
                ESP.getFreeHeap(),
                static_cast<uint32_t>(millis() / 1000UL)
            };
        },
        RolUsuario::NO_ADMIN
    );

    // ========================================================================
    // PROCEDIMIENTOS EXCLUSIVOS PARA ADMIN (Mute, ESP32, RTOS, Hardware, WiFi)
    // ========================================================================

    // 5. Mutear las alarmas activas (mantiene el LED encendido pero apaga la bocina)
    inline auto silenciarAlarmaActiva = llamadoSemantico.exactamenteUnaVez<void, EstadoHardwareDirectoDTO>(
        "silenciarAlarmaActiva",
        []() -> EstadoHardwareDirectoDTO {
            buzz.silenciarAlarmaActiva();
            const uint8_t slot = RPCBase::contextoActual().slotClienteOrigen;
            RTOS::encolarAuditoria(
                RTOS::TipoRegistroDB::AUDITORIA_TRANSACCION,
                RPAuth::obtenerIdUsuarioSlot(slot),
                RPAuth::obtenerNombreUsuarioSlot(slot),
                "HARDWARE",
                "MUTE_ALARMA_ACTIVA"
            );
            return construirEstadoHardware();
        },
        RolUsuario::ADMIN
    );

    // 6. Información completa del sistema usando primitivas nativas de ESP32
    inline auto obtenerInfoCompletaSistemaESP = llamadoSemantico.alMenosUnaVez<void, InfoCompletaEspDTO>(
        "obtenerInfoCompletaSistemaESP",
        []() -> InfoCompletaEspDTO {
            InfoCompletaEspDTO out{};
            out.heapLibreBytes           = ESP.getFreeHeap();
            out.heapMinimoHistoricoBytes = ESP.getMinFreeHeap();
            out.heapMaxBloqueBytes       = ESP.getMaxAllocHeap();
            out.heapTotalBytes           = ESP.getHeapSize();
            out.frecuenciaCpuMhz         = ESP.getCpuFreqMHz();
            out.flashTamanoBytes         = ESP.getFlashChipSize();
            out.uptimeSegundos           = static_cast<uint32_t>(millis() / 1000UL);
            out.incidentesBios           = RTOS::Tasks::TaskBiosRTOS::obtenerTotalIncidentes();
            out.revisionChip             = ESP.getChipRevision();
            out.modeloChip               = ESP.getChipModel();
            out.motivoUltimoReset        = obtenerTextoMotivoReset();
            return out;
        },
        RolUsuario::ADMIN
    );

    // 7. Telemetría completa de todas las tareas de FreeRTOS + BIOS
    inline auto obtenerEstadoTareasRTOS = llamadoSemantico.alMenosUnaVez<void, ListaTareasRtosDTO>(
        "obtenerEstadoTareasRTOS",
        []() -> ListaTareasRtosDTO {
            ListaTareasRtosDTO lista{};
            lista.agregar(empaquetarTarea(RTOS::Tasks::TaskBiosRTOS::obtenerTarea()));
            const size_t n = RTOS::Tasks::TaskBiosRTOS::obtenerCantidadVigiladas();
            for (size_t i = 0; i < n; ++i) {
                const RTOS::TareaSegura* t = RTOS::Tasks::TaskBiosRTOS::obtenerTareaVigilada(i);
                if (t != nullptr) {
                    lista.agregar(empaquetarTarea(*t));
                }
            }
            return lista;
        },
        RolUsuario::ADMIN
    );

    // 8. Consulta directa al estado de todos los drivers de hardware
    inline auto obtenerEstadoHardwareDirecto = llamadoSemantico.alMenosUnaVez<void, EstadoHardwareDirectoDTO>(
        "obtenerEstadoHardwareDirecto",
        []() -> EstadoHardwareDirectoDTO {
            return construirEstadoHardware();
        },
        RolUsuario::ADMIN
    );

    // 9. Control directo sobre periféricos (LED, Buzzer, OLED, Re-escaneo 1-Wire)
    inline auto controlarHardwareDirecto = llamadoSemantico.exactamenteUnaVez<ComandoHardwareDirectoDTO, EstadoHardwareDirectoDTO>(
        "controlarHardwareDirecto",
        [](const ComandoHardwareDirectoDTO& cmd) -> EstadoHardwareDirectoDTO {
            switch (cmd.accion) {
                case AccionHardwareDirecto::LED_ENCENDER:
                    led.encenderLed();
                    break;
                case AccionHardwareDirecto::LED_APAGAR:
                    led.apagarLed();
                    break;
                case AccionHardwareDirecto::BUZZ_SILENCIAR_ALARMA:
                    buzz.silenciarAlarmaActiva();
                    break;
                case AccionHardwareDirecto::BUZZ_TONO_PRUEBA: {
                    uint16_t hz  = (cmd.frecuenciaHz >= 100 && cmd.frecuenciaHz <= 5000) ? cmd.frecuenciaHz : 1200;
                    uint16_t dMs = (cmd.duracionMs >= 50 && cmd.duracionMs <= 2000) ? cmd.duracionMs : 200;
                    buzz.sonarBuzz( static_cast<int>(hz), static_cast<float>(dMs) / 1000.0f);
                    break;
                }
                case AccionHardwareDirecto::OLED_PUBLICAR_MENSAJE:
                    RTOS::pizarraOled.publicarPantalla("ADMIN-MSG", cmd.lineaOled1.c_str(), cmd.lineaOled2.c_str(), "", "");
                    RTOS::pizarraOled.volcarAlHardwareI2C();
                    break;
                case AccionHardwareDirecto::OLED_LIMPIAR:
                    oled.limpiarAreaContenido();
                    break;
                case AccionHardwareDirecto::SENSORES_REESCANEAR:
                    gestorSensores.escanearYAsignarSensores();
                    break;
                default:
                    break;
            }
            return construirEstadoHardware();
        },
        RolUsuario::ADMIN
    );

    // 10. Leer el archivo /db/help.log generado por la tarea BIOS/OS
    inline auto leerHelpLogBios = llamadoSemantico.alMenosUnaVez<void, ReporteHelpLogBiosDTO>(
        "leerHelpLogBios",
        []() -> ReporteHelpLogBiosDTO {
            ReporteHelpLogBiosDTO out{};
            out.incidentesTotalesBios = RTOS::Tasks::TaskBiosRTOS::obtenerTotalIncidentes();

            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada() || !sd.existeArchivo(RTOS::Tasks::TaskBiosRTOS::RUTA_HELP_LOG)) {
                out.ultimosRegistrosLog = "SIN_INCIDENTES_BIOS";
                return out;
            }

            auto lk = DbRuntime::Storage::RuntimeStorageAndIndexManager::lockEngine();
            File f = SD.open(RTOS::Tasks::TaskBiosRTOS::RUTA_HELP_LOG, FILE_READ);
            if (!f) {
                out.ultimosRegistrosLog = "ERROR_LECTURA_HELP_LOG";
                return out;
            }

            size_t size = f.size();
            if (size > 500) {
                f.seek(size - 500);
            }
            char buf[512]{};
            size_t leidos = f.readBytes(buf, sizeof(buf) - 1);
            buf[leidos] = '\0';
            f.close();

            out.ultimosRegistrosLog = buf;
            return out;
        },
        RolUsuario::ADMIN
    );

    // 11. Reiniciar el microcontrolador de forma remota (ADMIN)
    inline auto reiniciarHardware = llamadoSemantico.exactamenteUnaVez<void, void>(
        "reiniciarHardware",
        []() {
            xTaskCreate([](void*) {
                vTaskDelay(pdMS_TO_TICKS(250));
                ESP.restart();
            }, "RebootTask", 2048, nullptr, 5, nullptr);
        },
        RolUsuario::ADMIN
    );

    // 12. Actualizar credenciales WiFi STA/AP y reconectar (ADMIN)
    inline auto actualizarConfiguracionWiFi = llamadoSemantico.exactamenteUnaVez<ConfiguracionWiFiDTO, ResConfiguracionWiFiDTO>(
        "actualizarConfiguracionWiFi",
        [](const ConfiguracionWiFiDTO& nuevaConfig) -> ResConfiguracionWiFiDTO {
            if (nuevaConfig.ssidAp[0] == '\0') {
                throw std::invalid_argument("ValueError: el SSID del Punto de Acceso no puede estar vacio");
            }
            const size_t lenClaveAp = strnlen(nuevaConfig.claveAp, sizeof(nuevaConfig.claveAp));
            if (lenClaveAp > 0 && lenClaveAp < 8) {
                throw std::invalid_argument("ValueError: la clave WPA2 del AP requiere al menos 8 caracteres");
            }

            if (WiFiInicializador::instanciaActiva == nullptr ||
                !WiFiInicializador::instanciaActiva->actualizarCredencialesYReconectar(
                    nuevaConfig.ssidSta, nuevaConfig.claveSta, nuevaConfig.ssidAp, nuevaConfig.claveAp)) {
                throw std::runtime_error("RuntimeError: fallo al reiniciar la interfaz WiFi con las nuevas credenciales");
            }

            DbRuntime::Repositories::ConfiguracionRedDTO cfgDb{};
            if (!DbRuntime::Repositories::RepoConfiguracionRed::obtenerOInicializar(cfgDb)) {
                throw std::runtime_error("RuntimeError: no se pudo leer ConfiguracionRed desde la base de datos");
            }

            const EstadoRedWiFi& estado = WiFiInicializador::instanciaActiva->obtenerEstadoRed();
            ResConfiguracionWiFiDTO salida{};
            strncpy(salida.ssidSta, cfgDb.ssid_sta, sizeof(salida.ssidSta) - 1);
            strncpy(salida.ssidAp,  cfgDb.ssid_ap,  sizeof(salida.ssidAp) - 1);
            strncpy(salida.ipSta,   estado.ipEstacionLAN, sizeof(salida.ipSta) - 1);
            strncpy(salida.ipAp,    estado.ipPuntoAcceso, sizeof(salida.ipAp) - 1);
            salida.conectadoARouter  = estado.conectadoARouter ? 1 : 0;
            salida.puntoAccesoActivo = estado.puntoAccesoActivo ? 1 : 0;
            return salida;
        },
        RolUsuario::ADMIN
    );

    // 13. Obtener configuración WiFi completa incluyendo SSIDs (ADMIN)
    inline auto obtenerConfiguracionWiFi = llamadoSemantico.alMenosUnaVez<void, ResConfiguracionWiFiDTO>(
        "obtenerConfiguracionWiFi",
        []() -> ResConfiguracionWiFiDTO {
            DbRuntime::Repositories::ConfiguracionRedDTO cfgDb{};
            if (!DbRuntime::Repositories::RepoConfiguracionRed::obtenerOInicializar(cfgDb)) {
                throw std::runtime_error("RuntimeError: no se pudo leer ConfiguracionRed desde la base de datos");
            }

            ResConfiguracionWiFiDTO salida{};
            strncpy(salida.ssidSta, cfgDb.ssid_sta, sizeof(salida.ssidSta) - 1);
            strncpy(salida.ssidAp,  cfgDb.ssid_ap,  sizeof(salida.ssidAp) - 1);

            if (WiFiInicializador::instanciaActiva != nullptr) {
                const EstadoRedWiFi& estado = WiFiInicializador::instanciaActiva->obtenerEstadoRed();
                strncpy(salida.ipSta, estado.ipEstacionLAN, sizeof(salida.ipSta) - 1);
                strncpy(salida.ipAp,  estado.ipPuntoAcceso, sizeof(salida.ipAp) - 1);
                salida.conectadoARouter  = estado.conectadoARouter ? 1 : 0;
                salida.puntoAccesoActivo = estado.puntoAccesoActivo ? 1 : 0;
            }
            return salida;
        },
        RolUsuario::ADMIN
    );


    // ============================================================================
// 14. APAGADO SEGURO (SHUTDOWN CONTROLADO)
// ============================================================================
//  ¿Qué hace?:
//    - Silencia bocina y apaga LED de inmediato.
//    - Avisa por OLED y registra auditoría en SD.
//    - Lanza una tarea de bajo nivel que espera 800 ms (para que la respuesta
//      RPC salga por los canales activos), desconecta WiFi, cierra la SD
//      y entra en deep sleep (~10 µA, solo dominio RTC alimentado).
//  ¿Cómo se recupera el sistema?
//    - Reset físico (botón EN) o wake source configurado en RTC.
//    - Si tu placa tiene latch de potencia por GPIO, agregar la liberación
//      del GPIO antes del deep sleep (ver nota al final).
// ----------------------------------------------------------------------------

inline auto solicitarApagadoSeguro = llamadoSemantico.exactamenteUnaVez<void, ResApagadoSeguroDTO>(
    "solicitarApagadoSeguro",
    []() -> ResApagadoSeguroDTO {
        ResApagadoSeguroDTO out{};
        out.apagadoIniciado = true;
        out.motivo = "APAGADO_REMOTO_ADMIN";

        // 1. Apagar actuadores INMEDIATAMENTE (seguridad)
        buzz.silenciarAlarmaActiva();
        led.apagarLed();
        out.alarmaSilenciada = true;
        out.ledApagado       = true;

        // 2. Mostrar en OLED que el sistema se está apagando
        RTOS::pizarraOled.publicarPantalla("SISTEMA", "Apagando", "Espere...", "", "");
        RTOS::pizarraOled.volcarAlHardwareI2C();

        // 3. Auditoría (se escribirá a SD antes del cierre si hay margen)
        const uint8_t slot = RPCBase::contextoActual().slotClienteOrigen;
        RTOS::encolarAuditoria(
            RTOS::TipoRegistroDB::AUDITORIA_TRANSACCION,
            RPAuth::obtenerIdUsuarioSlot(slot),
            RPAuth::obtenerNombreUsuarioSlot(slot),
            "SISTEMA",
            "APAGADO_SEGURO_SOLICITADO"
        );

        // 4. Tarea de bajo nivel para cerrar ordenadamente
        xTaskCreate([](void*) {
            // Pausa para que la respuesta RPC salga por L1 / L2 / L3
            vTaskDelay(pdMS_TO_TICKS(800));

            // Desconectar WiFi de forma limpia (apaga la radio)
            WiFi.disconnect(true, true);

            // Cerrar la tarjeta SD de forma segura
            // AJUSTAR: usar el método real de TarjetaSD (cerrar(), close(), SD.end(), etc.)
            if (TarjetaSD::getInstance().estaInicializada()) {
                SD.end();
            }

            // Margen para que cualquier operación pendiente de flash/SD termine
            vTaskDelay(pdMS_TO_TICKS(300));

            // Apagar nuevamente actuadores por seguridad
            buzz.silenciarAlarmaActiva();
            led.apagarLed();
            oled.limpiarAreaContenido();

            vTaskDelay(pdMS_TO_TICKS(200));

            // Apagado lógico: deep sleep = mínimo consumo (~10 µA)
            // Despertar solo por reset físico o wake source RTC configurado
            esp_deep_sleep_start();
        }, "SafeShutdown", 4096, nullptr, 2, nullptr);

        out.tareaDeCierreLanzada = true;
        return out;
    },
    RolUsuario::ADMIN
);

} // namespace RPSistema

#endif