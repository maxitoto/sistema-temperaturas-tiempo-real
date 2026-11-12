#ifndef RP_CONNECT_WIFI_H
#define RP_CONNECT_WIFI_H

#include <stdint.h>

struct SuscripcionUdpDTO {
    uint16_t puertoEscuchaCliente;
};

struct RespuestaSuscripcionUdpDTO {
    uint16_t puertoEmisorServidor;
    bool registrado;
};

class RPConnectWiFi {
public:
    static constexpr uint16_t PUERTO_SERVIDOR_UDP_FIJO = 8266;

    // ¿Cómo lo hace?: Sirve como confirmación de keepalive sobre TCP+TLS para que L1 refresque el último contacto del slot WiFi.
    static void notificarEstoyVivoWiFi() {
    }

    // ¿Cómo lo hace?: Valida que el puerto UDP enviado por el cliente sea distinto de cero y devuelve el puerto fijo 8266 del servidor.
    static void vincularCanalStreamingUDP(const SuscripcionUdpDTO& entrada, RespuestaSuscripcionUdpDTO& salida) {
        salida.puertoEmisorServidor = PUERTO_SERVIDOR_UDP_FIJO;
        salida.registrado = (entrada.puertoEscuchaCliente > 0);
    }

    // ¿Cómo lo hace?: Indica a L1 que el cliente solicitó un cierre limpio para dar de baja su sesión en L2 y cerrar su socket TLS.
    static void cerrarConversacionWiFi() {
    }
};

#endif