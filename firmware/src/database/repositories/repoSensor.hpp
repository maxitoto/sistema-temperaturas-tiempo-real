#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    struct SensorDTO {
        PgAuto      idSensor          = 0;
        PgChar<25>  mac               = "";
        PgChar<32>  nombre            = "";
        PgChar<32>  ubicacion         = "";
        PgChar<128> descripcion       = "";
        PgFloat     umbralSup         = 126.0f;
        PgFloat     umbralInf         = -55.0f;
        PgFloat     errorSup          = 0.0f;
        PgFloat     errorInf          = 0.0f;
        PgFloat     offset            = 0.0f;
        PgBool      activa            = true;
        PgDateTime  fechaCreacion     = "";
        PgDateTime  fechaModificacion = "";
    };

    struct EstadisticasSensoresDTO {
        PgInt totalSensores     = 0;
        PgInt sensoresActivos   = 0;
        PgInt sensoresInactivos = 0;
    };

    class RepoSensor {
    private:
        static constexpr const char* TABLA = "Sensor";

    public:
        static SensorDTO mapear(const PgRowView& r) {
            SensorDTO s{};
            s.idSensor          = r.getInt32("id_sensor");
            s.mac               = r.getCStr("mac");
            s.nombre            = r.getCStr("nombre");
            s.ubicacion         = r.getCStr("ubicacion");
            s.descripcion       = r.getCStr("descripcion");
            s.umbralSup         = r.getFloat("umbral_sup", 126.0f);
            s.umbralInf         = r.getFloat("umbral_inf", -55.0f);
            s.errorSup          = r.getFloat("error_sup", 0.0f);
            s.errorInf          = r.getFloat("error_inf", 0.0f);
            s.offset            = r.getFloat("offset", 0.0f);
            s.activa            = r.getBool("activa", true);
            s.fechaCreacion     = r.getCStr("fecha_creacion");
            s.fechaModificacion = r.getCStr("fecha_modificacion");
            return s;
        }

        static bool obtenerPorId(int32_t idSensor, SensorDTO& outDto) {
            asegurarEsquemaTabla(TABLA);
            bool ok = false;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getInt32("id_sensor") == idSensor) {
                    outDto = mapear(r);
                    ok = true;
                    return false;
                }
                return true;
            });
            return ok;
        }

        static bool existePorMac(const char* macHex) {
            if (!macHex || macHex[0] == '\0') return false;
            bool existe = false;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (strcasecmp(r.getCStr("mac"), macHex) == 0) {
                    existe = true;
                    return false;
                }
                return true;
            });
            return existe;
        }

        static SensorDTO exigirPorId(int32_t idSensor) {
            SensorDTO s{};
            if (!obtenerPorId(idSensor, s)) {
                throw std::runtime_error("LookupError: el id_sensor solicitado no existe");
            }
            return s;
        }

        static bool obtenerPorMac(const char* macHex, SensorDTO& outDto) {
            asegurarEsquemaTabla(TABLA);
            if (!macHex || macHex[0] == '\0') return false;

            bool ok = false;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (strcasecmp(r.getCStr("mac"), macHex) == 0) {
                    outDto = mapear(r);
                    ok = true;
                    return false;
                }
                return true;
            });
            return ok;
        }

        static SensorDTO exigirPorMac(const char* macHex) {
            SensorDTO s{};
            if (!obtenerPorMac(macHex, s)) {
                throw std::runtime_error("LookupError: no existe un sensor registrado con esa MAC");
            }
            return s;
        }

        static SensorDTO crearSensor(const SensorDTO& entrada) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            if (entrada.mac.vacio()) {
                throw std::invalid_argument("ValueError: la direccion MAC del sensor es obligatoria");
            }
            if (entrada.umbralInf > entrada.umbralSup) {
                throw std::invalid_argument("ValueError: umbral_inf no puede ser mayor que umbral_sup");
            }

            
            if (existePorMac(entrada.mac.c_str())) {
                throw std::runtime_error("UniqueViolation: ya existe un sensor con esa MAC");
            }

            PgRowBuffer row(schema);
            row.setText("mac",         entrada.mac.c_str());
            row.setText("nombre",      entrada.nombre.c_str());
            row.setText("ubicacion",   entrada.ubicacion.c_str());
            row.setText("descripcion", entrada.descripcion.c_str());
            row.setFloat("umbral_sup", entrada.umbralSup);
            row.setFloat("umbral_inf", entrada.umbralInf);
            row.setFloat("error_sup",  entrada.errorSup);
            row.setFloat("error_inf",  entrada.errorInf);
            row.setFloat("offset",     entrada.offset);
            row.setBool("activa",      entrada.activa);

            if (!PgBinaryStorage::getInstance().appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo registrar el sensor");
            }
            return mapear(row.view());
        }

        __attribute__((noinline))
        static bool asegurarPorMacEnDestino(
            const char* macHex,
            SensorDTO& outDto,
            const char* nombreInicial = "Sensor DS18B20",
            const char* ubicacionInicial = "Sin asignar"
        ) {
            if (obtenerPorMac(macHex, outDto)) {
                return true;
            }
            if (!macHex || strlen(macHex) != 16) {
                throw std::invalid_argument("ValueError: mac debe tener 16 caracteres hex");
            }

            const auto* schema = asegurarEsquemaTabla(TABLA);
            PgRowBuffer row(schema);
            row.setText("mac",         macHex);
            row.setText("nombre",      nombreInicial ? nombreInicial : "Sensor DS18B20");
            row.setText("ubicacion",   ubicacionInicial ? ubicacionInicial : "Sin asignar");
            row.setText("descripcion", "");
            row.setFloat("umbral_sup", 126.0f);
            row.setFloat("umbral_inf", -55.0f);
            row.setFloat("error_sup",  0.0f);
            row.setFloat("error_inf",  0.0f);
            row.setFloat("offset",     0.0f);
            row.setBool("activa",      true);

            if (!PgBinaryStorage::getInstance().appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo registrar el sensor");
            }
            outDto = mapear(row.view());
            return true;
        }

        static SensorDTO registrarOAsegurarPorMac(
            const char* macHex,
            const char* nombreInicial = "Sensor DS18B20",
            const char* ubicacionInicial = "Sin asignar"
        ) {
            SensorDTO out{};
            asegurarPorMacEnDestino(macHex, out, nombreInicial, ubicacionInicial);
            return out;
        }

        static SensorDTO actualizarMetadatosPorMac(
            const char* macHex,
            const char* nombre,
            const char* ubicacion,
            const char* descripcion,
            bool activa
        ) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return macHex && strcasecmp(r.getCStr("mac"), macHex) == 0; },
                [&](PgRowBuffer& row) {
                    row.setText("nombre",      nombre ? nombre : "");
                    row.setText("ubicacion",   ubicacion ? ubicacion : "");
                    row.setText("descripcion", descripcion ? descripcion : "");
                    row.setBool("activa",      activa);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el sensor a actualizar no existe");
            }
            return mapear(updated.view());
        }

        static SensorDTO actualizarCalibracionYUmbralesPorMac(
            const char* macHex,
            float umbralSup,
            float umbralInf,
            float errorSup,
            float errorInf,
            float offset
        ) {
            asegurarEsquemaTabla(TABLA);
            if (umbralInf > umbralSup) {
                throw std::invalid_argument("ValueError: umbral_inf no puede ser mayor que umbral_sup");
            }

            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return macHex && strcasecmp(r.getCStr("mac"), macHex) == 0; },
                [&](PgRowBuffer& row) {
                    row.setFloat("umbral_sup", umbralSup);
                    row.setFloat("umbral_inf", umbralInf);
                    row.setFloat("error_sup",  errorSup);
                    row.setFloat("error_inf",  errorInf);
                    row.setFloat("offset",     offset);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el sensor a calibrar no existe");
            }
            return mapear(updated.view());
        }

        static SensorDTO cambiarEstadoActivoPorMac(const char* macHex, bool activa) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return macHex && strcasecmp(r.getCStr("mac"), macHex) == 0; },
                [&](PgRowBuffer& row) { row.setBool("activa", activa); },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el sensor solicitado no existe");
            }
            return mapear(updated.view());
        }

        static void eliminarPorMac(const char* macHex) {
            asegurarEsquemaTabla(TABLA);
            const uint16_t n = PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return macHex && strcasecmp(r.getCStr("mac"), macHex) == 0; },
                true
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el sensor a eliminar no existe");
            }
        }

        static std::vector<SensorDTO> listarSensoresPaginados(
            uint16_t limite = 12,
            uint16_t offset = 0,
            bool soloActivos = false
        ) {
            asegurarEsquemaTabla(TABLA);
            std::vector<SensorDTO> out;
            out.reserve(limite <= 16 ? limite : 16);

            uint32_t matched = 0;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (soloActivos && !r.getBool("activa", true)) return true;
                if (matched >= offset) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                matched++;
                return true;
            });
            return out;
        }

        static std::vector<SensorDTO> listarPorUbicacion(const char* ubicacion, uint16_t limite = 12) {
            asegurarEsquemaTabla(TABLA);
            std::vector<SensorDTO> out;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (ubicacion && strcasecmp(r.getCStr("ubicacion"), ubicacion) == 0) {
                    out.push_back(mapear(r));
                    if (out.size() >= limite) return false;
                }
                return true;
            });
            return out;
        }

        static EstadisticasSensoresDTO obtenerEstadisticasGlobales() {
            asegurarEsquemaTabla(TABLA);
            EstadisticasSensoresDTO out{};
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                out.totalSensores++;
                if (r.getBool("activa", true)) out.sensoresActivos++;
                else out.sensoresInactivos++;
                return true;
            });
            return out;
        }
    };

} // namespace DbRuntime::Repositories