#pragma once
#include <Arduino.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/PostgresDataTypes.hpp"
#include "RepositorioBase.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Storage::PgBinaryStorage;
    using DbRuntime::Storage::PgRowBuffer;
    using DbRuntime::Storage::PgRowView;

    struct UsuarioDTO {
        PgAuto     idUsuario           = 0;
        PgChar<20> usuario             = "";
        PgChar<32> salt                = "SALT";
        PgChar<64> contrasenaHash      = "";
        RolUsuario rol                 = RolUsuario::NINGUNO;
        PgSmallInt intentosFallidos    = 0;
        PgBool     bloqueado           = false;
        PgBigInt   bloqueadoDesde      = 0;
        PgBigInt   bloqueadoHasta      = 0;
        PgBigInt   fechaUltimoLogin    = 0;
        PgBigInt   fechaCambioPassword = 0;
        PgBool     activa              = true;
        PgDateTime creadoEl            = "";
        PgDateTime actualizadoEl       = "";
        PgEpoch    actualizadoEpoch    = 0;
    };

    struct EstadisticasUsuariosDTO {
        PgInt totalUsuarios       = 0;
        PgInt usuariosActivos     = 0;
        PgInt usuariosBloqueados  = 0;
        PgInt maxIntentosFallidos = 0;
    };

    class RepoUsuario {
    private:
        static constexpr const char* TABLA = "Usuario";
        static constexpr int64_t DURACION_BLOQUEO_SEGUNDOS = 600;

        static int64_t epochActual() {
            return static_cast<int64_t>(RTOS::RelojSistema::obtenerTimestampActual());
        }

        static void calcularHashConSalt(const char* clavePlana, const char* salt, char outHex[17]) {
            uint32_t h1 = 2166136261u;
            uint32_t h2 = 0x9E3779B9u;

            if (salt != nullptr) {
                for (size_t i = 0; salt[i] != '\0'; ++i) {
                    h1 ^= static_cast<uint8_t>(salt[i]);
                    h1 *= 16777619u;
                    h2 = (h2 ^ static_cast<uint8_t>(salt[i])) * 0x85EBCA6Bu;
                }
            }
            if (clavePlana != nullptr) {
                for (size_t i = 0; clavePlana[i] != '\0'; ++i) {
                    h1 ^= static_cast<uint8_t>(clavePlana[i]);
                    h1 *= 16777619u;
                    h2 = (h2 ^ static_cast<uint8_t>(clavePlana[i])) * 0xC2B2AE35u;
                }
            }

            snprintf(outHex, 17, "%08lX%08lX", static_cast<unsigned long>(h1), static_cast<unsigned long>(h2));
        }

        static void generarSaltDeterminista(const char* usuario, char outSalt[9]) {
            uint32_t acc = 0xA5A5F00Du;
            if (usuario != nullptr) {
                for (size_t i = 0; usuario[i] != '\0'; ++i) {
                    acc = (acc * 33u) ^ static_cast<uint8_t>(usuario[i]);
                }
            }
            acc ^= static_cast<uint32_t>(millis());
            snprintf(outSalt, 9, "S%07lX", static_cast<unsigned long>(acc & 0x0FFFFFFFu));
        }

        static bool coincideClave(const UsuarioDTO& u, const char* clavePlana) {
            if (!clavePlana) return false;
            char hashCalc[17]{};
            calcularHashConSalt(clavePlana, u.salt.c_str(), hashCalc);
            if (strcasecmp(hashCalc, u.contrasenaHash.c_str()) == 0) return true;
            return strcmp(u.contrasenaHash.c_str(), clavePlana) == 0;
        }

        static void asegurarSemillaPorDefecto() {
            const auto* schema = asegurarEsquemaTabla(TABLA);
            auto& st = PgBinaryStorage::getInstance();

            DbCommon::Standard::PgBinHeader hdr{};
            if (st.getTableHeader(TABLA, hdr) && hdr.activeRows > 0) {
                return;
            }

            const int64_t ahora = epochActual();
            char hashAdmin[17]{};
            calcularHashConSalt("admin", "SALTADM1", hashAdmin);

            PgRowBuffer rAdmin(schema);
            rAdmin.setText("usuario",               "admin");
            rAdmin.setText("salt",                  "SALTADM1");
            rAdmin.setText("contrasena_hash",       hashAdmin);
            rAdmin.setEnum("rol",                   RolUsuario::ADMIN);
            rAdmin.setInt16("intentos_fallidos",    0);
            rAdmin.setBool("bloqueado",             false);
            rAdmin.setInt64("bloqueado_desde",      0);
            rAdmin.setInt64("bloqueado_hasta",      0);
            rAdmin.setInt64("fecha_ultimo_login",   0);
            rAdmin.setInt64("fecha_cambio_password",ahora);
            rAdmin.setBool("activa",                true);
            st.appendRecord(TABLA, rAdmin);

            char hashOp[17]{};
            calcularHashConSalt("operador1234", "SALTOPE1", hashOp);

            PgRowBuffer rOp(schema);
            rOp.setText("usuario",                  "operador");
            rOp.setText("salt",                     "SALTOPE1");
            rOp.setText("contrasena_hash",          hashOp);
            rOp.setEnum("rol",                      RolUsuario::NO_ADMIN);
            rOp.setInt16("intentos_fallidos",       0);
            rOp.setBool("bloqueado",                false);
            rOp.setInt64("bloqueado_desde",         0);
            rOp.setInt64("bloqueado_hasta",         0);
            rOp.setInt64("fecha_ultimo_login",      0);
            rOp.setInt64("fecha_cambio_password",   ahora);
            rOp.setBool("activa",                   true);
            st.appendRecord(TABLA, rOp);
        }

    public:
        static UsuarioDTO mapear(const PgRowView& r) {
            UsuarioDTO u{};
            u.idUsuario           = r.getInt32("id_usuario");
            u.usuario             = r.getCStr("usuario");
            u.salt                = r.getCStr("salt", "SALT");
            u.contrasenaHash      = r.getCStr("contrasena_hash");
            u.rol                 = r.getEnum<RolUsuario>("rol", RolUsuario::NINGUNO);
            u.intentosFallidos    = r.getInt16("intentos_fallidos", 0);
            u.bloqueado           = r.getBool("bloqueado", false);
            u.bloqueadoDesde      = r.getInt64("bloqueado_desde", 0);
            u.bloqueadoHasta      = r.getInt64("bloqueado_hasta", 0);
            u.fechaUltimoLogin    = r.getInt64("fecha_ultimo_login", 0);
            u.fechaCambioPassword = r.getInt64("fecha_cambio_password", 0);
            u.activa              = r.getBool("activa", true);
            u.creadoEl            = r.getCStr("creado_el");
            u.actualizadoEl       = r.getCStr("actualizado_el");

            int64_t ep = 0;
            String stDummy, errDummy;
            if (!u.actualizadoEl.vacio() &&
                DbCommon::Standard::DataTypeUtils::parseIsoTimestamp(String(u.actualizadoEl.c_str()), false, ep, stDummy, errDummy) &&
                ep > 0) {
                u.actualizadoEpoch = static_cast<PgEpoch>(ep);
            }
            return u;
        }

        static bool obtenerPorNombreUsuario(const char* nombreUsuario, UsuarioDTO& outDto) {
            asegurarSemillaPorDefecto();
            if (!nombreUsuario || nombreUsuario[0] == '\0') return false;

            bool encontrado = false;
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                if (strcmp(r.getCStr("usuario"), nombreUsuario) == 0) {
                    outDto = mapear(r);
                    encontrado = true;
                    return false;
                }
                return true;
            });
            return encontrado;
        }

        static UsuarioDTO exigirPorNombreUsuario(const char* nombreUsuario) {
            UsuarioDTO u{};
            if (!obtenerPorNombreUsuario(nombreUsuario, u)) {
                throw std::runtime_error("LookupError: el usuario solicitado no existe");
            }
            return u;
        }

        static UsuarioDTO autenticarUsuario(const char* nombreUsuario, const char* clavePlana) {
            UsuarioDTO u{};
            if (!obtenerPorNombreUsuario(nombreUsuario, u)) {
                throw std::runtime_error("AuthenticationError: credenciales invalidas");
            }

            if (!u.activa) {
                throw std::runtime_error("PermissionError: la cuenta de usuario esta desactivada");
            }

            const int64_t ahora = epochActual();
            if (u.bloqueado && (u.bloqueadoHasta == 0 || ahora < u.bloqueadoHasta)) {
                throw std::runtime_error("AuthenticationError: usuario bloqueado por maximo de intentos fallidos");
            }

            if (!coincideClave(u, clavePlana)) {
                const int16_t nuevosIntentos = static_cast<int16_t>((u.intentosFallidos < 3) ? (u.intentosFallidos + 1) : 3);
                const bool bloquearAhora     = (nuevosIntentos >= 3);
                const int64_t desde          = bloquearAhora ? ahora : u.bloqueadoDesde;
                const int64_t hasta          = bloquearAhora ? (ahora + DURACION_BLOQUEO_SEGUNDOS) : u.bloqueadoHasta;

                PgBinaryStorage::getInstance().updateWhere(
                    TABLA,
                    [&](const PgRowView& r) -> bool { return r.getInt32("id_usuario") == u.idUsuario; },
                    [&](PgRowBuffer& row) {
                        row.setInt16("intentos_fallidos", nuevosIntentos);
                        row.setBool("bloqueado",          bloquearAhora);
                        row.setInt64("bloqueado_desde",   desde);
                        row.setInt64("bloqueado_hasta",   hasta);
                    }
                );

                if (bloquearAhora) {
                    throw std::runtime_error("AuthenticationError: usuario bloqueado tras 3 intentos fallidos");
                }
                throw std::runtime_error("AuthenticationError: credenciales invalidas");
            }

            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_usuario") == u.idUsuario; },
                [&](PgRowBuffer& row) {
                    row.setInt16("intentos_fallidos",  0);
                    row.setBool("bloqueado",           false);
                    row.setInt64("bloqueado_desde",    0);
                    row.setInt64("bloqueado_hasta",    0);
                    row.setInt64("fecha_ultimo_login", ahora);
                },
                &updated
            );
            if (n > 0) return mapear(updated.view());

            u.intentosFallidos = 0;
            u.bloqueado        = false;
            u.fechaUltimoLogin = ahora;
            return u;
        }

        static std::vector<UsuarioDTO> listarUsuariosPaginados(
            uint16_t limite = 32,
            uint16_t offset = 0,
            bool soloActivos = false
        ) {
            asegurarSemillaPorDefecto();
            std::vector<UsuarioDTO> out;
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

        static UsuarioDTO crearUsuario(const char* nombreUsuario, const char* clavePlana, RolUsuario rol) {
            asegurarSemillaPorDefecto();
            const auto* schema = asegurarEsquemaTabla(TABLA);

            if (!nombreUsuario || nombreUsuario[0] == '\0') {
                throw std::invalid_argument("ValueError: el nombre de usuario no puede estar vacio");
            }
            if (!clavePlana || strlen(clavePlana) < 4) {
                throw std::invalid_argument("ValueError: la contrasena debe tener al menos 4 caracteres");
            }

            UsuarioDTO dummy{};
            if (obtenerPorNombreUsuario(nombreUsuario, dummy)) {
                throw std::runtime_error("UniqueViolation: ya existe un usuario con ese nombre");
            }

            const int64_t ahora = epochActual();
            char nuevoSalt[9]{};
            char nuevoHash[17]{};
            generarSaltDeterminista(nombreUsuario, nuevoSalt);
            calcularHashConSalt(clavePlana, nuevoSalt, nuevoHash);

            PgRowBuffer row(schema);
            row.setText("usuario",               nombreUsuario);
            row.setText("salt",                  nuevoSalt);
            row.setText("contrasena_hash",       nuevoHash);
            row.setEnum("rol",                   rol);
            row.setInt16("intentos_fallidos",    0);
            row.setBool("bloqueado",             false);
            row.setInt64("bloqueado_desde",      0);
            row.setInt64("bloqueado_hasta",      0);
            row.setInt64("fecha_ultimo_login",   0);
            row.setInt64("fecha_cambio_password",ahora);
            row.setBool("activa",                true);

            if (!PgBinaryStorage::getInstance().appendRecord(TABLA, row)) {
                throw std::runtime_error("IOError: no se pudo crear el usuario");
            }
            return mapear(row.view());
        }

        static UsuarioDTO actualizarRolYEstado(const char* nombreUsuario, RolUsuario nuevoRol, bool activa) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return nombreUsuario && strcmp(r.getCStr("usuario"), nombreUsuario) == 0; },
                [&](PgRowBuffer& row) {
                    row.setEnum("rol",    nuevoRol);
                    row.setBool("activa", activa);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el usuario a actualizar no existe");
            }
            return mapear(updated.view());
        }

        static UsuarioDTO cambiarContrasena(
            const char* nombreUsuario,
            const char* claveActual,
            const char* claveNueva,
            bool verificarClaveActual = true
        ) {
            if (!claveNueva || strlen(claveNueva) < 4) {
                throw std::invalid_argument("ValueError: la nueva contrasena debe tener al menos 4 caracteres");
            }

            UsuarioDTO actual = exigirPorNombreUsuario(nombreUsuario);
            if (verificarClaveActual && !coincideClave(actual, claveActual)) {
                throw std::runtime_error("AuthenticationError: la contrasena actual es incorrecta");
            }

            const int64_t ahora = epochActual();
            char nuevoSalt[9]{};
            char nuevoHash[17]{};
            generarSaltDeterminista(nombreUsuario, nuevoSalt);
            calcularHashConSalt(claveNueva, nuevoSalt, nuevoHash);

            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return r.getInt32("id_usuario") == actual.idUsuario; },
                [&](PgRowBuffer& row) {
                    row.setText("salt",                  nuevoSalt);
                    row.setText("contrasena_hash",       nuevoHash);
                    row.setInt16("intentos_fallidos",    0);
                    row.setBool("bloqueado",             false);
                    row.setInt64("bloqueado_desde",      0);
                    row.setInt64("bloqueado_hasta",      0);
                    row.setInt64("fecha_cambio_password",ahora);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("IOError: no se pudo actualizar la contrasena");
            }
            return mapear(updated.view());
        }

        static UsuarioDTO desbloquearPorNombre(const char* nombreUsuario) {
            asegurarEsquemaTabla(TABLA);
            PgRowBuffer updated;
            const uint16_t n = PgBinaryStorage::getInstance().updateWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return nombreUsuario && strcmp(r.getCStr("usuario"), nombreUsuario) == 0; },
                [](PgRowBuffer& row) {
                    row.setInt16("intentos_fallidos", 0);
                    row.setBool("bloqueado",          false);
                    row.setInt64("bloqueado_desde",   0);
                    row.setInt64("bloqueado_hasta",   0);
                },
                &updated
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el usuario a desbloquear no existe");
            }
            return mapear(updated.view());
        }

        static void eliminarPorNombre(const char* nombreUsuario) {
            asegurarEsquemaTabla(TABLA);
            if (nombreUsuario && strcasecmp(nombreUsuario, "admin") == 0) {
                throw std::runtime_error("PermissionError: no se puede eliminar la cuenta principal admin");
            }
            const uint16_t n = PgBinaryStorage::getInstance().deleteWhere(
                TABLA,
                [&](const PgRowView& r) -> bool { return nombreUsuario && strcmp(r.getCStr("usuario"), nombreUsuario) == 0; },
                true
            );
            if (n == 0) {
                throw std::runtime_error("LookupError: el usuario a eliminar no existe");
            }
        }

        static EstadisticasUsuariosDTO obtenerEstadisticasGlobales() {
            asegurarSemillaPorDefecto();
            EstadisticasUsuariosDTO out{};
            PgBinaryStorage::getInstance().scanForward(TABLA, [&](const PgRowView& r, uint32_t) -> bool {
                out.totalUsuarios++;
                if (r.getBool("activa", true)) out.usuariosActivos++;
                if (r.getBool("bloqueado", false)) out.usuariosBloqueados++;
                const int32_t intentos = r.getInt16("intentos_fallidos", 0);
                if (intentos > out.maxIntentosFallidos) out.maxIntentosFallidos = intentos;
                return true;
            });
            return out;
        }
    };

} // namespace DbRuntime::Repositories