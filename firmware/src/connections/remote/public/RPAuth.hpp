#ifndef RP_AUTH_H
#define RP_AUTH_H

#include <stdint.h>
#include <string.h>
#include "../Controller/abstract/RPC_base.hpp"

struct CredencialesLoginDTO {
    char usuario[32];
    char clavePlana[32];
};

struct ResultadoLoginDTO {
    bool autenticado;
    uint8_t rolAsignado;
    uint32_t tiempoExpiracionSegundos;
};

class RPAuth {
public:
    static constexpr uint32_t EXPIRACION_SESION_SEGUNDOS = 1800000;   // 30 minutos

    static void iniciarSesionUsuario(const CredencialesLoginDTO& entrada, ResultadoLoginDTO& salida) {
        salida.autenticado = false;
        salida.rolAsignado = static_cast<uint8_t>(RolUsuario::NINGUNO);
        salida.tiempoExpiracionSegundos = 0;

        if (entrada.usuario[0] == '\0' || entrada.clavePlana[0] == '\0') {
            return;
        }

        // consulta DB

        if (strcmp(entrada.usuario, "admin") == 0 && strcmp(entrada.clavePlana, "admin") == 0) {
            salida.autenticado = true;
            salida.rolAsignado = static_cast<uint8_t>(RolUsuario::ADMIN);
            salida.tiempoExpiracionSegundos = EXPIRACION_SESION_SEGUNDOS;
        } else if (strcmp(entrada.usuario, "operador") == 0 && strcmp(entrada.clavePlana, "operador") == 0) {
            salida.autenticado = true;
            salida.rolAsignado = static_cast<uint8_t>(RolUsuario::NO_ADMIN);
            salida.tiempoExpiracionSegundos = EXPIRACION_SESION_SEGUNDOS;
        }
    }

    static void cerrarSesionUsuario() {
    }
};

#endif