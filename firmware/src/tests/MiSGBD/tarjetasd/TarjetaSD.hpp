#pragma once

#include <SD.h>
#include <SPI.h>

#include "../../../hardware/placa/esp32S/pines_nodemcu32s.hpp"

class TarjetaSD {
private:
    int FRECUENCIA_SPI = 4000000;
    bool inicializada = false;

    TarjetaSD();
    ~TarjetaSD() = default;

public:
    static TarjetaSD& getInstance() {
        static TarjetaSD instance;
        return instance;
    }

    TarjetaSD(const TarjetaSD&) = delete;
    TarjetaSD& operator=(const TarjetaSD&) = delete;
    TarjetaSD(TarjetaSD&&) = delete;
    TarjetaSD& operator=(TarjetaSD&&) = delete;

    bool estaInicializada() const;

    bool escribirArchivo(const char* ruta, const char* contenido);
    bool anexarArchivo(const char* ruta, const char* contenido);
    bool leerArchivo(const char* ruta, String& contenido);

    bool existeArchivo(const char* ruta);
    bool eliminarArchivo(const char* ruta);

    bool crearDirectorio(const char* ruta);
    bool eliminarDirectorio(const char* ruta);

    uint64_t espacioTotal();
    uint64_t espacioUsado();
};