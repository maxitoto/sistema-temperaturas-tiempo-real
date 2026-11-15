#ifndef RPC_MIDDLEWARE_MESSAGE_H
#define RPC_MIDDLEWARE_MESSAGE_H

#include <type_traits>
#include <stdexcept>
#include "RPC_base.hpp"
#include "../unmarshalling/decode.hpp"
#include "../../public/RPAuth.hpp"

class RPCMiddlewareMessage : public RPCBase {
public:
    static constexpr size_t MAX_PROCEDIMIENTOS_REGISTRADOS = 32;
    using InvocadorBorradoFn = MascaraClientes (*)(const SobreMensajeRPC&, MascaraClientes& refSuscriptoresStream);
    using EmisorStreamFn     = size_t (*)(uint16_t ticketSecuencia, uint16_t idProc, uint8_t* bufferOut, size_t capMax);
    using FuncionDifusionL1  = void (*)(const uint8_t* buffer, size_t longitud, MascaraClientes destinatarios, CanalSalida canal);

    struct DescriptorProcedimiento {
        uint16_t           idHashNombre;
        const char*        nombre;
        RolUsuario         rolMinimo;
        SemanticaEntrega   semantica;
        MascaraClientes    suscriptoresStream;
        InvocadorBorradoFn invocar;
        EmisorStreamFn     emitirStream;
    };

private:
    inline static DescriptorProcedimiento registro[MAX_PROCEDIMIENTOS_REGISTRADOS] = {};
    inline static size_t   totalRegistrados = 0;
    inline static uint16_t secuenciaStreamGlobal = 0;

public:
    static size_t registrarProcedimiento(
        const char* nombre,
        RolUsuario rolMinimo,
        SemanticaEntrega semantica,
        InvocadorBorradoFn fnInvocador,
        EmisorStreamFn fnEmisorStream = nullptr
    ) {
        if (!nombre || !fnInvocador || totalRegistrados >= MAX_PROCEDIMIENTOS_REGISTRADOS) {
            return 0;
        }
        uint16_t idHash = hashNombreProcedimiento(nombre);
        for (size_t i = 0; i < totalRegistrados; ++i) {
            if (registro[i].idHashNombre == idHash) {
                return i;
            }
        }
        size_t idx = totalRegistrados++;
        registro[idx] = DescriptorProcedimiento{
            idHash,
            nombre,
            rolMinimo,
            semantica,
            0,
            fnInvocador,
            fnEmisorStream
        };
        return idx;
    }

    static MascaraClientes despacharDesdeRegistro(const SobreMensajeRPC& sobre) {
        for (size_t i = 0; i < totalRegistrados; ++i) {
            if (sobre.idProcedimiento == registro[i].idHashNombre) {
                return registro[i].invocar(sobre, registro[i].suscriptoresStream);
            }
        }

        ejecutarConCapturaNativa(sobre.idTicket, sobre.idProcedimiento, [&]() {
            throw std::runtime_error("NameError: procedimiento remoto no existe en el servidor");
        });
        return static_cast<MascaraClientes>(1u << sobre.slotClienteOrigen);
    }

    // Emite un ciclo de todos los servicios registrados con semántica talVez que tengan clientes suscritos
    static void emitirStreamsTalVezActivos(FuncionDifusionL1 callbackDifusion) {
        if (!callbackDifusion) return;
        uint8_t bufferStream[CAPACIDAD_BUFFER_COMPARTIDO];

        for (size_t i = 0; i < totalRegistrados; ++i) {
            if (registro[i].semantica == SemanticaEntrega::TAL_VEZ &&
                registro[i].suscriptoresStream != 0 &&
                registro[i].emitirStream != nullptr) {
                size_t len = registro[i].emitirStream(
                    secuenciaStreamGlobal++,
                    registro[i].idHashNombre,
                    bufferStream,
                    sizeof(bufferStream)
                );
                if (len > 0) {
                    callbackDifusion(bufferStream, len, registro[i].suscriptoresStream, CanalSalida::STREAMING_TELEMETRIA);
                }
            }
        }
    }

    static MascaraClientes obtenerMascaraTotalSuscriptoresStream() {
        MascaraClientes m = 0;
        for (size_t i = 0; i < totalRegistrados; ++i) {
            m |= registro[i].suscriptoresStream;
        }
        return m;
    }

    static void desuscribirClienteDeTodosLosStreams(uint8_t slotCliente) {
        if (slotCliente > 8) return;
        const MascaraClientes maskInv = static_cast<MascaraClientes>(~(1u << slotCliente));
        for (size_t i = 0; i < totalRegistrados; ++i) {
            registro[i].suscriptoresStream &= maskInv;
        }
    }

    template <typename EntradaDTO, typename SalidaDTO, typename Callable>
    static MascaraClientes ejecutarEnvuelto(
        const SobreMensajeRPC& sobre,
        RolUsuario rolMinimoRequerido,
        SemanticaEntrega semantica,
        MascaraClientes& refSuscriptoresStream,
        Callable&& accionServidor
    ) {
        longitudSalidaCompartida = 0;
        const MascaraClientes mascaraSolicitante = static_cast<MascaraClientes>(1u << sobre.slotClienteOrigen);
        int8_t idxTicket = -1;

        bool exito = ejecutarConCapturaNativa(sobre.idTicket, sobre.idProcedimiento, [&]() {
            // 1. La validación de sesión y rol vive en RPAuth y lanza excepción si falla
            RPAuth::exigirSesionAutorizada(sobre.slotClienteOrigen, rolMinimoRequerido);

            if (semantica != SemanticaEntrega::EXACTAMENTE_UNA_VEZ &&
                intentarAcoplarAPeticionEnVuelo(sobre.hashPeticion, sobre.slotClienteOrigen)) {
                return;
            }

            idxTicket = abrirTicketEjecucion(
                sobre.idTicket, sobre.idProcedimiento, sobre.hashPeticion, sobre.slotClienteOrigen
            );
            if (idxTicket < 0) {
                throw std::runtime_error("RuntimeError: tabla de tickets llena en el servidor ESP32");
            }

            sobreEnEjecucion = sobre;

            if constexpr (std::is_void_v<EntradaDTO>) {
                if constexpr (std::is_void_v<SalidaDTO>) {
                    accionServidor();
                    longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento,
                        bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                } else {
                    SalidaDTO resultado = accionServidor();
                    longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                        sobre.idTicket, sobre.idProcedimiento, resultado,
                        bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                }
            } else {
                EntradaDTO dtoEntrada{};
                if (!DecodificadorRPC::deserializarDesdePayload(sobre.payloadEntrada, sobre.longitudEntrada, dtoEntrada)) {
                    throw std::invalid_argument("TypeError: tamano de argumentos invalido para el procedimiento");
                }

                if constexpr (std::is_void_v<SalidaDTO>) {
                    accionServidor(dtoEntrada);
                    longitudSalidaCompartida = CodificadorRPC::serializarAcuseSinPayload(
                        sobre.idTicket, sobre.idProcedimiento,
                        bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                } else {
                    SalidaDTO resultado = accionServidor(dtoEntrada);
                    longitudSalidaCompartida = CodificadorRPC::serializarEnBufferCompartido(
                        sobre.idTicket, sobre.idProcedimiento, resultado,
                        bufferSalidaCompartido, CAPACIDAD_BUFFER_COMPARTIDO
                    );
                }
            }

            // Si es un servicio talVez, el cliente queda suscrito automáticamente al stream de este procedimiento
            if (semantica == SemanticaEntrega::TAL_VEZ && !std::is_void_v<SalidaDTO>) {
                refSuscriptoresStream |= mascaraSolicitante;
            }
        });

        if (!exito) {
            if (idxTicket >= 0) cerrarTicketYObtenerDestinatarios(idxTicket);
            return mascaraSolicitante;
        }
        return (idxTicket >= 0) ? cerrarTicketYObtenerDestinatarios(idxTicket) : 0;
    }
};

struct RegistroProcedimientoToken {
    uint16_t idHash;
    const char* nombre;
};

class ConstructorLlamadoSemantico {
private:
    template <SemanticaEntrega Sem, typename EntradaDTO, typename SalidaDTO, typename Callable>
    static RegistroProcedimientoToken registrarConSemantica(
        const char* nombreProcedimiento,
        RolUsuario rolMinimo,
        Callable fn
    ) {
        static Callable fnEstatica = fn;
        static RolUsuario rolEstatico = rolMinimo;

        auto trampolin = +[](const SobreMensajeRPC& sobre, MascaraClientes& refSuscriptores) -> MascaraClientes {
            return RPCMiddlewareMessage::ejecutarEnvuelto<EntradaDTO, SalidaDTO>(
                sobre, rolEstatico, Sem, refSuscriptores, fnEstatica
            );
        };

        RPCMiddlewareMessage::EmisorStreamFn fnStream = nullptr;
        if constexpr (Sem == SemanticaEntrega::TAL_VEZ && std::is_void_v<EntradaDTO> && !std::is_void_v<SalidaDTO>) {
            fnStream = +[](uint16_t ticketSeq, uint16_t idProc, uint8_t* bufOut, size_t capMax) -> size_t {
                try {
                    SalidaDTO dato = fnEstatica();
                    return CodificadorRPC::serializarEnBufferCompartido(ticketSeq, idProc, dato, bufOut, capMax);
                } catch (...) {
                    return 0;
                }
            };
        }

        RPCMiddlewareMessage::registrarProcedimiento(nombreProcedimiento, rolMinimo, Sem, trampolin, fnStream);
        return RegistroProcedimientoToken{ hashNombreProcedimiento(nombreProcedimiento), nombreProcedimiento };
    }

public:
    template <typename EntradaDTO = void, typename SalidaDTO = void, typename Callable>
    RegistroProcedimientoToken talVez(const char* nombre, Callable fn, RolUsuario rol = RolUsuario::NO_ADMIN) const {
        return registrarConSemantica<SemanticaEntrega::TAL_VEZ, EntradaDTO, SalidaDTO>(nombre, rol, fn);
    }

    template <typename EntradaDTO = void, typename SalidaDTO = void, typename Callable>
    RegistroProcedimientoToken alMenosUnaVez(const char* nombre, Callable fn, RolUsuario rol = RolUsuario::NO_ADMIN) const {
        return registrarConSemantica<SemanticaEntrega::AL_MENOS_UNA_VEZ, EntradaDTO, SalidaDTO>(nombre, rol, fn);
    }

    template <typename EntradaDTO = void, typename SalidaDTO = void, typename Callable>
    RegistroProcedimientoToken exactamenteUnaVez(const char* nombre, Callable fn, RolUsuario rol = RolUsuario::ADMIN) const {
        return registrarConSemantica<SemanticaEntrega::EXACTAMENTE_UNA_VEZ, EntradaDTO, SalidaDTO>(nombre, rol, fn);
    }
};

inline constexpr ConstructorLlamadoSemantico llamadoSemantico{};

#endif