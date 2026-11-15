#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace RTOS {

    // =========================================================================
    // TIPO DE DATO: PgDate (Equivale a DATE en PostgreSQL: "YYYY-MM-DD")
    // Estructura plana (POD) de 11 bytes, sin memoria dinámica (Heap).
    // =========================================================================
    struct PgDate {
        char texto[11]; // "2026-11-13\0"

        PgDate() { strncpy(texto, "1970-01-01", sizeof(texto)); }
        explicit PgDate(const char* isoDate) {
            strncpy(texto, isoDate ? isoDate : "1970-01-01", sizeof(texto) - 1);
            texto[sizeof(texto) - 1] = '\0';
        }

        const char* c_str() const { return texto; }
        String toString() const { return String(texto); }
        operator String() const { return String(texto); }
    };

    // =========================================================================
    // TIPO DE DATO: PgDateTime (Equivale a TIMESTAMP en PostgreSQL)
    // Formato estándar: "YYYY-MM-DD HH:MM:SS" (o "YYYY-MM-DD HH:MM:SS-03")
    // Estructura plana (POD) de 24 bytes, sin memoria dinámica (Heap).
    // =========================================================================
    struct PgDateTime {
        char texto[24]; // "2026-11-13 14:35:20-03\0"

        PgDateTime() { strncpy(texto, "1970-01-01 00:00:00", sizeof(texto)); }
        explicit PgDateTime(const char* isoDateTime) {
            strncpy(texto, isoDateTime ? isoDateTime : "1970-01-01 00:00:00", sizeof(texto) - 1);
            texto[sizeof(texto) - 1] = '\0';
        }

        const char* c_str() const { return texto; }
        String toString() const { return String(texto); }
        operator String() const { return String(texto); }
    };

    class RelojSistema {
    private:
        inline static bool ntpConfigurado = false;
        inline static bool horaSincronizada = false;
        inline static uint32_t ultimoIntentoSyncMs = 0;
        // Época base de respaldo (2026-01-01 00:00:00 UTC) por si opera sin WiFi desde el arranque
        static constexpr time_t EPOCA_RESPALDO_2026 = 1767225600;

        static void obtenerTmActual(struct tm& outInfo) {
            if (horaSincronizada && getLocalTime(&outInfo, 5)) {
                return;
            }
            // Si aún no sincronizó NTP, avanza desde la época base usando millis()
            time_t t = EPOCA_RESPALDO_2026 + static_cast<time_t>(millis() / 1000UL);
            localtime_r(&t, &outInfo);
        }

    public:
        static constexpr long GMT_OFFSET_ARGENTINA_SEG = -3 * 3600; // UTC-3
        static constexpr int  DAYLIGHT_OFFSET_SEG      = 0;

        static void sincronizarNtpSiHayWiFi() {
            if (WiFi.status() != WL_CONNECTED) {
                return;
            }

            const uint32_t ahoraMs = millis();
            if (!ntpConfigurado || (!horaSincronizada && (ahoraMs - ultimoIntentoSyncMs >= 30000))) {
                ultimoIntentoSyncMs = ahoraMs;
                configTime(GMT_OFFSET_ARGENTINA_SEG, DAYLIGHT_OFFSET_SEG,
                           "pool.ntp.org", "time.nist.gov", "ar.pool.ntp.org");
                ntpConfigurado = true;
            }

            if (!horaSincronizada) {
                struct tm info{};
                if (getLocalTime(&info, 5)) {
                    if (info.tm_year + 1900 >= 2025) {
                        horaSincronizada = true;
                    }
                }
            }
        }

        static bool estaSincronizado() {
            return horaSincronizada;
        }

        // Devuelve el timestamp Unix en segundos
        static uint32_t obtenerTimestampActual() {
            if (horaSincronizada) {
                time_t now;
                time(&now);
                return static_cast<uint32_t>(now);
            }
            return static_cast<uint32_t>(EPOCA_RESPALDO_2026 + (millis() / 1000UL));
        }

        // 1. Para columnas DateField (PgDataType::DATE) -> "2026-11-13"
        static PgDate fechaActualPg() {
            struct tm info{};
            obtenerTmActual(info);
            PgDate out;
            strftime(out.texto, sizeof(out.texto), "%Y-%m-%d", &info);
            return out;
        }

        // 2. Para columnas DateTimeField sin TZ (PgDataType::TIMESTAMP) -> "2026-11-13 14:35:20"
        //    Ideal para campos "creado_el" y "actualizado_el"
        static PgDateTime fechaHoraActualPg() {
            struct tm info{};
            obtenerTmActual(info);
            PgDateTime out;
            strftime(out.texto, sizeof(out.texto), "%Y-%m-%d %H:%M:%S", &info);
            return out;
        }

        // 3. Para columnas DateTimeField con TZ (PgDataType::TIMESTAMP_TZ) -> "2026-11-13 14:35:20-03"
        static PgDateTime fechaHoraTzActualPg() {
            struct tm info{};
            obtenerTmActual(info);
            PgDateTime out;
            strftime(out.texto, sizeof(out.texto), "%Y-%m-%d %H:%M:%S-03", &info);
            return out;
        }

        // 4. Convierte cualquier timestamp Unix (uint32_t) a PgDateTime ("YYYY-MM-DD HH:MM:SS")
        static PgDateTime desdeTimestampUnix(uint32_t tsUnix) {
            time_t raw = static_cast<time_t>(tsUnix);
            struct tm info{};
            localtime_r(&raw, &info);
            PgDateTime out;
            strftime(out.texto, sizeof(out.texto), "%Y-%m-%d %H:%M:%S", &info);
            return out;
        }

        // 5. Formateador compacto de 10 caracteres exactos para la cabecera de la OLED (Cero warnings)
        static void formatearFechaHoraOled(char* outBuffer12Bytes, bool mostrarFecha) {
            if (!outBuffer12Bytes) return;
            if (horaSincronizada) {
                struct tm info{};
                if (getLocalTime(&info, 5)) {
                    strftime(outBuffer12Bytes, 12, mostrarFecha ? "%d/%m/%Y" : "%H:%M:%S", &info);
                    return;
                }
            }

            uint32_t seg = millis() / 1000UL;
            unsigned int hh = static_cast<unsigned int>((seg / 3600UL) % 24UL);
            unsigned int mm = static_cast<unsigned int>((seg / 60UL) % 60UL);
            unsigned int ss = static_cast<unsigned int>(seg % 60UL);
            snprintf(outBuffer12Bytes, 12, "T+%02u:%02u:%02u", hh & 0x1Fu, mm & 0x3Fu, ss & 0x3Fu);
        }
    };

} // namespace RTOS