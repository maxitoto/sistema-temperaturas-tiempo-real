#ifndef RP_SENSOR_H
#define RP_SENSOR_H

#include <stdint.h>
#include <string.h>
#include <stdexcept>
#include "../../Controller/abstract/RPC_middleware_message.hpp"
#include "../../../../hardware/driver/gestorsensores/GestorSensores.hpp"
#include "../../../../hardware/driver/gestorsensores/SensorInicializador.hpp"
#include "../../../../database/repositories/repoSensor.hpp"
#include "../../../../database/repositories/repoSenso.hpp"
#include "../../../../database/repositories/repoCalibracion.hpp"
#include "../../../../database/repositories/repoAuditoriaSensores.hpp"
#include "../../../../rtos/EventosYColas.hpp"

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

struct ActualizarCalibracionAmbienteDTO {
    PgFloat altura;
    PgFloat presionAtmosferica;
    PgFloat tempCongelacionRef;
    PgFloat tempEbullicionRef;
    PgBool  activa;
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

using ListaSensoresDTO = PgList<ResumenSensorDTO, 12>;
static_assert(sizeof(ListaSensoresDTO) <= RPCBase::CAPACIDAD_BUFFER_COMPARTIDO - 8,
              "ListaSensoresDTO excede MTU RPC");
using ListaAuditoriaSensorDTO   = PgList<DbRuntime::Repositories::AuditoriaSensorDTO, 32>;

namespace RPSensor {

    using DbRuntime::Repositories::RepoSensor;
    using DbRuntime::Repositories::RepoSenso;
    using DbRuntime::Repositories::RepoCalibracion;
    using DbRuntime::Repositories::RepoAuditoriaSensores;
    using DbRuntime::Repositories::SensorDTO;
    using DbRuntime::Repositories::SensoDTO;
    using DbRuntime::Repositories::CalibracionDTO;
    using DbRuntime::Repositories::AuditoriaSensorDTO;
    using DbRuntime::Repositories::EstadisticasSensoresDTO;
    using DbRuntime::Repositories::ResumenEstadisticoSensoDTO;
    using DbRuntime::Repositories::EstadisticasAuditoriaSensoresDTO;

    inline void auditarAccionSensor(const char* accion, const char* macDetalle) {
        const uint8_t slot = RPCBase::contextoActual().slotClienteOrigen;
        RTOS::encolarAuditoria(
            RTOS::TipoRegistroDB::AUDITORIA_TRANSACCION,
            RPAuth::obtenerIdUsuarioSlot(slot),
            RPAuth::obtenerNombreUsuarioSlot(slot),
            macDetalle ? macDetalle : "SENSOR",
            accion
        );
    }

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
            const uint32_t tsUnixSeg = RTOS::RelojSistema::obtenerTimestampActual();

            HistorialSensoresDTO lote{};
            for (uint8_t i = 0; i < cant; ++i) {
                LecturaSensorDTO item{};
                item.timestampMs          = tsUnixSeg;
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
            const uint16_t lim = (filtro.limiteRegistros == 0 || filtro.limiteRegistros > 12) ? 12 : filtro.limiteRegistros;
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
            auditarAccionSensor("ACTUALIZAR_METADATOS_SENSOR", dto.mac.c_str());
            return aPerfilSensor(act);
        },
        RolUsuario::ADMIN
    );

    inline auto actualizarCalibracionSensor = llamadoSemantico.exactamenteUnaVez<ActualizarCalibracionSensorDTO, PerfilSensorDTO>(
        "actualizarCalibracionSensor",
        [](const ActualizarCalibracionSensorDTO& dto) -> PerfilSensorDTO {
            if (dto.umbralInf > dto.umbralSup) {
                throw std::invalid_argument("ValueError: umbral_inf no puede ser mayor que umbral_sup");
            }

            const bool enRam = gestorSensores.configurarUmbralesPorMAC(dto.mac.c_str(), dto.umbralSup, dto.umbralInf, false)
                            && gestorSensores.calibrarPuntosPorMAC(dto.mac.c_str(), dto.errorSup, dto.errorInf, dto.offset);

            if (!enRam) {
                RepoSensor::actualizarCalibracionYUmbralesPorMac(
                    dto.mac.c_str(),
                    dto.umbralSup,
                    dto.umbralInf,
                    dto.errorSup,
                    dto.errorInf,
                    dto.offset
                );
            }

            SensorDTO act = RepoSensor::exigirPorMac(dto.mac.c_str());
            auditarAccionSensor("CALIBRAR_SENSOR", dto.mac.c_str());
            return aPerfilSensor(act);
        },
        RolUsuario::ADMIN
    );

    inline auto eliminarSensorPorMac = llamadoSemantico.exactamenteUnaVez<ConsultaSensorMacDTO, void>(
        "eliminarSensorPorMac",
        [](const ConsultaSensorMacDTO& dto) {
            RepoSensor::cambiarEstadoActivoPorMac(dto.mac.c_str(), false);
            auditarAccionSensor("DESACTIVAR_SENSOR", dto.mac.c_str());
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

    inline auto obtenerCalibracionAmbiente = llamadoSemantico.alMenosUnaVez<void, CalibracionDTO>(
        "obtenerCalibracionAmbiente",
        []() -> CalibracionDTO {
            return RepoCalibracion::exigirCalibracion();
        },
        RolUsuario::NO_ADMIN
    );

    inline auto actualizarCalibracionAmbiente = llamadoSemantico.exactamenteUnaVez<ActualizarCalibracionAmbienteDTO, CalibracionDTO>(
        "actualizarCalibracionAmbiente",
        [](const ActualizarCalibracionAmbienteDTO& dto) -> CalibracionDTO {
            CalibracionDTO entrada{};
            entrada.idCalibracion      = 1;
            entrada.altura             = dto.altura;
            entrada.presionAtmosferica = dto.presionAtmosferica;
            entrada.tempCongelacionRef = dto.tempCongelacionRef;
            entrada.tempEbullicionRef  = dto.tempEbullicionRef;
            entrada.activa             = dto.activa;

            CalibracionDTO act = RepoCalibracion::actualizarTodo(entrada);
            SensorInicializador::invalidarCacheCalibracionGlobal();
            auditarAccionSensor("ACTUALIZAR_CALIBRACION_AMBIENTE", "GLOBAL");
            return act;
        },
        RolUsuario::ADMIN
    );

    inline auto listarAuditoriaSensorPorMac = llamadoSemantico.alMenosUnaVez<FiltroPorMacDTO, ListaAuditoriaSensorDTO>(
        "listarAuditoriaSensorPorMac",
        [](const FiltroPorMacDTO& filtro) -> ListaAuditoriaSensorDTO {
            const String macHex = filtro.macSensor.toHexString();
            const uint16_t lim  = (filtro.limiteRegistros == 0 || filtro.limiteRegistros > 32) ? 32 : filtro.limiteRegistros;

            SensorDTO sensor{};
            if (!RepoSensor::obtenerPorMac(macHex.c_str(), sensor)) {
                return ListaAuditoriaSensorDTO{};
            }

            auto filas = RepoAuditoriaSensores::listarPorIdSensor(sensor.idSensor, lim, 0);
            ListaAuditoriaSensorDTO salida{};
            for (const auto& f : filas) {
                if (!salida.agregar(f)) break;
            }
            return salida;
        },
        RolUsuario::NO_ADMIN
    );

    inline auto obtenerEstadisticasAuditoriaSensores = llamadoSemantico.alMenosUnaVez<void, EstadisticasAuditoriaSensoresDTO>(
        "obtenerEstadisticasAuditoriaSensores",
        []() -> EstadisticasAuditoriaSensoresDTO {
            return RepoAuditoriaSensores::obtenerEstadisticasGlobales();
        },
        RolUsuario::NO_ADMIN
    );

    struct FiltroPaginadoSensoDTO {
        PgInt         idSensor; // 0 = todos los sensores
        PgPosSmallInt limiteRegistros;
        PgPosSmallInt offsetRegistros;
    };

    struct CrearSensoManualDTO {
        PgInt        idSensor;
        PgFloat      temperaturaCruda;
        PgFloat      temperaturaCorregida;
        EstadoSensor estadoSensor;
    };

    struct IdentificadorRegistroDTO {
        PgInt id;
    };

    struct PurgarPorEpochDTO {
        PgEpoch timestampLimite;
    };

    struct ActualizarReporteAuditoriaSensorDTO {
        PgInt             idAuditoriaSensor;
        EstadoEnvioCorreo nuevoEstado;
    };

    using ListaSensoCompletaDTO = PgList<SensoDTO, 32>;

    // Listar tabla Senso completa con paginación (NO_ADMIN)
    inline auto listarSensosPaginados = llamadoSemantico.alMenosUnaVez<FiltroPaginadoSensoDTO, ListaSensoCompletaDTO>(
        "listarSensosPaginados",
        [](const FiltroPaginadoSensoDTO& f) -> ListaSensoCompletaDTO {
            const uint16_t lim = (f.limiteRegistros == 0 || f.limiteRegistros > 32) ? 32 : f.limiteRegistros;
            auto filas = RepoSenso::listarPaginados(f.idSensor, lim, f.offsetRegistros);

            ListaSensoCompletaDTO out{};
            for (const auto& r : filas) {
                if (!out.agregar(r)) break;
            }
            return out;
        },
        RolUsuario::NO_ADMIN
    );

    // Crear registro manual en Senso (ADMIN)
    inline auto crearSensoManual = llamadoSemantico.exactamenteUnaVez<CrearSensoManualDTO, SensoDTO>(
        "crearSensoManual",
        [](const CrearSensoManualDTO& dto) -> SensoDTO {
            SensoDTO creado = RepoSenso::registrarLectura(
                dto.idSensor,
                dto.temperaturaCruda,
                dto.temperaturaCorregida,
                dto.estadoSensor,
                static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual())
            );
            auditarAccionSensor("CREAR_SENSO_MANUAL", "SENSO");
            return creado;
        },
        RolUsuario::ADMIN
    );

    // Eliminar un registro de Senso por ID (ADMIN)
    inline auto eliminarSensoPorId = llamadoSemantico.exactamenteUnaVez<IdentificadorRegistroDTO, void>(
        "eliminarSensoPorId",
        [](const IdentificadorRegistroDTO& dto) {
            RepoSenso::eliminarPorId(dto.id);
            auditarAccionSensor("ELIMINAR_SENSO_ID", "SENSO");
        },
        RolUsuario::ADMIN
    );

    // Purgar registros antiguos de Senso (ADMIN)
    inline auto purgarSensosAnterioresA = llamadoSemantico.exactamenteUnaVez<PurgarPorEpochDTO, PgPosSmallInt>(
        "purgarSensosAnterioresA",
        [](const PurgarPorEpochDTO& dto) -> PgPosSmallInt {
            uint16_t borrados = RepoSenso::purgarAnterioresA(static_cast<int64_t>(dto.timestampLimite));
            auditarAccionSensor("PURGAR_SENSOS", "SENSO");
            return borrados;
        },
        RolUsuario::ADMIN
    );

    // Actualizar estado de reporte de AuditoriaSensores (ADMIN)
    inline auto actualizarEstadoReporteAuditoriaSensor = llamadoSemantico.exactamenteUnaVez<ActualizarReporteAuditoriaSensorDTO, AuditoriaSensorDTO>(
        "actualizarEstadoReporteAuditoriaSensor",
        [](const ActualizarReporteAuditoriaSensorDTO& dto) -> AuditoriaSensorDTO {
            return RepoAuditoriaSensores::actualizarEstadoReporte(dto.idAuditoriaSensor, dto.nuevoEstado);
        },
        RolUsuario::ADMIN
    );

    // Eliminar registro de AuditoriaSensores por ID (ADMIN)
    inline auto eliminarAuditoriaSensorPorId = llamadoSemantico.exactamenteUnaVez<IdentificadorRegistroDTO, void>(
        "eliminarAuditoriaSensorPorId",
        [](const IdentificadorRegistroDTO& dto) {
            RepoAuditoriaSensores::eliminarPorId(dto.id);
            auditarAccionSensor("ELIMINAR_AUD_SENSOR", "AUDITORIA_SENSORES");
        },
        RolUsuario::ADMIN
    );

}

#endif