#ifndef RP_SENSOR_H
#define RP_SENSOR_H

#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../../../database/repositories/repoSensor.hpp"
#include "../../../../database/repositories/repoSenso.hpp"

struct FiltroPorMacDTO {
    PgMac         macSensor;
    PgPosSmallInt limiteRegistros;
};

struct FiltroPorMacYFechaDTO {
    PgMac         macSensor;
    PgEpoch       timestampDesde;
    PgEpoch       timestampHasta;
    PgPosSmallInt limiteRegistros;
};

struct LecturaSensorDTO {
    PgPosInt      timestampMs;
    PgPosSmallInt idSensor;
    PgMac         macSensor;
    PgFloat       temperaturaCorregida;
};

using HistorialSensoresDTO = PgList<LecturaSensorDTO, 32>;

struct ConsultaSensorMacDTO {
    PgChar<25> mac;
};

struct FiltroListadoSensoresDTO {
    PgPosSmallInt limiteRegistros;
    PgPosSmallInt offsetRegistros;
    PgBool        soloActivos;
};

struct ActualizarMetadatosSensorDTO {
    PgChar<25>  mac;
    PgChar<32>  nombre;
    PgChar<32>  ubicacion;
    PgChar<128> descripcion;
    PgBool      activa;
};

struct ActualizarCalibracionSensorDTO {
    PgChar<25> mac;
    PgFloat    umbralSup;
    PgFloat    umbralInf;
    PgFloat    errorSup;
    PgFloat    errorInf;
    PgFloat    offset;
};

struct PerfilSensorDTO {
    PgAuto      idSensor;
    PgChar<25>  mac;
    PgChar<32>  nombre;
    PgChar<32>  ubicacion;
    PgChar<128> descripcion;
    PgFloat     umbralSup;
    PgFloat     umbralInf;
    PgFloat     errorSup;
    PgFloat     errorInf;
    PgFloat     offset;
    PgBool      activa;
    PgDateTime  fechaCreacion;
    PgDateTime  fechaModificacion;
};

struct ResumenSensorDTO {
    PgAuto     idSensor;
    PgChar<25> mac;
    PgChar<32> nombre;
    PgChar<32> ubicacion;
    PgFloat    umbralSup;
    PgFloat    umbralInf;
    PgFloat    offset;
    PgBool     activa;
};

using ListaSensoresDTO = PgList<ResumenSensorDTO, 32>;

namespace RPSensor {

    using DbRuntime::Repositories::RepoSensor;
    using DbRuntime::Repositories::RepoSenso;
    using DbRuntime::Repositories::SensorDTO;
    using DbRuntime::Repositories::SensoDTO;
    using DbRuntime::Repositories::EstadisticasSensoresDTO;
    using DbRuntime::Repositories::ResumenEstadisticoSensoDTO;

    inline void hexStringABytes8(const char* hexStr, uint8_t out[8]) {
        memset(out, 0, 8);
        if (hexStr == nullptr) return;
        size_t idx = 0;
        for (size_t i = 0; hexStr[i] != '\0' && idx < 16; ++i) {
            const char c = hexStr[i];
            uint8_t nibble = 0;
            if (c >= '0' && c <= '9') nibble = static_cast<uint8_t>(c - '0');
            else if (c >= 'A' && c <= 'F') nibble = static_cast<uint8_t>(10 + (c - 'A'));
            else if (c >= 'a' && c <= 'f') nibble = static_cast<uint8_t>(10 + (c - 'a'));
            else continue;

            if ((idx & 1u) == 0) {
                out[idx >> 1] = static_cast<uint8_t>(nibble << 4);
            } else {
                out[idx >> 1] |= nibble;
            }
            idx++;
        }
    }

    inline PerfilSensorDTO aPerfilSensor(const SensorDTO& s) {
        PerfilSensorDTO out{};
        out.idSensor          = s.idSensor;
        out.mac               = s.mac;
        out.nombre            = s.nombre;
        out.ubicacion         = s.ubicacion;
        out.descripcion       = s.descripcion;
        out.umbralSup         = s.umbralSup;
        out.umbralInf         = s.umbralInf;
        out.errorSup          = s.errorSup;
        out.errorInf          = s.errorInf;
        out.offset            = s.offset;
        out.activa            = s.activa;
        out.fechaCreacion     = s.fechaCreacion;
        out.fechaModificacion = s.fechaModificacion;
        return out;
    }

    inline auto suscribirSensoresTiempoReal = llamadoSemantico.talVez<void, HistorialSensoresDTO>(
        "suscribirSensoresTiempoReal",
        []() -> HistorialSensoresDTO {
            SnapshotSensor conectados[GestorSensores::MAX_SENSORES];
            const uint8_t cant = gestorSensores.obtenerSensoresConectados(conectados);

            HistorialSensoresDTO lote{};
            for (uint8_t i = 0; i < cant; ++i) {
                LecturaSensorDTO item{};
                item.timestampMs          = conectados[i].timestampLecturaMs;
                item.idSensor             = static_cast<uint16_t>(conectados[i].idSensorDB > 0 ? conectados[i].idSensorDB : (i + 1));
                memcpy(item.macSensor.data, conectados[i].rom, 8);
                item.temperaturaCorregida = conectados[i].tempCorregida;
                if (!lote.agregar(item)) break;
            }
            return lote;
        }
    );

    inline auto obtenerCensosPorMac = llamadoSemantico.alMenosUnaVez<FiltroPorMacDTO, HistorialSensoresDTO>(
        "obtenerCensosPorMac",
        [](const FiltroPorMacDTO& filtro) -> HistorialSensoresDTO {
            const String macHex = filtro.macSensor.toHexString();
            const uint16_t lim  = (filtro.limiteRegistros == 0 || filtro.limiteRegistros > 32) ? 32 : filtro.limiteRegistros;

            SensorDTO sensor{};
            if (!RepoSensor::obtenerPorMac(macHex.c_str(), sensor)) {
                return HistorialSensoresDTO{};
            }

            auto filas = RepoSenso::listarPorIdSensor(sensor.idSensor, lim, 0);
            HistorialSensoresDTO salida{};
            for (const auto& f : filas) {
                LecturaSensorDTO item{};
                item.timestampMs          = static_cast<uint32_t>(f.fechaCreacion);
                item.idSensor             = static_cast<uint16_t>(f.idSensor);
                item.macSensor            = filtro.macSensor;
                item.temperaturaCorregida = f.temperaturaCorregida;
                if (!salida.agregar(item)) break;
            }
            return salida;
        }
    );

    inline auto obtenerCensosPorMacYFecha = llamadoSemantico.alMenosUnaVez<FiltroPorMacYFechaDTO, HistorialSensoresDTO>(
        "obtenerCensosPorMacYFecha",
        [](const FiltroPorMacYFechaDTO& filtro) -> HistorialSensoresDTO {
            if (filtro.timestampDesde > filtro.timestampHasta) {
                throw std::invalid_argument("ValueError: timestampDesde no puede ser mayor que timestampHasta");
            }

            const String macHex = filtro.macSensor.toHexString();
            const uint16_t lim  = (filtro.limiteRegistros == 0 || filtro.limiteRegistros > 32) ? 32 : filtro.limiteRegistros;

            SensorDTO sensor{};
            if (!RepoSensor::obtenerPorMac(macHex.c_str(), sensor)) {
                return HistorialSensoresDTO{};
            }

            auto filas = RepoSenso::listarPorIdSensorYRangoFecha(
                sensor.idSensor,
                 static_cast<int64_t>(filtro.timestampDesde),
                static_cast<int64_t>(filtro.timestampHasta),
                lim
            );

            HistorialSensoresDTO salida{};
            for (const auto& f : filas) {
                LecturaSensorDTO item{};
                item.timestampMs          = static_cast<uint32_t>(f.fechaCreacion);
                item.idSensor             = static_cast<uint16_t>(f.idSensor);
                item.macSensor            = filtro.macSensor;
                item.temperaturaCorregida = f.temperaturaCorregida;
                if (!salida.agregar(item)) break;
            }
            return salida;
        }
    );

    inline auto listarSensores = llamadoSemantico.alMenosUnaVez<FiltroListadoSensoresDTO, ListaSensoresDTO>(
        "listarSensores",
        [](const FiltroListadoSensoresDTO& filtro) -> ListaSensoresDTO {
            const uint16_t lim = (filtro.limiteRegistros == 0 || filtro.limiteRegistros > 32) ? 32 : filtro.limiteRegistros;
            auto filas = RepoSensor::listarSensoresPaginados(lim, filtro.offsetRegistros, filtro.soloActivos);

            ListaSensoresDTO salida{};
            for (const auto& s : filas) {
                ResumenSensorDTO item{};
                item.idSensor  = s.idSensor;
                item.mac       = s.mac;
                item.nombre    = s.nombre;
                item.ubicacion = s.ubicacion;
                item.umbralSup = s.umbralSup;
                item.umbralInf = s.umbralInf;
                item.offset    = s.offset;
                item.activa    = s.activa;
                if (!salida.agregar(item)) break;
            }
            return salida;
        },
        RolUsuario::NO_ADMIN
    );

    inline auto obtenerSensorPorMac = llamadoSemantico.alMenosUnaVez<ConsultaSensorMacDTO, PerfilSensorDTO>(
        "obtenerSensorPorMac",
        [](const ConsultaSensorMacDTO& consulta) -> PerfilSensorDTO {
            return aPerfilSensor(RepoSensor::exigirPorMac(consulta.mac.c_str()));
        },
        RolUsuario::NO_ADMIN
    );

    inline auto crearOActualizarMetadatosSensor = llamadoSemantico.exactamenteUnaVez<ActualizarMetadatosSensorDTO, PerfilSensorDTO>(
        "actualizarMetadatosSensor",
        [](const ActualizarMetadatosSensorDTO& dto) -> PerfilSensorDTO {
            RepoSensor::registrarOAsegurarPorMac(dto.mac.c_str(), dto.nombre.c_str(), dto.ubicacion.c_str());
            SensorDTO act = RepoSensor::actualizarMetadatosPorMac(
                dto.mac.c_str(),
                dto.nombre.c_str(),
                dto.ubicacion.c_str(),
                dto.descripcion.c_str(),
                dto.activa
            );
            return aPerfilSensor(act);
        },
        RolUsuario::ADMIN
    );

    inline auto actualizarCalibracionSensor = llamadoSemantico.exactamenteUnaVez<ActualizarCalibracionSensorDTO, PerfilSensorDTO>(
        "actualizarCalibracionSensor",
        [](const ActualizarCalibracionSensorDTO& dto) -> PerfilSensorDTO {
            SensorDTO act = RepoSensor::actualizarCalibracionYUmbralesPorMac(
                dto.mac.c_str(),
                dto.umbralSup,
                dto.umbralInf,
                dto.errorSup,
                dto.errorInf,
                dto.offset
            );
            // bool enRam = gestorSensores.configurarUmbralesPorMAC(mac, sup, inf, false)
            //           && gestorSensores.calibrarPuntosPorMAC(mac, errorSup, errorInf, offset);  // ambos persisten vía SensorInicializador
            // if (!enRam) RepoSensor::actualizarCalibracionYUmbralesPorMac(...);                 // sensor no conectado: solo BD
            // SensorDTO act = RepoSensor::exigirPorMac(mac);
            return aPerfilSensor(act);
        },
        RolUsuario::ADMIN
    );

    inline auto eliminarSensorPorMac = llamadoSemantico.exactamenteUnaVez<ConsultaSensorMacDTO, void>(
        "eliminarSensorPorMac",
        [](const ConsultaSensorMacDTO& dto) {
            RepoSensor::cambiarEstadoActivoPorMac(dto.mac.c_str(), false);
        },
        RolUsuario::ADMIN
    );

    inline auto obtenerEstadisticasSensores = llamadoSemantico.alMenosUnaVez<void, EstadisticasSensoresDTO>(
        "obtenerEstadisticasSensores",
        []() -> EstadisticasSensoresDTO {
            return RepoSensor::obtenerEstadisticasGlobales();
        },
        RolUsuario::NO_ADMIN
    );

    inline auto obtenerEstadisticasSensoPorMac = llamadoSemantico.alMenosUnaVez<ConsultaSensorMacDTO, ResumenEstadisticoSensoDTO>(
        "obtenerEstadisticasSensoPorMac",
        [](const ConsultaSensorMacDTO& dto) -> ResumenEstadisticoSensoDTO {
            SensorDTO sensor = RepoSensor::exigirPorMac(dto.mac.c_str());
            return RepoSenso::obtenerEstadisticasPorSensor(sensor.idSensor);
        },
        RolUsuario::NO_ADMIN
    );

}

#endif