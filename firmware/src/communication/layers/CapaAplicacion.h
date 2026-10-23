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

// Hook para el Proceso de Usuario (Autorización, validación de certificados/tokens o rangos)
// Recibe una copia por valor de LectorMsgPack para no consumir el cursor del Stub
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
    static const size_t MAX_RUTAS_RPC      = 32;
    static const size_t MAX_OYENTES_EVENTO = 8;

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

    EntradaRuta rutas[MAX_RUTAS_RPC];
    size_t totalRutas = 0;

    EntradaEvento oyentesEventos[MAX_OYENTES_EVENTO];
    size_t totalOyentesEventos = 0;

    void enviarErrorRapido(int idNodo, uint16_t reqId, const char* codigoError) {
        if (!presentacion || reqId == 0) return;
        bufferTx.prepararParaTx(idNodo);
        EscritorMsgPack escritor(bufferTx);
        escritor.iniciarArray(2);
        escritor.escribirTexto("ERR");
        escritor.escribirTexto(codigoError);
        presentacion->bajarRespuesta(idNodo, reqId, bufferTx);
    }

  public:
    explicit CapaAplicacion(CapaPresentacion* capaPres) : presentacion(capaPres) {}

    // Permite al Proceso de Usuario inyectar su motor de autorización sin acoplar la capa
    void establecerEvaluadorPolitica(EvaluadorPoliticaRPC evaluador) {
        evaluadorPolitica = evaluador;
    }

    bool on(const char* comando, ManejadorProcedimientoRPC manejador) {
        if (!comando || !manejador) return false;
        for (size_t i = 0; i < totalRutas; i++) {
            if (strcmp(rutas[i].comando, comando) == 0) {
                rutas[i].manejador = manejador;
                return true;
            }
        }
        if (totalRutas >= MAX_RUTAS_RPC) return false;
        rutas[totalRutas++] = EntradaRuta(comando, manejador);
        return true;
    }

    template <typename FnBuilder>
    uint16_t call(const char* comando, uint8_t cantidadArgs, FnBuilder&& constructorArgs,
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

    bool onEvento(const char* evento, ManejadorEventoRPC manejador) {
        if (!evento || !manejador || totalOyentesEventos >= MAX_OYENTES_EVENTO) return false;
        oyentesEventos[totalOyentesEventos++] = EntradaEvento(evento, manejador);
        return true;
    }

    void emitirEventoGlobal(const char* evento, uint8_t cantidadArgs,
                            ConstructorArgumentosRPC constructorArgs = nullptr,
                            void* contextoArgs = nullptr, bool fiable = true) {
        if (!presentacion || !evento) return;
        bufferTx.prepararParaTx(-1);
        EscritorMsgPack escritor(bufferTx);
        if (!escritor.iniciarArray(1 + cantidadArgs) || !escritor.escribirTexto(evento)) return;
        if (cantidadArgs > 0 && constructorArgs) {
            if (!constructorArgs(escritor, contextoArgs)) return;
        }
        presentacion->bajarEventoGlobal(bufferTx, fiable);
    }

    void procesar() {
        if (!presentacion) return;

        MensajePresentacion msj;
        if (presentacion->subirMensaje(msj)) {
            MetaCall meta = { msj.idNodo, msj.reqId };

            // CASO A: Petición RPC entrante (CALL)
            if (msj.tipoSesion == CapaSesion::TIPO_RPC_REQ) {
                ManejadorProcedimientoRPC manejador = nullptr;
                for (size_t i = 0; i < totalRutas; i++) {
                    if (strcmp(rutas[i].comando, msj.cabecera) == 0) {
                        manejador = rutas[i].manejador;
                        break;
                    }
                }

                if (!manejador) {
                    enviarErrorRapido(msj.idNodo, msj.reqId, "ROUTE_NOT_FOUND");
                    return;
                }

                // Consulta al Proceso de Usuario (si inyectó una política de acceso)
                if (evaluadorPolitica) {
                    char motivo[32] = "FORBIDDEN";
                    LectorMsgPack copiaCursor = msj.lector;
                    if (!evaluadorPolitica(msj.cabecera, msj.cantidadArgs, copiaCursor, meta, motivo, sizeof(motivo))) {
                        enviarErrorRapido(msj.idNodo, msj.reqId, motivo);
                        return;
                    }
                }

                bufferTx.prepararParaTx(msj.idNodo);
                EscritorMsgPack escritor(bufferTx);
                bool ok = manejador(msj.cantidadArgs, msj.lector, escritor, meta);
                if (ok && msj.reqId != 0 && bufferTx.longitud > 0) {
                    presentacion->bajarRespuesta(msj.idNodo, msj.reqId, bufferTx);
                } else if (!ok && msj.reqId != 0) {
                    enviarErrorRapido(msj.idNodo, msj.reqId, "HANDLER_EXEC_ERR");
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
            // CASO C: Evento asíncrono entrante
            else if (msj.tipoSesion == CapaSesion::TIPO_EVENTO) {
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