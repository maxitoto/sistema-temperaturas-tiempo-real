#ifndef RP_SENSOR_H
#define RP_SENSOR_H

#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../hardware/driver/gestorsensores/GestorSensores.hpp"

struct FiltroPorMacDTO {
    uint8_t  macSensor[8];
    uint16_t limiteRegistros;
};

struct FiltroPorMacYFechaDTO {
    uint8_t  macSensor[8];
    uint32_t timestampDesde;
    uint32_t timestampHasta;
    uint16_t limiteRegistros;
};

struct LecturaSensorDTO {
    uint32_t timestampMs;
    uint16_t idSensor;
    uint8_t  macSensor[8];
    float    temperaturaCorregida;
};

struct HistorialSensoresDTO {
    uint8_t          cantidadLecturas;
    LecturaSensorDTO lecturas[32];
};

namespace RPSensor {

    // 1. Servicio de streaming en tiempo real con semántica talVez:
    //    Al llamarlo el cliente queda suscrito y recibe este retorno periódicamente.
    inline auto suscribirSensoresTiempoReal = llamadoSemantico.talVez<void, HistorialSensoresDTO>(
        "suscribirSensoresTiempoReal",
        []() -> HistorialSensoresDTO {
            SnapshotSensor conectados[GestorSensores::MAX_SENSORES];
            const uint8_t cant = gestorSensores.obtenerSensoresConectados(conectados);

            HistorialSensoresDTO lote{};
            lote.cantidadLecturas = cant;
            for (uint8_t i = 0; i < cant; ++i) {
                lote.lecturas[i].timestampMs          = conectados[i].timestampLecturaMs;
                lote.lecturas[i].idSensor             = static_cast<uint16_t>(conectados[i].idSensorDB > 0 ? conectados[i].idSensorDB : (i + 1));
                memcpy(lote.lecturas[i].macSensor, conectados[i].rom, 8);
                lote.lecturas[i].temperaturaCorregida = conectados[i].tempCorregida;
            }
            return lote;
        }
    );

    // 2. Obtener censos por MAC (el límite de 32 lo valida y arroja directamente el SGBD)
    inline auto obtenerCensosPorMac = llamadoSemantico.alMenosUnaVez<FiltroPorMacDTO, HistorialSensoresDTO>(
        "obtenerCensosPorMac",
        [](const FiltroPorMacDTO& filtro) -> HistorialSensoresDTO {
            (void)filtro;
            HistorialSensoresDTO salida{};
            // consulta DB (al pasar filtro.limiteRegistros al SGBD, si supera 32 el SGBD lanza std::runtime_error)
            return salida;
        }
    );

    // 3. Obtener censos por MAC y fecha
    inline auto obtenerCensosPorMacYFecha = llamadoSemantico.alMenosUnaVez<FiltroPorMacYFechaDTO, HistorialSensoresDTO>(
        "obtenerCensosPorMacYFecha",
        [](const FiltroPorMacYFechaDTO& filtro) -> HistorialSensoresDTO {
            if (filtro.timestampDesde > filtro.timestampHasta) {
                throw std::invalid_argument("ValueError: timestampDesde no puede ser mayor que timestampHasta");
            }

            HistorialSensoresDTO salida{};
            // consulta DB (el SGBD arroja error si filtro.limiteRegistros > 32)
            return salida;
        }
    );

} // namespace RPSensor

#endif