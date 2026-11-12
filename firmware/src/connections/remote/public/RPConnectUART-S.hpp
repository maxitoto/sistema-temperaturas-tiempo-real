#ifndef RP_CONNECT_UARTS_H
#define RP_CONNECT_UARTS_H

#include <stdint.h>

struct HandshakeUartsDTO {
    uint8_t versionProtocolo;
};

struct EstadoEnlaceUartsDTO {
    bool aceptado;
};

class RPConnectUARTS {
public:
    // ¿Cómo lo hace?: Comprueba que versionProtocolo sea 1 y marca aceptado en true para que L1 habilite el slot 0 (UART-S).
    static void iniciarConversacionSerie(const HandshakeUartsDTO& entrada, EstadoEnlaceUartsDTO& salida) {
        salida.aceptado = (entrada.versionProtocolo == 1);
    }

    // ¿Cómo lo hace?: Actúa como procedimiento vacío de pulso de vida; la recepción del paquete en L1 ya actualiza el temporizador del slot 0.
    static void notificarEstoyVivoSerie() {
    }

    // ¿Cómo lo hace?: Actúa como señal formal de desconexión para que L1 notifique a L2 y reinicie el enlace serie en UARTSRuntime.
    static void cerrarConversacionSerie() {
    }
};

#endif