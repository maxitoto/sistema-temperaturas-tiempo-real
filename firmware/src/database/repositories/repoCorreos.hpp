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

    struct CorreoDTO {
        PgAuto              idCorreo              = 0;
        PgChar<24>          nombre                = "";
        PgChar<24>          apellido              = "";
        PgChar<48>          correoElectronico     = "";
        TipoRecepcionCorreo tipoRecepcionMensajes = TipoRecepcionCorreo::RECIBIR_TODOS;
        EstadoEnvioCorreo   ultimoEstadoEnvio     = EstadoEnvioCorreo::PENDIENTE;
        PgBool              activa                = true;
        PgDateTime          fechaCreacion         = "";
        PgDateTime          fechaModificacion     = "";
    };

    class RepoCorreos {
    private:
        static constexpr const char* TABLA = "Correos";

        static bool esEmailValido(const char* email) {
            if (email == nullptr) return false;
            const char* arroba = strchr(email, '@');
            if (arroba == nullptr || arroba == email) return false;
            const char* punto = strchr(arroba, '.');
            return (punto != nullptr && *(punto + 1) != '\0');
        }

    public:
        static CorreoDTO mapear(const PgRowView& r) {
            CorreoDTO c{};
            c.idCorreo              = r.getInt32("id_correo");
            c.nombre                = r.getCStr("nombre");
            c.apellido              = r.getCStr("apellido");
            c.correoElectronico     = r.getCStr("correo_electronico");
            c.tipoRecepcionMensajes = r.getEnum<TipoRecepcionCorreo>("tipo_recepcion_mensajes");
            c.ultimoEstadoEnvio     = r.getEnum<EstadoEnvioCorreo>("ultimo_estado_envio");
            c.activa                = r.getBool("activa", true);
            c.fechaCreacion         = r.getCStr("fecha_creacion");
            c.fechaModificacion     = r.getCStr("fecha_modificacion");
            return c;
        }

        static bool obtenerPorEmail(const char* email, CorreoDTO& outDto) {
            asegurarEsquemaTabla(TABLA);
            if (!email || email[0] == '\0') return false;

            bool encontrado = false;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (strcasecmp(r.getCStr("correo_electronico"), email) == 0) {
                    outDto = mapear(r);
                    encontrado = true;
                    return false;
                }
                return true;
            });
            return encontrado;
        }

        static CorreoDTO exigirPorEmail(const char* email) {
            CorreoDTO c{};
            if (!obtenerPorEmail(email, c)) {
                throw std::runtime_error("LookupError: el destinatario de correo no existe");
            }
            return c;
        }

        static std::vector<CorreoDTO> listarPaginados(
            uint16_t limite = 12,
            uint16_t offset = 0,
            bool soloActivos = false
        ) {
            asegurarEsquemaTabla(TABLA);
            std::vector<CorreoDTO> out;
            out.reserve(limite <= 32 ? limite : 32);

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

        static std::vector<CorreoDTO> listarDestinatariosParaCriticidad(CriticidadEvento criticidad) {
            auto activos = listarPaginados(16, 0, true);
            if (criticidad == CriticidadEvento::ALTA) return activos;

            std::vector<CorreoDTO> filtrados;
            for (const auto& d : activos) {
                if (criticidad == CriticidadEvento::NULA) {
                    if (d.tipoRecepcionMensajes == TipoRecepcionCorreo::RECIBIR_TODOS) filtrados.push_back(d);
                } else {
                    if (d.tipoRecepcionMensajes != TipoRecepcionCorreo::RECIBIR_EVENTOS_CRITICOS) filtrados.push_back(d);
                }
            }
            return filtrados;
        }

        static CorreoDTO crearCorreo(
            const char* nombre,
            const char* apellido,
            const char* email,
            TipoRecepcionCorreo tipoRecepcion = TipoRecepcionCorreo::RECIBIR_TODOS,
            bool activa = true
        ) {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            if (!nombre || nombre[0] == '\0') {
                throw std::invalid_argument("ValueError: el nombre del destinatario es obligatorio");
            }
            if (!esEmailValido(email)) {
                throw std::invalid_argument("ValueError: el formato de correo_electronico es invalido");
            }

            CorreoDTO dummy{};
            if (obtenerPorEmail(email, dummy)) {
                throw std::runtime_error("UniqueViolation: el correo_electronico ya esta registrado");
            }

            PgRowBuffer row(schema);
            row.setText("nombre",                  nombre);
            row.setText("apellido",                apellido ? apellido : "");
            row.setText("correo_electronico",      email);
            row.setEnum("tipo_recepcion_mensajes", tipoRecepcion);
            row.setEnum("ultimo_estado_envio",     EstadoEnvioCorreo::PENDIENTE);
            row.setBool("activa",                  activa);

            if (!PgBinaryStorage::getInstance().appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo crear el correo");
            }
            return mapear(row.view());
        }

        static CorreoDTO actualizarPorId(
            int32_t idCorreo,
            const char* nombre,
            const char* apellido,
            const char* email,
            TipoRecepcionCorreo tipoRecepcion,
            bool activa
        ) {
            asegurarEsquemaTabla(TABLA);
            if (!nombre || nombre[0] == '\0' || !esEmailValido(email)) {
                throw std::invalid_argument("ValueError: nombre y correo_electronico validos son obligatorios");
            }

            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_correo") == idCorreo; },
                [&](PgRowBuffer& row) {
                    row.setText("nombre",                  nombre);
                    row.setText("apellido",                apellido ? apellido : "");
                    row.setText("correo_electronico",      email);
                    row.setEnum("tipo_recepcion_mensajes", tipoRecepcion);
                    row.setBool("activa",                  activa);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: no se encontro el id_correo a actualizar");
            }
            return mapear(updated.view());
        }

        static CorreoDTO actualizarPorEmail(
            const char* email,
            const char* nombre,
            const char* apellido,
            TipoRecepcionCorreo tipoRecepcion,
            bool activa
        ) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return email && strcasecmp(r.getCStr("correo_electronico"), email) == 0; },
                [&](PgRowBuffer& row) {
                    row.setText("nombre",                  nombre ? nombre : "");
                    row.setText("apellido",                apellido ? apellido : "");
                    row.setEnum("tipo_recepcion_mensajes", tipoRecepcion);
                    row.setBool("activa",                  activa);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el correo a actualizar no existe");
            }
            return mapear(updated.view());
        }

        static CorreoDTO registrarEstadoEnvioPorEmail(const char* email, EstadoEnvioCorreo estado) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return email && strcasecmp(r.getCStr("correo_electronico"), email) == 0; },
                [&](PgRowBuffer& row) {
                    row.setEnum("ultimo_estado_envio", estado);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el correo no existe");
            }
            return mapear(updated.view());
        }

        static void eliminarPorId(int32_t idCorreo) {
            asegurarEsquemaTabla(TABLA);
            PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_correo") == idCorreo; },
                true
            );
        }

        static void eliminarPorEmail(const char* email) {
            asegurarEsquemaTabla(TABLA);
            const uint16_t n = PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return email && strcasecmp(r.getCStr("correo_electronico"), email) == 0; },
                true
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el destinatario a eliminar no existe");
            }
        }
    };

} // namespace DbRuntime::Repositories