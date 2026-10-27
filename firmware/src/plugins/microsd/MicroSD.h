// Archivo: ./src/plugins/microsd/MicroSD.h
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "const/Pines.h"

// Handle opaco para operaciones de archivo (INVALID_HANDLE = -1)
typedef int8_t HandleSD;

class MicroSD {
  public:
    static constexpr HandleSD HANDLE_INVALIDO      = -1;
    static constexpr size_t   MAX_HANDLES_ABIERTOS = 4;
    static constexpr uint32_t BYTES_POR_SECTOR     = 512;
    static constexpr uint32_t FREC_SPI_RAPIDA_HZ   = 4000000UL;  // 4 MHz estable para módulos con level-shifter
    static constexpr uint32_t FREC_SPI_SEGURA_HZ   = 400000UL;   // 400 kHz fallback de inicialización MMC/SD

    enum ResultadoSD : uint8_t {
        OK = 0,
        ERR_NO_CARD,
        ERR_NO_MONTADO,
        ERR_ARCHIVO_NO_ENCONTRADO,
        ERR_YA_EXISTE,
        ERR_DEMASIADOS_ARCHIVOS,
        ERR_HANDLE_INVALIDO,
        ERR_IO_LECTURA,
        ERR_IO_ESCRITURA,
        ERR_SEEK_FALLIDO,
        ERR_DISCO_LLENO,
        ERR_PARAMETRO_INVALIDO
    };

    enum TipoTarjeta : uint8_t {
        TARJETA_NINGUNA = 0,
        TARJETA_MMC,
        TARJETA_SDSC,
        TARJETA_SDHC,
        TARJETA_DESCONOCIDA
    };

    enum EstadoTarjeta : uint8_t {
        ESTADO_NO_MONTADA = 0,
        ESTADO_LISTA,
        ESTADO_ERROR_IO
    };

    typedef bool (*CallbackDirectorio)(const char* nombre, bool esDirectorio, uint64_t tamanoBytes);

  private:
    struct SlotHandle {
        bool ocupado = false;
        bool modoEscritura = false;
        File archivo;
    };

    uint8_t pinCS;
    uint32_t frecRelojHz;
    bool montada;
    ResultadoSD codUltimoError;
    SlotHandle slots[MAX_HANDLES_ABIERTOS];

    int8_t reservarSlot() {
        for (size_t i = 0; i < MAX_HANDLES_ABIERTOS; i++) {
            if (!slots[i].ocupado) return (int8_t)i;
        }
        codUltimoError = ERR_DEMASIADOS_ARCHIVOS;
        return HANDLE_INVALIDO;
    }

    bool validarHandle(HandleSD h) {
        if (!montada) {
            codUltimoError = ERR_NO_MONTADO;
            return false;
        }
        if (h < 0 || (size_t)h >= MAX_HANDLES_ABIERTOS || !slots[h].ocupado || !slots[h].archivo) {
            codUltimoError = ERR_HANDLE_INVALIDO;
            return false;
        }
        return true;
    }

  public:
    explicit MicroSD(uint8_t cs = Pines::PIN_SD_CS, uint32_t relojHz = FREC_SPI_RAPIDA_HZ)
        : pinCS(cs), frecRelojHz(relojHz), montada(false), codUltimoError(OK) {}

    // =========================================================================
    // 2.1. CICLO DE VIDA Y MONTAJE (Puro ESP-WROOM-32 VSPI)
    // =========================================================================
    bool inicializar(int maxIntentos = 3) {
        SD.end();
        SPI.end();
        delay(20);

        // Asegurar CS en alto y pull-up en MISO antes de iniciar VSPI
        pinMode(pinCS, OUTPUT);
        digitalWrite(pinCS, HIGH);
        pinMode(Pines::PIN_SD_MISO, INPUT_PULLUP);
        delay(20);

        SPI.begin(Pines::PIN_SD_SCK, Pines::PIN_SD_MISO, Pines::PIN_SD_MOSI, pinCS);
        delay(20);

        for (int i = 0; i < maxIntentos; i++) {
            uint32_t clk = (i == maxIntentos - 1) ? FREC_SPI_SEGURA_HZ : frecRelojHz;

            if (SD.begin(pinCS, SPI, clk, "/sd", MAX_HANDLES_ABIERTOS)) {
                frecRelojHz = clk;
                montada = true;
                codUltimoError = OK;
                return true;
            }
            delay(150);
            yield();
        }
        montada = false;
        codUltimoError = ERR_NO_CARD;
        return false;
    }

    bool desmontar() {
        if (!montada) return true;
        sincronizarTodo();
        for (size_t i = 0; i < MAX_HANDLES_ABIERTOS; i++) {
            if (slots[i].ocupado) {
                slots[i].archivo.close();
                slots[i].ocupado = false;
            }
        }
        SD.end();
        montada = false;
        codUltimoError = OK;
        return true;
    }

    bool estaMontada() const {
        return montada;
    }

    // =========================================================================
    // 2.2. CONSULTA DE CAPACIDAD Y ESPACIO (Nativo ESP32 FatFS)
    // =========================================================================
    uint64_t bytesTotales() {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return 0; }
        return SD.totalBytes();
    }

    uint64_t bytesOcupados() {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return 0; }
        return SD.usedBytes();
    }

    uint64_t bytesLibres() {
        uint64_t tot = bytesTotales();
        uint64_t ocu = bytesOcupados();
        return (tot >= ocu) ? (tot - ocu) : 0;
    }

    uint32_t sectoresLibres() {
        return (uint32_t)(bytesLibres() / BYTES_POR_SECTOR);
    }

    // =========================================================================
    // 2.3. OPERACIONES DE ARCHIVO (HANDLE-BASED)
    // =========================================================================
    HandleSD abrirLectura(const char* path) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return HANDLE_INVALIDO; }
        if (!path)    { codUltimoError = ERR_PARAMETRO_INVALIDO; return HANDLE_INVALIDO; }

        int8_t idx = reservarSlot();
        if (idx == HANDLE_INVALIDO) return HANDLE_INVALIDO;

        File f = SD.open(path, FILE_READ);
        if (!f) {
            codUltimoError = ERR_ARCHIVO_NO_ENCONTRADO;
            return HANDLE_INVALIDO;
        }

        slots[idx].archivo = f;
        slots[idx].ocupado = true;
        slots[idx].modoEscritura = false;
        codUltimoError = OK;
        return idx;
    }

    HandleSD abrirEscritura(const char* path) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return HANDLE_INVALIDO; }
        if (!path)    { codUltimoError = ERR_PARAMETRO_INVALIDO; return HANDLE_INVALIDO; }

        int8_t idx = reservarSlot();
        if (idx == HANDLE_INVALIDO) return HANDLE_INVALIDO;

        File f = SD.exists(path) ? SD.open(path, FILE_APPEND) : SD.open(path, FILE_WRITE);
        if (!f) {
            codUltimoError = ERR_IO_ESCRITURA;
            return HANDLE_INVALIDO;
        }

        slots[idx].archivo = f;
        slots[idx].ocupado = true;
        slots[idx].modoEscritura = true;
        codUltimoError = OK;
        return idx;
    }

    HandleSD abrirLecturaYEscritura(const char* path) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return HANDLE_INVALIDO; }
        if (!path)    { codUltimoError = ERR_PARAMETRO_INVALIDO; return HANDLE_INVALIDO; }

        int8_t idx = reservarSlot();
        if (idx == HANDLE_INVALIDO) return HANDLE_INVALIDO;

        File f = SD.exists(path) ? SD.open(path, "r+") : SD.open(path, "w+");
        if (!f) {
            codUltimoError = ERR_ARCHIVO_NO_ENCONTRADO;
            return HANDLE_INVALIDO;
        }

        slots[idx].archivo = f;
        slots[idx].ocupado = true;
        slots[idx].modoEscritura = true;
        codUltimoError = OK;
        return idx;
    }

    HandleSD crearArchivo(const char* path) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return HANDLE_INVALIDO; }
        if (!path)    { codUltimoError = ERR_PARAMETRO_INVALIDO; return HANDLE_INVALIDO; }
        if (SD.exists(path)) {
            codUltimoError = ERR_YA_EXISTE;
            return HANDLE_INVALIDO;
        }

        int8_t idx = reservarSlot();
        if (idx == HANDLE_INVALIDO) return HANDLE_INVALIDO;

        File f = SD.open(path, FILE_WRITE);
        if (!f) {
            codUltimoError = ERR_IO_ESCRITURA;
            return HANDLE_INVALIDO;
        }

        slots[idx].archivo = f;
        slots[idx].ocupado = true;
        slots[idx].modoEscritura = true;
        codUltimoError = OK;
        return idx;
    }

    size_t leer(HandleSD h, uint8_t* buf, size_t n) {
        if (!validarHandle(h)) return 0;
        if (!buf || n == 0) { codUltimoError = ERR_PARAMETRO_INVALIDO; return 0; }

        size_t leidos = slots[h].archivo.read(buf, n);
        yield();
        codUltimoError = (leidos > 0) ? OK : ERR_IO_LECTURA;
        return leidos;
    }

    size_t escribir(HandleSD h, const uint8_t* buf, size_t n) {
        if (!validarHandle(h)) return 0;
        if (!buf || n == 0 || !slots[h].modoEscritura) {
            codUltimoError = ERR_PARAMETRO_INVALIDO;
            return 0;
        }

        size_t escritos = slots[h].archivo.write(buf, n);
        yield();
        codUltimoError = (escritos == n) ? OK : ERR_IO_ESCRITURA;
        return escritos;
    }

    bool buscar(HandleSD h, uint64_t pos) {
        if (!validarHandle(h)) return false;
        if (!slots[h].archivo.seek((size_t)pos)) {
            codUltimoError = ERR_SEEK_FALLIDO;
            return false;
        }
        codUltimoError = OK;
        return true;
    }

    uint64_t posicion(HandleSD h) {
        if (!validarHandle(h)) return 0;
        codUltimoError = OK;
        return (uint64_t)slots[h].archivo.position();
    }

    uint64_t tamanoArchivo(HandleSD h) {
        if (!validarHandle(h)) return 0;
        codUltimoError = OK;
        return (uint64_t)slots[h].archivo.size();
    }

    bool finDeArchivo(HandleSD h) {
        if (!validarHandle(h)) return true;
        return slots[h].archivo.available() <= 0;
    }

    bool sincronizar(HandleSD h) {
        if (!validarHandle(h)) return false;
        if (slots[h].modoEscritura) {
            slots[h].archivo.flush();
            yield();
        }
        codUltimoError = OK;
        return true;
    }

    bool cerrar(HandleSD h) {
        if (!validarHandle(h)) return false;
        slots[h].archivo.close();
        slots[h].ocupado = false;
        slots[h].modoEscritura = false;
        codUltimoError = OK;
        return true;
    }

    bool cerrarSeguro(HandleSD h) {
        if (!validarHandle(h)) return false;
        if (slots[h].modoEscritura) {
            slots[h].archivo.flush();
            yield();
        }
        slots[h].archivo.close();
        slots[h].ocupado = false;
        slots[h].modoEscritura = false;
        codUltimoError = OK;
        return true;
    }

    // =========================================================================
    // 2.4. GESTIÓN DE DIRECTORIOS Y ARCHIVOS (PATH-BASED)
    // =========================================================================
    bool crearDirectorio(const char* path) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return false; }
        if (!path)    { codUltimoError = ERR_PARAMETRO_INVALIDO; return false; }
        if (SD.exists(path)) { codUltimoError = OK; return true; }
        bool ok = SD.mkdir(path);
        codUltimoError = ok ? OK : ERR_IO_ESCRITURA;
        return ok;
    }

    bool eliminarArchivo(const char* path) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return false; }
        if (!path || !SD.exists(path)) { codUltimoError = ERR_ARCHIVO_NO_ENCONTRADO; return false; }
        bool ok = SD.remove(path);
        codUltimoError = ok ? OK : ERR_IO_ESCRITURA;
        return ok;
    }

    bool renombrar(const char* viejo, const char* nuevo) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return false; }
        if (!viejo || !nuevo || !SD.exists(viejo)) {
            codUltimoError = ERR_ARCHIVO_NO_ENCONTRADO;
            return false;
        }
        if (SD.exists(nuevo)) {
            SD.remove(nuevo);
        }
        bool ok = SD.rename(viejo, nuevo);
        codUltimoError = ok ? OK : ERR_IO_ESCRITURA;
        return ok;
    }

    bool existe(const char* path) {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return false; }
        if (!path)    { codUltimoError = ERR_PARAMETRO_INVALIDO; return false; }
        codUltimoError = OK;
        return SD.exists(path);
    }

    bool esDirectorio(const char* path) {
        if (!montada || !path) return false;
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        bool esDir = f.isDirectory();
        f.close();
        return esDir;
    }

    uint64_t tamanoArchivo(const char* path) {
        if (!montada || !path) return 0;
        File f = SD.open(path, FILE_READ);
        if (!f) {
            codUltimoError = ERR_ARCHIVO_NO_ENCONTRADO;
            return 0;
        }
        uint64_t tam = (uint64_t)f.size();
        f.close();
        codUltimoError = OK;
        return tam;
    }

    bool listarDirectorio(const char* path, CallbackDirectorio cb) {
        if (!montada || !path || !cb) return false;
        File dir = SD.open(path, FILE_READ);
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            codUltimoError = ERR_ARCHIVO_NO_ENCONTRADO;
            return false;
        }

        while (true) {
            File entrada = dir.openNextFile();
            if (!entrada) break;
            bool seguir = cb(entrada.name(), entrada.isDirectory(), (uint64_t)entrada.size());
            entrada.close();
            yield();
            if (!seguir) break;
        }
        dir.close();
        codUltimoError = OK;
        return true;
    }

    // =========================================================================
    // 2.5. METADATOS Y ESTADO DE LA TARJETA (DIAGNÓSTICO)
    // =========================================================================
    TipoTarjeta tipoTarjeta() const {
        if (!montada) return TARJETA_NINGUNA;
        uint8_t t = SD.cardType();
        if (t == CARD_MMC)  return TARJETA_MMC;
        if (t == CARD_SD)   return TARJETA_SDSC;
        if (t == CARD_SDHC) return TARJETA_SDHC;
        if (t == CARD_NONE) return TARJETA_NINGUNA;
        return TARJETA_DESCONOCIDA;
    }

    uint32_t velocidadReloj() const {
        return frecRelojHz;
    }

    EstadoTarjeta estadoTarjeta() const {
        if (!montada) return ESTADO_NO_MONTADA;
        return (codUltimoError == ERR_IO_LECTURA || codUltimoError == ERR_IO_ESCRITURA)
                 ? ESTADO_ERROR_IO
                 : ESTADO_LISTA;
    }

    ResultadoSD ultimoError() const {
        return codUltimoError;
    }

    // =========================================================================
    // 2.6. SINCRONIZACIÓN Y SEGURIDAD DE INTEGRIDAD GLOBAL
    // =========================================================================
    bool sincronizarTodo() {
        if (!montada) { codUltimoError = ERR_NO_MONTADO; return false; }
        for (size_t i = 0; i < MAX_HANDLES_ABIERTOS; i++) {
            if (slots[i].ocupado && slots[i].modoEscritura && slots[i].archivo) {
                slots[i].archivo.flush();
                yield();
            }
        }
        codUltimoError = OK;
        return true;
    }

    bool cerrarSeguro() {
        return desmontar();
    }
};