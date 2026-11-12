#ifndef UARTS_RUNTIME_H
#define UARTS_RUNTIME_H

#include <atomic>
#include "UART/AdaptadorHardwareUART.hpp"
#include "utils/DefinicionTramaUARTS.hpp"
#include "out/SegmentadorMensajesSalientes.hpp"
#include "out/GestorRetransmisionYEnvio.hpp"
#include "in/DeserializadorTramasEntrantes.hpp"
#include "in/ReensambladorOrdenadoEntrante.hpp"
#include "../../../../TeeUART.hpp"

class UARTSRuntime {
public:
    static constexpr uint32_t BAUDIOS_SERVIDOR = 115200;
    static constexpr int8_t PIN_RX_SERVIDOR = 3;
    static constexpr int8_t PIN_TX_SERVIDOR = 1;

private:
    AdaptadorHardwareUART adaptadorUART;
    GestorRetransmisionYEnvio gestorSalida;
    DeserializadorTramasEntrantes deserializadorEntrada;
    ReensambladorOrdenadoEntrante reensambladorEntrada;

    std::atomic<bool> clienteConectado{false};
    uint16_t contadorSecuenciaSalida{0};
    uint16_t ultimoSecuenciaConfirmada{0};

public:
    // ¿Cómo lo hace?: Vincula el puerto HardwareSerial nativo con el adaptador y el gestor de salida dejando el estado desconectado.
    explicit UARTSRuntime(HardwareSerial& puertoHardware)
        : adaptadorUART(puertoHardware),
          gestorSalida(adaptadorUART),
          contadorSecuenciaSalida(0),
          ultimoSecuenciaConfirmada(0) {}

    explicit UARTSRuntime(TeeUART& teePuerto)
        : UARTSRuntime(teePuerto.hardwareSerial()) {}

    // ¿Cómo lo hace?: Invoca a inicializarAdaptativo() con las constantes fijas del servidor (115200, 8E1) y resetea los contadores de secuencia.
    void abrirPuertoServidor() {
        adaptadorUART.inicializarAdaptativo(BAUDIOS_SERVIDOR, PIN_RX_SERVIDOR, PIN_TX_SERVIDOR);
        cerrarConexionCliente();
    }

    // ¿Cómo lo hace?: Reinicia las máquinas de estado de entrada/salida, pone los contadores de secuencia en cero y apaga atómicamente clienteConectado.
    void cerrarConexionCliente(bool notificarAlCliente = true) {
        const bool estabaConectado = clienteConectado.exchange(false, std::memory_order_relaxed);
        if (estabaConectado && notificarAlCliente) {
            gestorSalida.enviarTramaControlInmediata(BANDERA_CERRAR, 0, 0);
        }
        contadorSecuenciaSalida = 0;
        ultimoSecuenciaConfirmada = 0;
        deserializadorEntrada.reiniciarEstadoParser();
        reensambladorEntrada = ReensambladorOrdenadoEntrante();
        gestorSalida.reiniciarColaSalida();
    }

    // ¿Cómo lo hace?: Cambia atómicamente la bandera clienteConectado a true cuando el handshake de L1 o la bandera SINCRONIZAR lo confirman.
    void marcarClienteConectado() {
        clienteConectado.store(true, std::memory_order_relaxed);
    }

    // ¿Cómo lo hace?: Lee de forma atómica (memory_order_relaxed) la bandera clienteConectado sin bloquear tareas de FreeRTOS como dibujarOled().
    bool hayClienteConectado() const {
        return clienteConectado.load(std::memory_order_relaxed);
    }

    // ¿Cómo lo hace?: Fracciona el buffer usando SegmentadorMensajesSalientes según el FIFO y encola cada tramo en GestorRetransmisionYEnvio.
    bool enviarMensajeConfiable(const uint8_t* datos, size_t longitud) {
        if (datos == nullptr || longitud == 0) {
            return false;
        }

        const PerfilCapacidadesUART& perfil = adaptadorUART.obtenerPerfilCapacidades();
        if (perfil.tamanoMaximoSegmentoDatos == 0) {
            return false;
        }

        // Protege contra truncamiento silencioso si el mensaje supera las 16 ranuras de salida
        const size_t capacidadMaximaBytes = static_cast<size_t>(CAPACIDAD_COLA_SALIDA) * perfil.tamanoMaximoSegmentoDatos;
        if (longitud > capacidadMaximaBytes) {
            return false;
        }

        TramaSegmentoUARTS segmentosGenerados[CAPACIDAD_COLA_SALIDA];
        uint8_t ventanaLocal = reensambladorEntrada.calcularVentanaSegmentosDisponible(perfil.tamanoMaximoSegmentoDatos);
        bool modoHD = (perfil.modoDuplex == MODO_HALF_DUPLEX);

        uint8_t cantidad = SegmentadorMensajesSalientes::dividirMensajeEnSegmentos(
            datos,
            longitud,
            perfil.tamanoMaximoSegmentoDatos,
            contadorSecuenciaSalida,
            ultimoSecuenciaConfirmada,
            ventanaLocal,
            modoHD,
            segmentosGenerados,
            CAPACIDAD_COLA_SALIDA
        );

        if (cantidad == 0) {
            return false;
        }

        for (uint8_t i = 0; i < cantidad; i++) {
            if (!gestorSalida.encolarSegmentoParaEnvio(segmentosGenerados[i])) {
                return false;
            }
        }
        gestorSalida.actualizarCicloEnvioYRetransmision();
        return true;
    }

    // ¿Cómo lo hace?: Drena bytes RX, activa clienteConectado si recibe BANDERA_SINCRONIZAR, procesa ACKs/NACKs y ejecuta retransmisiones vencidas.
    void ejecutarCicloProtocolo() {
        const PerfilCapacidadesUART& perfil = adaptadorUART.obtenerPerfilCapacidades();
        TramaSegmentoUARTS tramaRecibida;

        ResultadoLecturaTrama estadoLectura = deserializadorEntrada.procesarBytesDisponibles(adaptadorUART, tramaRecibida);

        if (estadoLectura == ERROR_FORMATO_TRAMA) {
            uint8_t ventana = reensambladorEntrada.calcularVentanaSegmentosDisponible(perfil.tamanoMaximoSegmentoDatos);
            uint8_t banderasNack = BANDERA_NACK | ((perfil.modoDuplex == MODO_HALF_DUPLEX) ? BANDERA_CEDER_TURNO_HD : 0);
            gestorSalida.enviarTramaControlInmediata(
                banderasNack,
                deserializadorEntrada.obtenerUltimaSecuenciaDetectada(),
                ventana
            );
        } else if (estadoLectura == TRAMA_VALIDA_COMPLETA) {
            if (tramaRecibida.banderas & BANDERA_CERRAR) {
                cerrarConexionCliente(false);
                return;
            }
            
            if (tramaRecibida.banderas & BANDERA_SINCRONIZAR) {
                contadorSecuenciaSalida = 0;
                ultimoSecuenciaConfirmada = 0;
                reensambladorEntrada = ReensambladorOrdenadoEntrante();
                gestorSalida.reiniciarColaSalida();
                clienteConectado.store(true, std::memory_order_relaxed);

                uint8_t ventana = reensambladorEntrada.calcularVentanaSegmentosDisponible(perfil.tamanoMaximoSegmentoDatos);
                gestorSalida.enviarTramaControlInmediata(BANDERA_ACK | BANDERA_SINCRONIZAR, 0, ventana);
            }

            if (tramaRecibida.banderas & (BANDERA_ACK | BANDERA_NACK)) {
                gestorSalida.procesarConfirmacionEntrante(
                    tramaRecibida.banderas,
                    tramaRecibida.numeroConfirmacion,
                    tramaRecibida.ventanaRecepcion
                );
            }

            if (tramaRecibida.banderas & BANDERA_DATOS) {
                bool eraDuplicado = false;
                bool aceptado = reensambladorEntrada.procesarSegmentoDatosOrdenado(tramaRecibida, eraDuplicado);
                uint8_t ventana = reensambladorEntrada.calcularVentanaSegmentosDisponible(perfil.tamanoMaximoSegmentoDatos);

                if (aceptado) {
                    ultimoSecuenciaConfirmada = tramaRecibida.numeroSecuencia;
                    uint8_t banderasAck = BANDERA_ACK | ((perfil.modoDuplex == MODO_HALF_DUPLEX) ? BANDERA_CEDER_TURNO_HD : 0);
                    gestorSalida.enviarTramaControlInmediata(banderasAck, tramaRecibida.numeroSecuencia, ventana);
                } else {
                    uint8_t banderasNack = BANDERA_NACK | ((perfil.modoDuplex == MODO_HALF_DUPLEX) ? BANDERA_CEDER_TURNO_HD : 0);
                    gestorSalida.enviarTramaControlInmediata(
                        banderasNack,
                        reensambladorEntrada.obtenerSecuenciaEsperada(),
                        ventana
                    );
                }
            }
        }

        gestorSalida.actualizarCicloEnvioYRetransmision();
    }

    // ¿Cómo lo hace?: Verifica si ReensambladorOrdenadoEntrante completó un paquete y vuelca los bytes en bufferDestino.
    size_t extraerMensajeRecibido(uint8_t* bufferDestino, size_t capacidadMaxima) {
        if (!reensambladorEntrada.hayMensajeCompletoDisponible()) {
            return 0;
        }
        return reensambladorEntrada.extraerMensajeReensamblado(bufferDestino, capacidadMaxima);
    }

    // ¿Cómo lo hace?: Retorna la estructura PerfilCapacidadesUART calculada por el adaptador de hardware.
    const PerfilCapacidadesUART& obtenerPerfilActual() const {
        return adaptadorUART.obtenerPerfilCapacidades();
    }
};

#endif