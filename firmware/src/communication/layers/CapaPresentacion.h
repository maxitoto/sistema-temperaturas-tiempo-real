// Archivo: ./src/communication/layers/CapaPresentacion.h
#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaSesion.h"

// ==========================================================
// CURSOR LECTOR MESSAGEPACK (UNMARSHALLING ZERO-HEAP)
// ==========================================================
class LectorMsgPack {
  private:
    const uint8_t* buf;
    size_t len;
    size_t pos;

  public:
    LectorMsgPack() : buf(nullptr), len(0), pos(0) {}
    LectorMsgPack(const uint8_t* datos, size_t longitud) : buf(datos), len(longitud), pos(0) {}

    void reiniciar(const uint8_t* datos, size_t longitud) {
        buf = datos;
        len = longitud;
        pos = 0;
    }

    bool hayDatos() const { return buf != nullptr && pos < len; }
    size_t posicionActual() const { return pos; }
    size_t bytesRestantes() const { return (pos < len) ? (len - pos) : 0; }

    bool saltarValor(uint8_t profundidad = 0) {
        if (!buf || pos >= len || profundidad > 6) return false;
        uint8_t b = buf[pos++];

        if (b <= 0x7F || b >= 0xE0 || b == 0xC0 || b == 0xC2 || b == 0xC3) return true;
        if (b >= 0xA0 && b <= 0xBF) { pos += (b & 0x1F); return pos <= len; }
        if (b >= 0x90 && b <= 0x9F) {
            uint8_t n = b & 0x0F;
            for (uint8_t i = 0; i < n; i++) if (!saltarValor(profundidad + 1)) return false;
            return true;
        }
        if (b >= 0x80 && b <= 0x8F) {
            uint8_t n = b & 0x0F;
            for (uint8_t i = 0; i < n * 2; i++) if (!saltarValor(profundidad + 1)) return false;
            return true;
        }
        switch (b) {
            case 0xCC: case 0xD0: pos += 1; break;
            case 0xCD: case 0xD1: pos += 2; break;
            case 0xCE: case 0xD2: case 0xCA: pos += 4; break;
            case 0xCF: case 0xD3: case 0xCB: pos += 8; break;
            case 0xD9: case 0xC4:
                if (pos >= len) return false;
                pos += 1 + buf[pos];
                break;
            case 0xDA: case 0xC5:
                if (pos + 2 > len) return false;
                pos += 2 + (((uint16_t)buf[pos] << 8) | buf[pos + 1]);
                break;
            case 0xDC: {
                if (pos + 2 > len) return false;
                uint16_t n = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
                pos += 2;
                for (uint16_t i = 0; i < n; i++) if (!saltarValor(profundidad + 1)) return false;
                break;
            }
            default: return false;
        }
        return pos <= len;
    }

    bool iniciarArray(size_t& cantidadElementos) {
        if (!buf || pos >= len) return false;
        uint8_t b = buf[pos];
        if (b >= 0x90 && b <= 0x9F) {
            pos++;
            cantidadElementos = b & 0x0F;
            return true;
        }
        if (b == 0xDC && pos + 3 <= len) {
            pos++;
            cantidadElementos = ((size_t)buf[pos] << 8) | buf[pos + 1];
            pos += 2;
            return true;
        }
        return false;
    }

    bool iniciarMapa(size_t& cantidadPares) {
        if (!buf || pos >= len) return false;
        uint8_t b = buf[pos];
        if (b >= 0x80 && b <= 0x8F) {
            pos++;
            cantidadPares = b & 0x0F;
            return true;
        }
        if (b == 0xDE && pos + 3 <= len) {
            pos++;
            cantidadPares = ((size_t)buf[pos] << 8) | buf[pos + 1];
            pos += 2;
            return true;
        }
        return false;
    }

    bool leerBool(bool& salida) {
        if (!buf || pos >= len) return false;
        uint8_t b = buf[pos];
        if (b == 0xC3) { pos++; salida = true; return true; }
        if (b == 0xC2) { pos++; salida = false; return true; }
        return false;
    }

    bool leerInt32(int32_t& salida) {
        if (!buf || pos >= len) return false;
        uint8_t b = buf[pos++];
        if (b <= 0x7F) { salida = (int32_t)b; return true; }
        if (b >= 0xE0) { salida = (int32_t)((int8_t)b); return true; }
        switch (b) {
            case 0xCC: if (pos + 1 > len) return false; salida = (int32_t)buf[pos++]; return true;
            case 0xD0: if (pos + 1 > len) return false; salida = (int32_t)((int8_t)buf[pos++]); return true;
            case 0xCD:
                if (pos + 2 > len) return false;
                salida = (int32_t)(((uint16_t)buf[pos] << 8) | buf[pos + 1]);
                pos += 2; return true;
            case 0xD1:
                if (pos + 2 > len) return false;
                salida = (int32_t)((int16_t)(((uint16_t)buf[pos] << 8) | buf[pos + 1]));
                pos += 2; return true;
            case 0xCE: case 0xD2:
                if (pos + 4 > len) return false;
                salida = (int32_t)(((uint32_t)buf[pos] << 24) | ((uint32_t)buf[pos + 1] << 16) |
                                   ((uint32_t)buf[pos + 2] << 8) | buf[pos + 3]);
                pos += 4; return true;
            case 0xCF: case 0xD3: {
                if (pos + 8 > len) return false;
                int64_t v64 = 0;
                for (int i = 0; i < 8; i++) v64 = (v64 << 8) | buf[pos + i];
                pos += 8;
                if (v64 > 2147483647LL) salida = 2147483647L;
                else if (v64 < -2147483648LL) salida = -2147483648L;
                else salida = (int32_t)v64;
                return true;
            }
            default:
                pos--;
                return false;
        }
    }

    bool leerUInt32(uint32_t& salida) {
        int32_t temp = 0;
        if (!leerInt32(temp)) return false;
        salida = (uint32_t)temp;
        return true;
    }

    bool leerFloat(float& salida) {
        if (!buf || pos >= len) return false;
        uint8_t b = buf[pos];
        if (b == 0xCA && pos + 5 <= len) {
            pos++;
            uint32_t raw = ((uint32_t)buf[pos] << 24) | ((uint32_t)buf[pos + 1] << 16) |
                           ((uint32_t)buf[pos + 2] << 8) | buf[pos + 3];
            pos += 4;
            memcpy(&salida, &raw, 4);
            return true;
        }
        if (b == 0xCB && pos + 9 <= len) {
            pos++;
            uint64_t raw = 0;
            for (int i = 0; i < 8; i++) raw = (raw << 8) | buf[pos + i];
            pos += 8;
            double d = 0.0;
            memcpy(&d, &raw, 8);
            salida = (float)d;
            return true;
        }
        if ((b == 0xCF || b == 0xD3) && pos + 9 <= len) {
            pos++;
            int64_t v64 = 0;
            for (int i = 0; i < 8; i++) v64 = (v64 << 8) | buf[pos + i];
            pos += 8;
            salida = (float)v64;
            return true;
        }
        int32_t entero = 0;
        if (leerInt32(entero)) {
            salida = (float)entero;
            return true;
        }
        return false;
    }

    bool leerBinario(uint8_t* destino, size_t capacidadDestino, size_t& longitudLeida) {
        if (!buf || !destino || pos >= len) return false;
        uint8_t b = buf[pos++];
        size_t blen = 0;
        if (b == 0xC4) {
            if (pos >= len) return false;
            blen = buf[pos++];
        } else if (b == 0xC5) {
            if (pos + 2 > len) return false;
            blen = ((size_t)buf[pos] << 8) | buf[pos + 1];
            pos += 2;
        } else {
            pos--;
            return false;
        }
        if (pos + blen > len || blen > capacidadDestino) return false;
        memcpy(destino, &buf[pos], blen);
        longitudLeida = blen;
        pos += blen;
        return true;
    }

    bool leerTexto(char* destino, size_t capacidadDestino) {
        if (!buf || !destino || capacidadDestino == 0 || pos >= len) return false;
        uint8_t b = buf[pos++];
        size_t slen = 0;

        if (b >= 0xA0 && b <= 0xBF) {
            slen = b & 0x1F;
        } else if (b == 0xD9) {
            if (pos >= len) return false;
            slen = buf[pos++];
        } else if (b == 0xDA) {
            if (pos + 2 > len) return false;
            slen = ((size_t)buf[pos] << 8) | buf[pos + 1];
            pos += 2;
        } else {
            pos--;
            return false;
        }

        if (pos + slen > len) return false;
        size_t aCopiar = (slen < capacidadDestino - 1) ? slen : (capacidadDestino - 1);
        memcpy(destino, &buf[pos], aCopiar);
        destino[aCopiar] = '\0';
        pos += slen;
        return true;
    }
};

// ==========================================================
// CURSOR ESCRITOR MESSAGEPACK (MARSHALLING ZERO-COPY EN BUFFERPAQUETE)
// ==========================================================
class EscritorMsgPack {
  private:
    BufferPaquete* pkt;
    uint8_t* buf;
    size_t maxLen;
    size_t pos;

  public:
    explicit EscritorMsgPack(BufferPaquete& paqueteDestino)
        : pkt(&paqueteDestino),
          buf(paqueteDestino.ptrPayloadApp()),
          maxLen(LimitesPila::MAX_PAYLOAD_APP),
          pos(paqueteDestino.longitud) {}

    size_t bytesEscritos() const { return pos; }

    bool iniciarArray(size_t cantidadElementos) {
        if (cantidadElementos <= 15) {
            if (pos + 1 > maxLen) return false;
            buf[pos++] = (uint8_t)(0x90 | cantidadElementos);
        } else {
            if (pos + 3 > maxLen) return false;
            buf[pos++] = 0xDC;
            buf[pos++] = (uint8_t)(cantidadElementos >> 8);
            buf[pos++] = (uint8_t)(cantidadElementos & 0xFF);
        }
        pkt->longitud = pos;
        return true;
    }

    bool iniciarMapa(size_t cantidadPares) {
        if (cantidadPares <= 15) {
            if (pos + 1 > maxLen) return false;
            buf[pos++] = (uint8_t)(0x80 | cantidadPares);
        } else {
            if (pos + 3 > maxLen) return false;
            buf[pos++] = 0xDE;
            buf[pos++] = (uint8_t)(cantidadPares >> 8);
            buf[pos++] = (uint8_t)(cantidadPares & 0xFF);
        }
        pkt->longitud = pos;
        return true;
    }

    bool escribirNulo() {
        if (pos + 1 > maxLen) return false;
        buf[pos++] = 0xC0;
        pkt->longitud = pos;
        return true;
    }

    bool escribirBool(bool valor) {
        if (pos + 1 > maxLen) return false;
        buf[pos++] = valor ? 0xC3 : 0xC2;
        pkt->longitud = pos;
        return true;
    }

    bool escribirInt(int32_t valor) {
        if (valor >= 0 && valor <= 127) {
            if (pos + 1 > maxLen) return false;
            buf[pos++] = (uint8_t)valor;
        } else if (valor >= -32 && valor < 0) {
            if (pos + 1 > maxLen) return false;
            buf[pos++] = (uint8_t)((int8_t)valor);
        } else if (valor >= -128 && valor <= 127) {
            if (pos + 2 > maxLen) return false;
            buf[pos++] = 0xD0;
            buf[pos++] = (uint8_t)((int8_t)valor);
        } else if (valor >= -32768 && valor <= 32767) {
            if (pos + 3 > maxLen) return false;
            buf[pos++] = 0xD1;
            uint16_t v = (uint16_t)((int16_t)valor);
            buf[pos++] = (uint8_t)(v >> 8);
            buf[pos++] = (uint8_t)(v & 0xFF);
        } else {
            if (pos + 5 > maxLen) return false;
            buf[pos++] = 0xD2;
            uint32_t v = (uint32_t)valor;
            buf[pos++] = (uint8_t)(v >> 24);
            buf[pos++] = (uint8_t)(v >> 16);
            buf[pos++] = (uint8_t)(v >> 8);
            buf[pos++] = (uint8_t)(v & 0xFF);
        }
        pkt->longitud = pos;
        return true;
    }

    bool escribirUInt(uint32_t valor) {
        if (valor <= 127) {
            if (pos + 1 > maxLen) return false;
            buf[pos++] = (uint8_t)valor;
        } else if (valor <= 0xFF) {
            if (pos + 2 > maxLen) return false;
            buf[pos++] = 0xCC;
            buf[pos++] = (uint8_t)valor;
        } else if (valor <= 0xFFFF) {
            if (pos + 3 > maxLen) return false;
            buf[pos++] = 0xCD;
            buf[pos++] = (uint8_t)(valor >> 8);
            buf[pos++] = (uint8_t)(valor & 0xFF);
        } else {
            if (pos + 5 > maxLen) return false;
            buf[pos++] = 0xCE;
            buf[pos++] = (uint8_t)(valor >> 24);
            buf[pos++] = (uint8_t)(valor >> 16);
            buf[pos++] = (uint8_t)(valor >> 8);
            buf[pos++] = (uint8_t)(valor & 0xFF);
        }
        pkt->longitud = pos;
        return true;
    }

    bool escribirFloat(float valor) {
        if (pos + 5 > maxLen) return false;
        uint32_t raw = 0;
        memcpy(&raw, &valor, 4);
        buf[pos++] = 0xCA;
        buf[pos++] = (uint8_t)(raw >> 24);
        buf[pos++] = (uint8_t)(raw >> 16);
        buf[pos++] = (uint8_t)(raw >> 8);
        buf[pos++] = (uint8_t)(raw & 0xFF);
        pkt->longitud = pos;
        return true;
    }

    bool escribirBinario(const uint8_t* datos, size_t longitud) {
        if (!datos && longitud > 0) return false;
        if (longitud <= 255) {
            if (pos + 2 + longitud > maxLen) return false;
            buf[pos++] = 0xC4;
            buf[pos++] = (uint8_t)longitud;
        } else {
            if (pos + 3 + longitud > maxLen) return false;
            buf[pos++] = 0xC5;
            buf[pos++] = (uint8_t)(longitud >> 8);
            buf[pos++] = (uint8_t)(longitud & 0xFF);
        }
        if (longitud > 0) {
            memcpy(&buf[pos], datos, longitud);
            pos += longitud;
        }
        pkt->longitud = pos;
        return true;
    }

    bool escribirTexto(const char* str) {
        if (!str) return escribirNulo();
        size_t slen = strlen(str);
        if (slen <= 31) {
            if (pos + 1 + slen > maxLen) return false;
            buf[pos++] = (uint8_t)(0xA0 | slen);
        } else if (slen <= 255) {
            if (pos + 2 + slen > maxLen) return false;
            buf[pos++] = 0xD9;
            buf[pos++] = (uint8_t)slen;
        } else {
            if (pos + 3 + slen > maxLen) return false;
            buf[pos++] = 0xDA;
            buf[pos++] = (uint8_t)(slen >> 8);
            buf[pos++] = (uint8_t)(slen & 0xFF);
        }
        memcpy(&buf[pos], str, slen);
        pos += slen;
        pkt->longitud = pos;
        return true;
    }
};

struct MensajePresentacion {
    int idNodo = -1;
    uint8_t tipoSesion = 0;
    uint16_t reqId = 0;
    char cabecera[40] = {0};
    size_t cantidadArgs = 0;
    LectorMsgPack lector;
    void* callbackPendiente = nullptr;
};

typedef bool (*TransformadorPayloadCapa6)(
    int idNodo,
    uint16_t reqId,
    uint8_t* bufferInOut,
    size_t& longitudInOut,
    size_t capacidadMax
);

class CapaPresentacion {
  private:
    CapaSesion* sesion;
    BufferPaquete bufferRx;
    TransformadorPayloadCapa6 transformadorTx = nullptr;
    TransformadorPayloadCapa6 transformadorRx = nullptr;

  public:
    explicit CapaPresentacion(CapaSesion* capaSesion) : sesion(capaSesion) {}

    int obtenerNodoPreferido() const {
        return sesion ? sesion->resolverNodoPreferido() : -1;
    }

    bool puedeEnviarPeticion() const {
        return sesion ? sesion->haySlotPendienteLibre() : false;
    }

    void configurarTransformadores(TransformadorPayloadCapa6 fnTx, TransformadorPayloadCapa6 fnRx) {
        transformadorTx = fnTx;
        transformadorRx = fnRx;
    }

    bool subirMensaje(MensajePresentacion& msjSalida) {
        MensajeSesion meta;
        if (!sesion || !sesion->subirSesion(bufferRx, meta)) return false;

        msjSalida.idNodo = meta.idNodo;
        msjSalida.tipoSesion = meta.tipo;
        msjSalida.reqId = meta.reqId;
        msjSalida.cabecera[0] = '\0';
        msjSalida.cantidadArgs = 0;
        msjSalida.callbackPendiente = meta.callbackPendiente;

        if (meta.tipo == CapaSesion::TIPO_TIMEOUT_LOCAL) {
            strncpy(msjSalida.cabecera, "ERR_TIMEOUT", sizeof(msjSalida.cabecera));
            msjSalida.lector.reiniciar(nullptr, 0);
            return true;
        }

        if (meta.tipo == CapaSesion::TIPO_ERROR_SESION) {
            strncpy(msjSalida.cabecera, "ERR_SESION", sizeof(msjSalida.cabecera));
            msjSalida.lector.reiniciar(bufferRx.ptrActual(), bufferRx.longitud);
            return true;
        }

        if (bufferRx.longitud == 0 || bufferRx.longitud > LimitesPila::MAX_PAYLOAD_APP) {
            return false;
        }

        // Hook RX Capa 6: Desencripta el mensaje completo antes de decodificar MessagePack
        if (transformadorRx) {
            if (!transformadorRx(meta.idNodo, meta.reqId, bufferRx.ptrActual(), bufferRx.longitud, LimitesPila::MAX_PAYLOAD_APP)) {
                return false;
            }
        }

        msjSalida.lector.reiniciar(bufferRx.ptrActual(), bufferRx.longitud);

        size_t totalElementos = 0;
        if (!msjSalida.lector.iniciarArray(totalElementos) || totalElementos == 0) {
            return false;
        }

        if (!msjSalida.lector.leerTexto(msjSalida.cabecera, sizeof(msjSalida.cabecera))) {
            return false;
        }

        msjSalida.cantidadArgs = totalElementos - 1;
        return true;
    }

    uint16_t bajarPeticion(int idNodo, BufferPaquete& pkt, void* callback, unsigned long timeoutMs) {
        if (!sesion) return 0;
        if (transformadorTx) {
            if (!transformadorTx(idNodo, 0, pkt.ptrPayloadApp(), pkt.longitud, LimitesPila::MAX_PAYLOAD_APP)) {
                return 0;
            }
        }
        return sesion->enviarPeticionRPC(idNodo, pkt, callback, timeoutMs);
    }

    bool bajarRespuesta(int idNodo, uint16_t reqId, BufferPaquete& pkt) {
        if (!sesion) return false;
        if (transformadorTx) {
            if (!transformadorTx(idNodo, reqId, pkt.ptrPayloadApp(), pkt.longitud, LimitesPila::MAX_PAYLOAD_APP)) {
                return false;
            }
        }
        return sesion->bajarRespuesta(idNodo, reqId, pkt);
    }

    void bajarEventoGlobal(BufferPaquete& pkt, bool fiable = true) {
        if (!sesion) return;
        if (transformadorTx) {
            if (!transformadorTx(-1, 0, pkt.ptrPayloadApp(), pkt.longitud, LimitesPila::MAX_PAYLOAD_APP)) {
                return;
            }
        }
        sesion->bajarEventoGlobal(pkt, fiable);
    }

    void mantenerSesiones() {
        if (sesion) sesion->auditarSesiones();
    }

    bool hayClientesActivos() const {
        return sesion ? sesion->hayClientesAutenticados() : false;
    }
};