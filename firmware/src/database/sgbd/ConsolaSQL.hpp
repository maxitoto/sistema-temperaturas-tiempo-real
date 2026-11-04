// Archivo: src/tests/SQLite/sgbd/ConsolaSQL.hpp
#pragma once
#include <Arduino.h>
#include <vector>
#include <functional>

// Descomentar SOLO en tests aislados donde no se use el protocolo OSI por USB:
// #define MODO_TEST_SERIAL 1

namespace SGBD {

    struct RespuestaSQL {
        bool exito = true;
        String sqlState = "00000";   // "00000" = SUCCESS en estándar PostgreSQL
        String comando = "";         // Ej: "CREATE TABLE", "SELECT", "INSERT"
        String mensaje = "";         // Mensaje de éxito o detalle del error
        String origen = "";          // Tabla o contexto donde ocurrió
        String sugerencia = "";      // Ayuda para corregir el error
        uint32_t timestampMs = 0;

        // Formatea la salida igual que una terminal interactiva psql para enviarla por RPC
        String formatearComoTerminal() const {
            if (exito) {
                return comando + " OK: " + mensaje;
            }
            String out = "ERROR [" + sqlState + "] (" + comando + " en " + origen + "): " + mensaje;
            if (!sugerencia.isEmpty()) {
                out += "\nHINT: " + sugerencia;
            }
            return out;
        }
    };

    class ConsolaSQL {
    public:
        using TransportCallback = std::function<void(const RespuestaSQL&)>;

    private:
        static constexpr size_t MAX_HISTORIAL = 12;
        RespuestaSQL ultimaRespuesta;
        std::vector<RespuestaSQL> historial;
        TransportCallback callbackRed = nullptr;
        bool modoTestSerial = false;

        ConsolaSQL() {
            #ifdef MODO_TEST_SERIAL
                modoTestSerial = true;
            #endif
        }

        void registrar(const RespuestaSQL& r) {
            ultimaRespuesta = r;
            if (historial.size() >= MAX_HISTORIAL) {
                historial.erase(historial.begin());
            }
            historial.push_back(r);

            // 1. Si hay un cliente/admin suscripto por RPC, le envía el paquete
            if (callbackRed) {
                callbackRed(r);
            }

            // 2. Silencio total en UART0, excepto si estamos explícitamente en dominio de test
            if (modoTestSerial) {
                Serial.println(r.formatearComoTerminal());
            }
        }

    public:
        static ConsolaSQL& getInstance() {
            static ConsolaSQL instance;
            return instance;
        }

        ConsolaSQL(const ConsolaSQL&) = delete;
        ConsolaSQL& operator=(const ConsolaSQL&) = delete;

        // Activar solo en tests unitarios locales
        void habilitarSerialParaTests(bool activo) {
            modoTestSerial = activo;
        }

        // Conecta la consola con tu capa de comunicación RPC cuando un Admin abre sesión remota
        void vincularTransporteRemoto(TransportCallback cb) {
            callbackRed = cb;
        }

        void desvincularTransporteRemoto() {
            callbackRed = nullptr;
        }

        void reportarError(const char* sqlState,
                           const char* nombreError,
                           const String& origen,
                           const String& detalle,
                           const String& sugerencia = "") {
            RespuestaSQL r;
            r.exito       = false;
            r.sqlState    = sqlState;
            r.comando     = nombreError;
            r.origen      = origen;
            r.mensaje     = detalle;
            r.sugerencia  = sugerencia;
            r.timestampMs = millis();
            registrar(r);
        }

        void reportarInfo(const String& comando, const String& detalle) {
            RespuestaSQL r;
            r.exito       = true;
            r.sqlState    = "00000";
            r.comando     = comando;
            r.origen      = "SGBD";
            r.mensaje     = detalle;
            r.timestampMs = millis();
            registrar(r);
        }

        // Devuelve el resultado de la última operación ejecutada
        const RespuestaSQL& obtenerUltimaRespuesta() const {
            return ultimaRespuesta;
        }

        String obtenerUltimoMensajeTerminal() const {
            return ultimaRespuesta.formatearComoTerminal();
        }

        const std::vector<RespuestaSQL>& obtenerHistorial() const {
            return historial;
        }

        void limpiarHistorial() {
            historial.clear();
        }
    };

}