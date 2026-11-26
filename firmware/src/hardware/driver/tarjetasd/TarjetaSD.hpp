#pragma once

#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../placa/esp32S/pines_nodemcu32s.hpp"

class TarjetaSD {
private:
    static constexpr int FRECUENCIA_SPI = 4000000;
    bool inicializada = false;
    SemaphoreHandle_t mutexSd = nullptr;
    uint32_t cacheTotalKb = 0;
    uint32_t cacheUsadoKb = 0;
    uint32_t ultimoCalculoEspacioMs = 0;

    TarjetaSD();
    ~TarjetaSD() = default;

    public:
    // Guardia RAII no copiable para bloquear de forma segura transacciones de Base de Datos / MicroSD
        class ScopedLock {
        private:
            SemaphoreHandle_t mtx;
        public:
            explicit ScopedLock(SemaphoreHandle_t m) : mtx(m) {
                if (mtx) xSemaphoreTakeRecursive(mtx, portMAX_DELAY);
            }
            ScopedLock(const ScopedLock&) = delete;
            ScopedLock& operator=(const ScopedLock&) = delete;

            ScopedLock(ScopedLock&& other) noexcept : mtx(other.mtx) {
                other.mtx = nullptr;
            }
            ScopedLock& operator=(ScopedLock&& other) noexcept {
                if (this != &other) {
                    if (mtx) xSemaphoreGiveRecursive(mtx);
                    mtx = other.mtx;
                    other.mtx = nullptr;
                }
                return *this;
            }
            ~ScopedLock() {
                if (mtx) xSemaphoreGiveRecursive(mtx);
            }
        };

    static TarjetaSD& getInstance() {
        static TarjetaSD instance;
        return instance;
    }

    TarjetaSD(const TarjetaSD&) = delete;
    TarjetaSD& operator=(const TarjetaSD&) = delete;
    TarjetaSD(TarjetaSD&&) = delete;
    TarjetaSD& operator=(TarjetaSD&&) = delete;

    bool iniciar();
    bool estaInicializada() const;
    SemaphoreHandle_t obtenerMutex() const { return mutexSd; }

    bool escribirArchivo(const char* ruta, const char* contenido);
    bool anexarArchivo(const char* ruta, const char* contenido);
    bool leerArchivo(const char* ruta, String& contenido);

    bool existeArchivo(const char* ruta);
    bool eliminarArchivo(const char* ruta);

    bool crearDirectorio(const char* ruta);
    bool eliminarDirectorio(const char* ruta);

    uint64_t espacioTotal();
    uint64_t espacioUsado();
    uint32_t espacioLibreKB();
};

inline TarjetaSD& tarjetaSD = TarjetaSD::getInstance();