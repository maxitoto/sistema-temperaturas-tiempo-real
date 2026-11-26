#pragma once
#include <Arduino.h>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    struct ServidorNtpDTO {
        PgAuto     idServer          = 0;
        PgChar<40> server            = "";
        PgSmallInt prioridad         = 1;
        PgBool     activa            = true;
        PgBigInt   fechaCreacion     = 0;
        PgBigInt   fechaModificacion = 0;
    };

    class RepoServidoresNtp {
    private:
        static constexpr const char* TABLA = "ServidoresNtp";

        static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

        static int16_t clampPrioridad(int16_t p) {
            return (p < 1) ? 1 : ((p > 10) ? 10 : p);
        }

        static void asegurarSemillaPorDefecto() {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            auto& st = PgBinaryStorage::getInstance();

            DbCommon::Standard::PgBinHeader hdr{};
            if (st.getTableHeader(TABLA, hdr) && hdr.activeRows > 0) {
                return;
            }

            const int64_t ahora = epochActual();
            TransaccionBatch tx;
            PgRowBuffer r1(schema);
            r1.setText("server",             "pool.ntp.org");
            r1.setInt16("prioridad",         1);
            r1.setBool("activa",             true);
            r1.setInt64("fecha_creacion",    ahora);
            r1.setInt64("fecha_modificacion",ahora);
            st.appendRecord(TABLA, r1);

            PgRowBuffer r2(schema);
            r2.setText("server",             "time.google.com");
            r2.setInt16("prioridad",         2);
            r2.setBool("activa",             true);
            r2.setInt64("fecha_creacion",    ahora);
            r2.setInt64("fecha_modificacion",ahora);
            st.appendRecord(TABLA, r2);
            tx.confirmar();
        }

    public:
        static ServidorNtpDTO mapear(const PgRowView& r) {
            ServidorNtpDTO dto{};
            dto.idServer          = r.getInt32("id_server");
            dto.server            = r.getCStr("server");
            dto.prioridad         = r.getInt16("prioridad", 1);
            dto.activa            = r.getBool("activa", true);
            dto.fechaCreacion     = r.getInt64("fecha_creacion", 0);
            dto.fechaModificacion = r.getInt64("fecha_modificacion", 0);
            return dto;
        }

        static std::vector<ServidorNtpDTO> listarActivosPorPrioridad(uint16_t limite = 16) {
            asegurarSemillaPorDefecto();
            std::vector<ServidorNtpDTO> todos;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (r.getBool("activa", true)) {
                    todos.push_back(mapear(r));
                }
                return todos.size() < 32;
            });
            std::sort(todos.begin(), todos.end(), [](const ServidorNtpDTO& a, const ServidorNtpDTO& b) {
                return (a.prioridad != b.prioridad) ? (a.prioridad < b.prioridad) : (a.idServer < b.idServer);
            });
            if (todos.size() > limite) todos.resize(limite);
            return todos;
        }

        static std::vector<ServidorNtpDTO> listarPaginados(uint16_t limite = 16, uint16_t offset = 0) {
            asegurarSemillaPorDefecto();
            std::vector<ServidorNtpDTO> todos;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                todos.push_back(mapear(r));
                return todos.size() < 32;
            });
            std::sort(todos.begin(), todos.end(), [](const ServidorNtpDTO& a, const ServidorNtpDTO& b) {
                return (a.prioridad != b.prioridad) ? (a.prioridad < b.prioridad) : (a.idServer < b.idServer);
            });
            if (offset >= todos.size()) return {};
            const size_t fin = std::min<size_t>(todos.size(), static_cast<size_t>(offset) + limite);
            return std::vector<ServidorNtpDTO>(todos.begin() + offset, todos.begin() + fin);
        }

        static ServidorNtpDTO crearServidor(const char* host, int16_t prioridad = 1, bool activa = true) {
            asegurarSemillaPorDefecto();
            const auto* schema = asegurarEsquemaTabla(TABLA);
            if (!host || host[0] == '\0') {
                throw std::invalid_argument("ValueError: el host NTP no puede estar vacio");
            }

            auto& st = PgBinaryStorage::getInstance();
            bool dup = false;
            st.scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (strcasecmp(r.getCStr("server"), host) == 0) { dup = true; return false; }
                return true;
            });
            if (dup) {
                throw std::runtime_error("UniqueViolation: ya existe un servidor NTP con ese host");
            }

            const int64_t ahora = epochActual();
            PgRowBuffer row(schema);
            row.setText("server",             host);
            row.setInt16("prioridad",         clampPrioridad(prioridad));
            row.setBool("activa",             activa);
            row.setInt64("fecha_creacion",    ahora);
            row.setInt64("fecha_modificacion",ahora);

            if (!st.appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo crear el servidor NTP");
            }
            return mapear(row.view());
        }

        static ServidorNtpDTO actualizarPorId(int32_t idServer, const char* host, int16_t prioridad, bool activa) {
            asegurarEsquemaTabla(TABLA);
            if (!host || host[0] == '\0') {
                throw std::invalid_argument("ValueError: el host NTP no puede estar vacio");
            }

            const int64_t ahora = epochActual();
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_server") == idServer; },
                [&](PgRowBuffer& row) {
                    row.setText("server",             host);
                    row.setInt16("prioridad",         clampPrioridad(prioridad));
                    row.setBool("activa",             activa);
                    row.setInt64("fecha_modificacion",ahora);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el servidor NTP solicitado no existe");
            }
            return mapear(updated.view());
        }

        static ServidorNtpDTO actualizarPorHost(const char* host, int16_t prioridad, bool activa) {
            asegurarEsquemaTabla(TABLA);
            const int64_t ahora = epochActual();
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return host && strcmp(r.getCStr("server"), host) == 0; },
                [&](PgRowBuffer& row) {
                    row.setInt16("prioridad",         clampPrioridad(prioridad));
                    row.setBool("activa",             activa);
                    row.setInt64("fecha_modificacion",ahora);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el servidor NTP solicitado no existe");
            }
            return mapear(updated.view());
        }

        static void eliminarPorId(int32_t idServer) {
            asegurarEsquemaTabla(TABLA);
            PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_server") == idServer; },
                true
            );
        }

        static void eliminarPorHost(const char* host) {
            asegurarEsquemaTabla(TABLA);
            const uint16_t n = PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return host && strcmp(r.getCStr("server"), host) == 0; },
                true
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el servidor NTP a eliminar no existe");
            }
        }
    };

} // namespace DbRuntime::Repositories