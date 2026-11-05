#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include "../database/orm/ORM.hpp"
#include "../rtos/SystemBus.hpp"
#include "../connections/CommunicationContainer.hpp"

namespace Time {

class TimeService {
public:
    // Umbral mínimo para considerar que el reloj tiene fecha real (2024-01-01 00:00:00 UTC)
    static constexpr uint32_t MIN_VALID_EPOCH = 1704067200UL;
    // Zona horaria Argentina (UTC-3 = -10800 segundos)
    static constexpr long GMT_OFFSET_SEC = -3L * 3600L;

    static TimeService& instance() {
        static TimeService inst;
        return inst;
    }

    // Devuelve el timestamp Unix en segundos (o segundos desde boot si aún no sincronizó)
    uint32_t nowUnix() const {
        time_t now = 0;
        time(&now);
        if (static_cast<uint32_t>(now) >= MIN_VALID_EPOCH) {
            return static_cast<uint32_t>(now);
        }
        return millis() / 1000UL;
    }

    bool estaSincronizado() const {
        time_t now = 0;
        time(&now);
        return static_cast<uint32_t>(now) >= MIN_VALID_EPOCH;
    }

    // Ajusta el reloj interno del ESP32 desde un epoch Unix en segundos (o milisegundos si > 1e11)
    bool establecerEpoch(uint64_t epochIn) {
        uint32_t epochSec = (epochIn > 100000000000ULL)
                            ? static_cast<uint32_t>(epochIn / 1000ULL)
                            : static_cast<uint32_t>(epochIn);
        if (epochSec < MIN_VALID_EPOCH) return false;

        struct timeval tv;
        tv.tv_sec  = static_cast<time_t>(epochSec);
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
        sincronizado_ = true;
        esperandoCliente_ = false;
        ultimoSyncMs_ = millis();
        Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_SENSORES_ACTUALIZADOS);
        return true;
    }

    // Formatea la fecha actual "DD/MM/YYYY" para la cabecera de la OLED
    void formatearFechaActual(char* outBuf, size_t maxLen) const {
        if (!estaSincronizado()) {
            snprintf(outBuf, maxLen, "SIN HORA");
            return;
        }
        time_t raw = static_cast<time_t>(nowUnix() + GMT_OFFSET_SEC);
        struct tm ti;
        gmtime_r(&raw, &ti);
        snprintf(outBuf, maxLen, "%02d/%02d/%04d",
                 ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
    }

    // Siembra los 3 servidores NTP por defecto con prioridad 1, 2 y 3 si la tabla está vacía
    void inicializarServidoresPorDefecto() {
        auto lock = Rtos::sysBus.lockDb(3000);
        if (!lock.isLocked()) return;

        db.setSession("system_ntp", ROL_ADMIN);
        if (!SGBD::ModuloDQL::getInstance().existeRegistro("ServidoresNtp", "id_server", "1")) {
            uint32_t ts = nowUnix();
            db.from("ServidoresNtp").insert({
                {"id_server", "1"}, {"server", "time.google.com"},
                {"prioridad", "1"}, {"timeout_ms", "2500"},
                {"fallos_consecutivos", "0"}, {"ultimo_sync_exitoso", "0"},
                {"activa", "1"}, {"fecha_creacion", String(ts)}, {"fecha_modificacion", String(ts)}
            });
            db.from("ServidoresNtp").insert({
                {"id_server", "2"}, {"server", "pool.ntp.org"},
                {"prioridad", "2"}, {"timeout_ms", "2500"},
                {"fallos_consecutivos", "0"}, {"ultimo_sync_exitoso", "0"},
                {"activa", "1"}, {"fecha_creacion", String(ts)}, {"fecha_modificacion", String(ts)}
            });
            db.from("ServidoresNtp").insert({
                {"id_server", "3"}, {"server", "time.cloudflare.com"},
                {"prioridad", "3"}, {"timeout_ms", "3000"},
                {"fallos_consecutivos", "0"}, {"ultimo_sync_exitoso", "0"},
                {"activa", "1"}, {"fecha_creacion", String(ts)}, {"fecha_modificacion", String(ts)}
            });
        }
    }

    // Intenta sincronizar en orden de prioridad (1 -> 2 -> 3...) si hay WiFi STA conectado
    bool sincronizarConFailoverNtp() {
        if (WiFi.status() != WL_CONNECTED) {
            esperandoCliente_ = !estaSincronizado();
            return false;
        }

        std::vector<SGBD::Fila> servidores;
        {
            auto lock = Rtos::sysBus.lockDb(2500);
            if (lock.isLocked()) {
                db.setSession("system_ntp", ROL_ADMIN);
                servidores = db.from("ServidoresNtp")
                               .select("*")
                               .eq("activa", "1")
                               .orderBy("prioridad", SGBD::ASC)
                               .execute();
            }
        }

        if (servidores.empty()) return false;

        for (const auto& srv : servidores) {
            String idSrv = srv.at("id_server");
            String host  = srv.at("server");
            uint32_t timeoutMs = static_cast<uint32_t>(srv.at("timeout_ms").toInt());
            if (timeoutMs < 1000 || timeoutMs > 10000) timeoutMs = 2500;
            int fallosPrevios = srv.at("fallos_consecutivos").toInt();

            configTime(0, 0, host.c_str());

            uint32_t t0 = millis();
            bool exito = false;
            while (millis() - t0 < timeoutMs) {
                time_t now = 0;
                time(&now);
                if (static_cast<uint32_t>(now) >= MIN_VALID_EPOCH) {
                    exito = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            auto lock = Rtos::sysBus.lockDb(2000);
            if (exito) {
                sincronizado_ = true;
                esperandoCliente_ = false;
                ultimoSyncMs_ = millis();
                uint32_t ts = nowUnix();
                if (lock.isLocked()) {
                    db.setSession("system_ntp", ROL_ADMIN);
                    db.from("ServidoresNtp").eq("id_server", idSrv).update({
                        {"fallos_consecutivos", "0"},
                        {"ultimo_sync_exitoso", String(ts)},
                        {"fecha_modificacion",  String(ts)}
                    });
                }
                Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_SENSORES_ACTUALIZADOS);
                return true;
            } else if (lock.isLocked()) {
                db.setSession("system_ntp", ROL_ADMIN);
                db.from("ServidoresNtp").eq("id_server", idSrv).update({
                    {"fallos_consecutivos", String(fallosPrevios + 1)},
                    {"fecha_modificacion",  String(nowUnix())}
                });
            }
        }

        // Si ningún servidor NTP respondió, queda esperando en silencio a que se conecte un cliente
        esperandoCliente_ = !estaSincronizado();
        return false;
    }

    // Pide silenciosamente el timestamp al cliente recién conectado por RPC
    void solicitarHoraACliente(uint32_t clientId = 0) {
        if (estaSincronizado()) return;
        Connections::comm.c.call_async<uint64_t>(
            clientId,
            "client.get_timestamp",
            [this](uint64_t tsCliente) {
                this->establecerEpoch(tsCliente);
            }
        );
    }

private:
    bool sincronizado_ = false;
    bool esperandoCliente_ = true;
    uint32_t ultimoSyncMs_ = 0;

    TimeService() = default;
};

inline TimeService& timeSrv = TimeService::instance();
inline uint32_t nowUnix() { return timeSrv.nowUnix(); }

} // namespace Time