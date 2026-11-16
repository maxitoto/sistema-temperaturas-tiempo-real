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
        reensambladorEntrada.reiniciar();
        gestorSalida.reiniciarColaSalida();
    }

    void marcarClienteConectado() {
        clienteConectado.store(true, std::memory_order_relaxed);
    }

    bool hayClienteConectado() const {
        return clienteConectado.load(std::memory_order_relaxed);
    }

    // Segmenta y encola usando 1 sola TramaSegmentoUARTS (121 bytes en stack en vez de 1936 bytes)
    bool enviarMensajeConfiable(const uint8_t* datos, size_t longitud) {
        if (datos == nullptr || longitud == 0) {
            return false;
        }

        const PerfilCapacidadesUART& perfil = adaptadorUART.obtenerPerfilCapacidades();
        if (perfil.tamanoMaximoSegmentoDatos == 0) {
            return false;
        }

        const uint8_t pasoCorte = (perfil.tamanoMaximoSegmentoDatos > TAMANO_MAXIMO_PAYLOAD_ESTATICO)
                                  ? TAMANO_MAXIMO_PAYLOAD_ESTATICO
                                  : perfil.tamanoMaximoSegmentoDatos;

        const size_t segmentosRequeridos = (longitud + pasoCorte - 1) / pasoCorte;
        if (segmentosRequeridos == 0 || segmentosRequeridos > CAPACIDAD_COLA_SALIDA) {
            return false;
        }

        // Si la cola tiene segmentos previos en tránsito, drena el protocolo hasta liberar espacio
        const uint32_t inicioEsperaMs = millis();
        while (gestorSalida.obtenerRanurasLibres() < segmentosRequeridos) {
            if (!hayClienteConectado() || (millis() - inicioEsperaMs) > 600) {
                return false;
            }
            ejecutarCicloProtocolo();
            delay(1);
        }

        const uint8_t ventanaLocal = reensambladorEntrada.calcularVentanaSegmentosDisponible(pasoCorte);
        const bool esHalfDuplex = (perfil.modoDuplex == MODO_HALF_DUPLEX);

        size_t offset = 0;
        TramaSegmentoUARTS seg{};

        while (offset < longitud) {
            const size_t restantes = longitud - offset;
            const uint8_t longitudTrozo = (restantes > pasoCorte) ? pasoCorte : static_cast<uint8_t>(restantes);
            const bool hayMasFragmentos = (offset + longitudTrozo) < longitud;

            seg.inicioTrama = BYTE_INICIO_TRAMA;
            seg.banderas = BANDERA_DATOS;
            if (hayMasFragmentos) {
                seg.banderas |= BANDERA_MAS_FRAGMENTOS;
            }
            if (esHalfDuplex && !hayMasFragmentos) {
                seg.banderas |= BANDERA_CEDER_TURNO_HD;
            }

            seg.numeroSecuencia = contadorSecuenciaSalida++;
            seg.numeroConfirmacion = ultimoSecuenciaConfirmada;
            seg.ventanaRecepcion = ventanaLocal;
            seg.longitudDatos = longitudTrozo;
            memcpy(seg.datosUtiles, datos + offset, longitudTrozo);
            seg.finTrama = BYTE_FIN_TRAMA;

            gestorSalida.encolarSegmentoParaEnvio(seg);
            offset += longitudTrozo;
        }

        gestorSalida.actualizarCicloEnvioYRetransmision();
        return true;
    }

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
                reensambladorEntrada.reiniciar();
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
                // Si el cliente habla directamente con SEQ:0 tras estar desconectado, sincroniza en caliente
                if (!clienteConectado.load(std::memory_order_relaxed) && tramaRecibida.numeroSecuencia == 0) {
                    contadorSecuenciaSalida = 0;
                    ultimoSecuenciaConfirmada = 0;
                    reensambladorEntrada.reiniciar();
                    gestorSalida.reiniciarColaSalida();
                    clienteConectado.store(true, std::memory_order_relaxed);
                }

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