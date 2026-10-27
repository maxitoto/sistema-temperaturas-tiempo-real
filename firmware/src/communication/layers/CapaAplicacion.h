// Archivo: ./src/communication/layers/CapaAplicacion.h
#pragma once
#include <Arduino.h>
#include <string.h>
#include "communication/common/BufferRed.h"
#include "communication/layers/CapaPresentacion.h"

struct MetaCall {
    int idNodo;
    uint16_t reqId;
};

typedef bool (*EvaluadorPoliticaRPC)(
    const char* comando,
    size_t cantidadArgs,
    LectorMsgPack argsCopia,
    const MetaCall& meta,
    char* codigoRechazo,
    size_t capCodigo
);

typedef bool (*ManejadorProcedimientoRPC)(
    size_t cantidadArgs,
    LectorMsgPack& args,
    EscritorMsgPack& respuesta,
    const MetaCall& meta
);

typedef bool (*ConstructorArgumentosRPC)(EscritorMsgPack& args, void* contexto);

typedef void (*CallbackRespuestaRPC)(
    bool exito,
    size_t cantidadDatos,
    LectorMsgPack& datos,
    const MetaCall& meta
);

typedef void (*ManejadorEventoRPC)(
    const char* evento,
    size_t cantidadArgs,
    LectorMsgPack& args,
    int idNodoOrigen
);

class CapaAplicacion {
  public:
    static constexpr size_t MAX_RUTAS_REQUEST  = 32;
    static constexpr size_t MAX_RUTAS_STREAM   = 8;
    static constexpr size_t MAX_OYENTES_EVENTO = 8;

  private:
    CapaPresentacion* presentacion;
    BufferPaquete bufferTx;
    EvaluadorPoliticaRPC evaluadorPolitica = nullptr;

    struct EntradaRuta {
        const char* comando = nullptr;
        ManejadorProcedimientoRPC manejador = nullptr;
        EntradaRuta() = default;
        EntradaRuta(const char* c, ManejadorProcedimientoRPC m) : comando(c), manejador(m) {}
    };

    struct EntradaEvento {
        const char* evento = nullptr;
        ManejadorEventoRPC manejador = nullptr;
        EntradaEvento() = default;
        EntradaEvento(const char* e, ManejadorEventoRPC m) : evento(e), manejador(m) {}
    };

    // Tablas de despacho paralelas: Request (Transaccional) y Stream (Tiempo Real)
    EntradaRuta rutasRequest[MAX_RUTAS_REQUEST];
    size_t totalRutasRequest = 0;

    EntradaRuta rutasStream[MAX_RUTAS_STREAM];
    size_t totalRutasStream = 0;

    EntradaEvento oyentesEventos[MAX_OYENTES_EVENTO];
    size_t totalOyentesEventos = 0;

    void enviarErrorRequest(int idNodo, uint16_t reqId, const char* codigoError) {
        if (!presentacion || reqId == 0) return;
        bufferTx.prepararParaTx(idNodo);
        EscritorMsgPack escritor(bufferTx);
        escritor.iniciarArray(2);
        escritor.escribirTexto("ERR");
        escritor.escribirTexto(codigoError);
        presentacion->bajarRespuesta(idNodo, reqId, bufferTx);
    }

    void enviarErrorStream(int idNodo, uint16_t reqId, const char* codigoError) {
        if (!presentacion || reqId == 0) return;
        bufferTx.prepararParaTx(idNodo);
        EscritorMsgPack escritor(bufferTx);
        escritor.iniciarArray(2);
        escritor.escribirTexto("ERR");
        escritor.escribirTexto(codigoError);
        presentacion->bajarRespuestaStream(idNodo, reqId, bufferTx);
    }

  public:
    explicit CapaAplicacion(CapaPresentacion* capaPres) : presentacion(capaPres) {}

    void establecerEvaluadorPolitica(EvaluadorPoliticaRPC evaluador) {
        evaluadorPolitica = evaluador;
    }

    // =========================================================================
    // CAMINO 1: TRANSACCIONAL FIABLE (REQUEST / REPLY)
    // =========================================================================
    bool onRequest(const char* comando, ManejadorProcedimientoRPC manejador) {
        if (!comando || !manejador) return false;
        for (size_t i = 0; i < totalRutasRequest; i++) {
            if (strcmp(rutasRequest[i].comando, comando) == 0) {
                rutasRequest[i].manejador = manejador;
                return true;
            }
        }
        if (totalRutasRequest >= MAX_RUTAS_REQUEST) return false;
        rutasRequest[totalRutasRequest++] = EntradaRuta(comando, manejador);
        return true;
    }

    // Alias de compatibilidad con los 14 skeletons existentes
    bool on(const char* comando, ManejadorProcedimientoRPC manejador) {
        return onRequest(comando, manejador);
    }

    template <typename FnBuilder>
    uint16_t request(const char* comando, uint8_t cantidadArgs, FnBuilder&& constructorArgs,
                     CallbackRespuestaRPC onRespuesta, unsigned long timeoutMs = 4000,
                     int idNodoForzado = -1) {
        if (!presentacion || !comando || !presentacion->puedeEnviarPeticion()) return 0;

        int idNodo = (idNodoForzado >= 0) ? idNodoForzado : presentacion->obtenerNodoPreferido();
        if (idNodo < 0) return 0;

        bufferTx.prepararParaTx(idNodo);
        EscritorMsgPack escritor(bufferTx);

        if (!escritor.iniciarArray(1 + cantidadArgs) || !escritor.escribirTexto(comando)) {
            return 0;
        }
        if (cantidadArgs > 0) {
            if (!constructorArgs(escritor)) return 0;
        }

        return presentacion->bajarPeticion(
            idNodo,
            bufferTx,
            reinterpret_cast<void*>(onRespuesta),
            timeoutMs
        );
    }

    // Alias de compatibilidad con BackendStub
    template <typename FnBuilder>
    uint16_t call(const char* comando, uint8_t cantidadArgs, FnBuilder&& constructorArgs,
                  CallbackRespuestaRPC onRespuesta, unsigned long timeoutMs = 4000,
                  int idNodoForzado = -1) {
        return request(comando, cantidadArgs, constructorArgs, onRespuesta, timeoutMs, idNodoForzado);
    }

    bool onEvento(const char* evento, ManejadorEventoRPC manejador) {
        if (!evento || !manejador || totalOyentesEventos >= MAX_OYENTES_EVENTO) return false;
        oyentesEventos[totalOyentesEventos++] = EntradaEvento(evento, manejador);
        return true;
    }

    void emitirEventoGlobal(const char* evento, uint8_t cantidadArgs,
                            ConstructorArgumentosRPC constructorArgs = nullptr,
                            void* contextoArgs = nullptr) {
        if (!presentacion || !evento) return;
        bufferTx.prepararParaTx(-1);
        EscritorMsgPack escritor(bufferTx);
        if (!escritor.iniciarArray(1 + cantidadArgs) || !escritor.escribirTexto(evento)) return;
        if (cantidadArgs > 0 && constructorArgs) {
            if (!constructorArgs(escritor, contextoArgs)) return;
        }
        presentacion->bajarEventoGlobal(bufferTx);
    }

    // =========================================================================
    // CAMINO 2: STREAMING TIEMPO REAL (UDP / DATAGRAMA)
    // =========================================================================
    bool onStream(const char* canal, ManejadorProcedimientoRPC manejador) {
        if (!canal || !manejador) return false;
        for (size_t i = 0; i < totalRutasStream; i++) {
            if (strcmp(rutasStream[i].comando, canal) == 0) {
                rutasStream[i].manejador = manejador;
                return true;
            }
        }
        if (totalRutasStream >= MAX_RUTAS_STREAM) return false;
        rutasStream[totalRutasStream++] = EntradaRuta(canal, manejador);
        return true;
    }

    void emitirStreamGlobal(const char* canal, uint8_t cantidadArgs,
                            ConstructorArgumentosRPC constructorArgs = nullptr,
                            void* contextoArgs = nullptr) {
        if (!presentacion || !canal) return;
        bufferTx.prepararParaTx(-1);
        EscritorMsgPack escritor(bufferTx);
        if (!escritor.iniciarArray(1 + cantidadArgs) || !escritor.escribirTexto(canal)) return;
        if (cantidadArgs > 0 && constructorArgs) {
            if (!constructorArgs(escritor, contextoArgs)) return;
        }
        presentacion->bajarStreamGlobal(bufferTx);
    }

    // =========================================================================
    // MOTOR DE DESPACHO DE SUBIDA (RX)
    // =========================================================================
    void procesar() {
        if (!presentacion) return;

        MensajePresentacion msj;
        if (presentacion->subirMensaje(msj)) {
            MetaCall meta = { msj.idNodo, msj.reqId };

            // CASO A1: Petición Transaccional entrante (REQUEST -> PROTO_TCP)
            if (msj.tipoSesion == CapaSesion::TIPO_RPC_REQ) {
                ManejadorProcedimientoRPC manejador = nullptr;
                for (size_t i = 0; i < totalRutasRequest; i++) {
                    if (strcmp(rutasRequest[i].comando, msj.cabecera) == 0) {
                        manejador = rutasRequest[i].manejador;
                        break;
                    }
                }

                if (!manejador) {
                    enviarErrorRequest(msj.idNodo, msj.reqId, "ROUTE_NOT_FOUND");
                    return;
                }

                if (evaluadorPolitica) {
                    char motivo[32] = "FORBIDDEN";
                    LectorMsgPack copiaCursor = msj.lector;
                    if (!evaluadorPolitica(msj.cabecera, msj.cantidadArgs, copiaCursor, meta, motivo, sizeof(motivo))) {
                        enviarErrorRequest(msj.idNodo, msj.reqId, motivo);
                        return;
                    }
                }

                bufferTx.prepararParaTx(msj.idNodo);
                EscritorMsgPack escritor(bufferTx);
                bool ok = manejador(msj.cantidadArgs, msj.lector, escritor, meta);
                if (ok && msj.reqId != 0 && bufferTx.longitud > 0) {
                    presentacion->bajarRespuesta(msj.idNodo, msj.reqId, bufferTx);
                } else if (!ok && msj.reqId != 0) {
                    enviarErrorRequest(msj.idNodo, msj.reqId, "HANDLER_EXEC_ERR");
                }
            }
            // CASO A2: Petición de Streaming entrante (STREAM_REQ -> PROTO_UDP)
            else if (msj.tipoSesion == CapaSesion::TIPO_STREAM_REQ) {
                ManejadorProcedimientoRPC manejador = nullptr;
                for (size_t i = 0; i < totalRutasStream; i++) {
                    if (strcmp(rutasStream[i].comando, msj.cabecera) == 0) {
                        manejador = rutasStream[i].manejador;
                        break;
                    }
                }

                if (!manejador) {
                    enviarErrorStream(msj.idNodo, msj.reqId, "STREAM_NOT_FOUND");
                    return;
                }

                if (evaluadorPolitica) {
                    char motivo[32] = "FORBIDDEN";
                    LectorMsgPack copiaCursor = msj.lector;
                    if (!evaluadorPolitica(msj.cabecera, msj.cantidadArgs, copiaCursor, meta, motivo, sizeof(motivo))) {
                        enviarErrorStream(msj.idNodo, msj.reqId, motivo);
                        return;
                    }
                }

                bufferTx.prepararParaTx(msj.idNodo);
                EscritorMsgPack escritor(bufferTx);
                bool ok = manejador(msj.cantidadArgs, msj.lector, escritor, meta);
                if (ok && bufferTx.longitud > 0) {
                    presentacion->bajarRespuestaStream(msj.idNodo, msj.reqId, bufferTx);
                } else if (!ok && msj.reqId != 0) {
                    enviarErrorStream(msj.idNodo, msj.reqId, "STREAM_EXEC_ERR");
                }
            }
            // CASO B: Respuesta correlacionada o Timeout desde Capa 5
            else if (msj.tipoSesion == CapaSesion::TIPO_RPC_RES ||
                     msj.tipoSesion == CapaSesion::TIPO_ERROR_SESION ||
                     msj.tipoSesion == CapaSesion::TIPO_TIMEOUT_LOCAL) {
                CallbackRespuestaRPC cb = reinterpret_cast<CallbackRespuestaRPC>(msj.callbackPendiente);
                if (cb) {
                    bool exito = (msj.tipoSesion == CapaSesion::TIPO_RPC_RES) &&
                                 (strcmp(msj.cabecera, "OK") == 0);
                    cb(exito, msj.cantidadArgs, msj.lector, meta);
                }
            }
            // CASO C: Evento o Stream unidireccional entrante
            else if (msj.tipoSesion == CapaSesion::TIPO_EVENTO ||
                     msj.tipoSesion == CapaSesion::TIPO_STREAM_DATA) {
                for (size_t i = 0; i < totalOyentesEventos; i++) {
                    if (strcmp(oyentesEventos[i].evento, msj.cabecera) == 0) {
                        oyentesEventos[i].manejador(msj.cabecera, msj.cantidadArgs, msj.lector, msj.idNodo);
                    }
                }
            }
        }

        presentacion->mantenerSesiones();
    }

    bool hayClientesActivos() const {
        return presentacion ? presentacion->hayClientesActivos() : false;
    }
};