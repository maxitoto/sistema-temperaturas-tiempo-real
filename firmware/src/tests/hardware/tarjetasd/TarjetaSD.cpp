#include "TarjetaSD.hpp"

TarjetaSD::TarjetaSD() {

    SPI.begin(
        Pin::PIN_SD_SCK,
        Pin::PIN_SD_MISO,
        Pin::PIN_SD_MOSI,
        Pin::PIN_SD_CS
    );

    inicializada = SD.begin(
        Pin::PIN_SD_CS,
        SPI,
        FRECUENCIA_SPI
    );
}

bool TarjetaSD::estaInicializada() const {
    return inicializada;
}

bool TarjetaSD::escribirArchivo(
    const char* ruta,
    const char* contenido
) {
    if (!inicializada || !ruta || !contenido) return false;

    if (SD.exists(ruta)) {
        if (!SD.remove(ruta)) return false;
    }

    File archivo = SD.open(ruta, FILE_WRITE);

    if (!archivo) return false;

    size_t escritos = archivo.print(contenido);

    archivo.close();

    return escritos == strlen(contenido);
}

bool TarjetaSD::anexarArchivo(
    const char* ruta,
    const char* contenido
) {
    if (!inicializada || !ruta || !contenido) return false;

    File archivo = SD.open(ruta, FILE_APPEND);

    if (!archivo) return false;

    size_t escritos = archivo.print(contenido);

    archivo.close();

    return escritos == strlen(contenido);
}

bool TarjetaSD::leerArchivo(
    const char* ruta,
    String& contenido
) {
    if (!inicializada || !ruta) return false;

    File archivo = SD.open(ruta, FILE_READ);

    if (!archivo || archivo.isDirectory()) return false;

    contenido = archivo.readString();

    archivo.close();

    return true;
}

bool TarjetaSD::existeArchivo(const char* ruta) {
    if (!inicializada || !ruta) return false;

    return SD.exists(ruta);
}

bool TarjetaSD::eliminarArchivo(const char* ruta) {
    if (!inicializada || !ruta) return false;

    return SD.remove(ruta);
}

bool TarjetaSD::crearDirectorio(const char* ruta) {
    if (!inicializada || !ruta) return false;

    return SD.mkdir(ruta);
}

bool TarjetaSD::eliminarDirectorio(const char* ruta) {
    if (!inicializada || !ruta) return false;

    return SD.rmdir(ruta);
}

uint64_t TarjetaSD::espacioTotal() {
    if (!inicializada) return 0;

    return SD.totalBytes();
}

uint64_t TarjetaSD::espacioUsado() {
    if (!inicializada) return 0;

    return SD.usedBytes();
}