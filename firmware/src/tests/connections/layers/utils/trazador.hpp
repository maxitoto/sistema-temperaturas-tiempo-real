#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "../common/BufferRed.hpp"

namespace OSI {
namespace Debug {

// Etiqueta opcional de una región/capa de la trama.
// offset medido SIEMPRE desde el byte 0 físico de la trama.
struct RegionTrama {
    const char* nombre;
    size_t      offset;
    size_t      longitud;
};

namespace detail {

// ¡OJO! No usar el nombre HEX: es un macro de Arduino (#define HEX 16).
inline const char* hexChars() { return "0123456789ABCDEF"; }

inline void hexLinea(const uint8_t* datos, size_t n, size_t offsetBase, Print& out) {
    static const char* H = hexChars();
    char linea[96];
    int pos = snprintf(linea, sizeof(linea), "  %04X  ", (unsigned)offsetBase);
    if (pos < 0) pos = 0;
    for (size_t i = 0; i < 16; i++) {
        if (i < n) {
            linea[pos++] = H[(datos[i] >> 4) & 0x0F];
            linea[pos++] = H[datos[i] & 0x0F];
            linea[pos++] = ' ';
        } else {
            linea[pos++] = ' '; linea[pos++] = ' '; linea[pos++] = ' ';
        }
        if (i == 7) linea[pos++] = ' ';
    }
    linea[pos++] = '|';
    for (size_t i = 0; i < n && i < 16; i++) {
        char c = (char)datos[i];
        linea[pos++] = (c >= 32 && c < 127) ? c : '.';
    }
    linea[pos++] = '|';
    linea[pos++] = '\n';
    linea[pos++] = '\0';
    out.print(linea);
}

inline void dibujar(const uint8_t* base, size_t total,
                    size_t inicioVista, size_t longitudVista,
                    const char* etiqueta, Print& out,
                    const RegionTrama* regiones, size_t numRegiones) {
    out.print(F("\n==== TRAMA "));
    out.print(etiqueta ? etiqueta : "?");
    out.print(F(" ===="));
    if (longitudVista != total) {
        out.print(F("  vista=["));
        out.print((unsigned)inicioVista);
        out.print(F(".."));
        out.print((unsigned)(inicioVista + longitudVista - 1));
        out.print(F("]"));
    }
    out.print(F("  total="));
    out.print((unsigned)total);
    out.print(F("B\n"));

    RegionTrama autoRegs[2];
    size_t      nAuto = 0;
    if ((!regiones || numRegiones == 0) && inicioVista > 0) {
        autoRegs[nAuto++] = { "Encabezados (ya consumidos)", 0,            inicioVista };
        autoRegs[nAuto++] = { "Vista actual (capa superior)", inicioVista, longitudVista };
        regiones    = autoRegs;
        numRegiones = nAuto;
    }

    if (numRegiones > 0) {
        out.print(F("Regiones:\n"));
        for (size_t r = 0; r < numRegiones; r++) {
            char buf[96];
            snprintf(buf, sizeof(buf),
                     "  [%-26s]  0x%04X..0x%04X  (%3u bytes)\n",
                     regiones[r].nombre,
                     (unsigned)regiones[r].offset,
                     (unsigned)(regiones[r].offset + regiones[r].longitud - 1),
                     (unsigned)regiones[r].longitud);
            out.print(buf);
        }
        out.print(F("\n"));
    }

    out.print(F("Hex dump:\n"));
    for (size_t off = 0; off < total; off += 16) {
        size_t n = total - off;
        if (n > 16) n = 16;
        hexLinea(base + off, n, off, out);
    }
    out.print(F("\n"));
}

} // namespace detail

// ---------- Versión BYTES CRUDOS (capa 1, y cualquier otra) ----------
inline void volcarTramaBytes(const uint8_t* datos, size_t len, const char* etiqueta, Print& out,
                             const RegionTrama* regiones = nullptr, size_t numRegiones = 0) {
    if (!datos) { out.print(F("[volcarTramaBytes] datos nulos\n")); return; }
    if (len == 0) {
        out.print(F("\n==== TRAMA "));
        out.print(etiqueta ? etiqueta : "?");
        out.print(F(" ====  (sin bytes)\n\n"));
        return;
    }
    detail::dibujar(datos, len, 0, len, etiqueta, out, regiones, numRegiones);
}

// ---------- Versión BufferPaquete (capas 2..7) ----------
inline void volcarTrama(const BufferPaquete& pkt, const char* etiqueta, Print& out,
                        const RegionTrama* regiones = nullptr, size_t numRegiones = 0) {
    const uint8_t* base = pkt.ptrActual() - pkt.inicio;
    size_t total = pkt.inicio + pkt.longitud;
    detail::dibujar(base, total, pkt.inicio, pkt.longitud,
                    etiqueta, out, regiones, numRegiones);
}

// ---------- Azúcar: ENTRADA / SALIDA ----------
inline void imprimirEntrada(const uint8_t* datos, size_t len, Print& out,
                            const char* detalle = nullptr,
                            const RegionTrama* regs = nullptr, size_t nr = 0) {
    char tag[80];
    if (detalle) snprintf(tag, sizeof(tag), "ENTRADA / %s", detalle);
    else         snprintf(tag, sizeof(tag), "ENTRADA");
    volcarTramaBytes(datos, len, tag, out, regs, nr);
}
inline void imprimirSalida(const uint8_t* datos, size_t len, Print& out,
                           const char* detalle = nullptr,
                           const RegionTrama* regs = nullptr, size_t nr = 0) {
    char tag[80];
    if (detalle) snprintf(tag, sizeof(tag), "SALIDA / %s", detalle);
    else         snprintf(tag, sizeof(tag), "SALIDA");
    volcarTramaBytes(datos, len, tag, out, regs, nr);
}

inline void imprimirEntrada(const BufferPaquete& pkt, Print& out,
                            const char* detalle = nullptr,
                            const RegionTrama* regs = nullptr, size_t nr = 0) {
    char tag[80];
    if (detalle) snprintf(tag, sizeof(tag), "ENTRADA / %s", detalle);
    else         snprintf(tag, sizeof(tag), "ENTRADA");
    volcarTrama(pkt, tag, out, regs, nr);
}
inline void imprimirSalida(const BufferPaquete& pkt, Print& out,
                           const char* detalle = nullptr,
                           const RegionTrama* regs = nullptr, size_t nr = 0) {
    char tag[80];
    if (detalle) snprintf(tag, sizeof(tag), "SALIDA / %s", detalle);
    else         snprintf(tag, sizeof(tag), "SALIDA");
    volcarTrama(pkt, tag, out, regs, nr);
}

} // namespace Debug
} // namespace OSI