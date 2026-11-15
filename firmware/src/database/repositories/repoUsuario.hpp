#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <stdexcept>

#include "RepositorioBase.hpp"
#include "../../connections/remote/Controller/abstract/RPC_base.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::ModelManager;
    using DbRuntime::Orm::QuerySet;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::AggregateFunc;
    using DbRuntime::Standard::WindowFunc;
    using DbRuntime::Standard::ConflictAction;
    using DbRuntime::Engine::OnConflictClause;
    using DbRuntime::Engine::PredicateCondition;
    using DbRuntime::Engine::DmlExecutionResult;
    using DbCommon::Standard::PgDataType;
    using DbCommon::Standard::SortDirection;
    using DbCommon::Engine::DdlDiagnosticConsole;

    inline const ModelManager Usuario("Usuario");

    struct UsuarioDTO {
        int32_t    idUsuario;
        char       usuario[20];
        char       salt[9];
        char       contrasenaHash[65];
        RolUsuario rol;
        int16_t    intentosFallidos;
        bool       bloqueado;
        bool       activa;
        char       creadoEl[20];
        char       actualizadoEl[20];
        uint32_t   actualizadoEpoch;
    };

    struct EstadisticasUsuariosDTO {
        int32_t totalUsuarios;
        int32_t usuariosActivos;
        int32_t usuariosBloqueados;
        int32_t maxIntentosFallidos;
    };

    class RepoUsuario {
    public:
        static constexpr int16_t MAX_INTENTOS_LOGIN = 3;

    private:
        static RolUsuario textoARol(const String& txt) {
            if (txt.equalsIgnoreCase("ADMIN"))    return RolUsuario::ADMIN;
            if (txt.equalsIgnoreCase("NO_ADMIN")) return RolUsuario::NO_ADMIN;
            return RolUsuario::NINGUNO;
        }

        static const char* rolATexto(RolUsuario rol) {
            switch (rol) {
                case RolUsuario::ADMIN:    return "ADMIN";
                case RolUsuario::NO_ADMIN: return "NO_ADMIN";
                default:                   return "NINGUNO";
            }
        }

        static void exigirExitoDml(const DmlExecutionResult& res) {
            if (!res.success) {
                throw std::runtime_error(DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str());
            }
        }

        static String calcularHashConSalt(const char* salt, const char* clavePlana) {
            uint64_t h = 14695981039346656037ULL;
            const char* s = salt ? salt : "SALT";
            while (*s) {
                h ^= static_cast<uint8_t>(*s++);
                h *= 1099511628211ULL;
            }
            const char* p = clavePlana ? clavePlana : "";
            while (*p) {
                h ^= static_cast<uint8_t>(*p++);
                h *= 1099511628211ULL;
            }
            char buf[17];
            snprintf(buf, sizeof(buf), "%08lX%08lX",
                     static_cast<unsigned long>((h >> 32) & 0xFFFFFFFFUL),
                     static_cast<unsigned long>(h & 0xFFFFFFFFUL));
            return String(buf);
        }

        static bool verificarContrasena(const UsuarioDTO& u, const char* clavePlana) {
            if (!clavePlana || clavePlana[0] == '\0') return false;
            String hashCalculado = calcularHashConSalt(u.salt, clavePlana);
            if (hashCalculado.equalsIgnoreCase(u.contrasenaHash)) {
                return true;
            }
            return strcmp(u.contrasenaHash, clavePlana) == 0;
        }

        static UsuarioDTO mapear(const PgTuple& fila) {
            UsuarioDTO dto{};
            dto.idUsuario        = fila.getInt("id_usuario", -1);
            dto.rol              = textoARol(fila.getText("rol", "NINGUNO"));
            dto.intentosFallidos = fila.getSmallInt("intentos_fallidos", 0);
            dto.bloqueado        = fila.getBool("bloqueado", false);
            dto.activa           = fila.getBool("activa", true);
            dto.actualizadoEpoch = static_cast<uint32_t>(fila.getEpochSeconds("actualizado_el", 0));

            strncpy(dto.usuario, fila.getText("usuario", "").c_str(), sizeof(dto.usuario) - 1);
            strncpy(dto.salt, fila.getText("salt", "SALT").c_str(), sizeof(dto.salt) - 1);
            strncpy(dto.contrasenaHash, fila.getText("contrasena_hash", "").c_str(), sizeof(dto.contrasenaHash) - 1);
            strncpy(dto.creadoEl, fila.getText("creado_el", "1970-01-01 00:00:00").c_str(), sizeof(dto.creadoEl) - 1);
            strncpy(dto.actualizadoEl, fila.getText("actualizado_el", "1970-01-01 00:00:00").c_str(), sizeof(dto.actualizadoEl) - 1);
            return dto;
        }

    public:
        static bool sembrarUsuariosPorDefecto() {
            TransaccionBatch tx;

            String hashAdmin = calcularHashConSalt("SALT_ADM", "admin");
            String hashOper  = calcularHashConSalt("SALT_OPE", "operador");

            auto r1 = Usuario.upsert({
                {"usuario",           "admin",    PgDataType::VARCHAR},
                {"salt",              "SALT_ADM", PgDataType::CHAR},
                {"contrasena_hash",   hashAdmin,  PgDataType::TEXT},
                {"rol",               "ADMIN",    PgDataType::ENUM},
                {"intentos_fallidos", "0",        PgDataType::SMALLINT},
                {"bloqueado",         "FALSE",    PgDataType::BOOLEAN},
                {"activa",            "TRUE",     PgDataType::BOOLEAN}
            }, ConflictAction::DO_NOTHING);

            auto r2 = Usuario.upsert({
                {"usuario",           "operador", PgDataType::VARCHAR},
                {"salt",              "SALT_OPE", PgDataType::CHAR},
                {"contrasena_hash",   hashOper,   PgDataType::TEXT},
                {"rol",               "NO_ADMIN", PgDataType::ENUM},
                {"intentos_fallidos", "0",        PgDataType::SMALLINT},
                {"bloqueado",         "FALSE",    PgDataType::BOOLEAN},
                {"activa",            "TRUE",     PgDataType::BOOLEAN}
            }, ConflictAction::DO_NOTHING);

            return tx.confirmar() && r1.success && r2.success;
        }

        static bool obtenerPorNombreUsuario(const char* nombreUsuario, UsuarioDTO& outDto) {
            if (!nombreUsuario || nombreUsuario[0] == '\0') return false;
            PgTuple fila;
            if (!Usuario.filter_eq("usuario", String(nombreUsuario)).first(fila)) {
                return false;
            }
            outDto = mapear(fila);
            return true;
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
            if (!obtenerPorNombreUsuario(nombreUsuario, u) || !u.activa) {
                throw std::runtime_error("AuthenticationError: credenciales invalidas");
            }

            if (u.bloqueado) {
                throw std::runtime_error("AuthenticationError: cuenta bloqueada en base de datos por intentos fallidos");
            }

            if (!verificarContrasena(u, clavePlana)) {
                int16_t nuevosIntentos = static_cast<int16_t>(u.intentosFallidos + 1);
                if (nuevosIntentos > MAX_INTENTOS_LOGIN) {
                    nuevosIntentos = MAX_INTENTOS_LOGIN;
                }
                const bool bloquearAhora = (nuevosIntentos >= MAX_INTENTOS_LOGIN);

                auto resUpd = Usuario
                    .filter_eq("id_usuario", u.idUsuario)
                    .update({
                        {"intentos_fallidos", String(nuevosIntentos),          PgDataType::SMALLINT},
                        {"bloqueado",         bloquearAhora ? "TRUE" : "FALSE", PgDataType::BOOLEAN}
                    });
                exigirExitoDml(resUpd);

                if (bloquearAhora) {
                    throw std::runtime_error("AuthenticationError: usuario bloqueado tras alcanzar 3 intentos fallidos");
                }
                throw std::runtime_error("AuthenticationError: credenciales invalidas");
            }

            if (u.intentosFallidos > 0) {
                auto resReset = Usuario
                    .filter_eq("id_usuario", u.idUsuario)
                    .update({
                        {"intentos_fallidos", "0",     PgDataType::SMALLINT},
                        {"bloqueado",         "FALSE", PgDataType::BOOLEAN}
                    });
                exigirExitoDml(resReset);
                u.intentosFallidos = 0;
            }

            return u;
        }

        // Lista usuarios delegando el control del límite de 32 registros directamente al SGBD (ModuloDQL)
        static std::vector<UsuarioDTO> listarUsuariosPaginados(int32_t limite, int32_t offset = 0, bool soloActivos = false) {
            QuerySet qs = Usuario.objects();
            if (soloActivos) {
                qs.filter_eq("activa", "TRUE");
            }
            return qs
                .order_by("id_usuario")
                .offset(offset)
                .limit(limite)
                .all_as<UsuarioDTO>(&RepoUsuario::mapear);
        }

        static UsuarioDTO crearUsuario(const char* nombreUsuario,
                                       const char* clavePlana,
                                       RolUsuario rol = RolUsuario::NO_ADMIN) {
            if (!nombreUsuario || nombreUsuario[0] == '\0' || !clavePlana || clavePlana[0] == '\0') {
                throw std::invalid_argument("ValueError: usuario y contrasena no pueden estar vacios");
            }

            const char* salt = "SALT_USR";
            String hash = calcularHashConSalt(salt, clavePlana);

            auto res = Usuario.create({
                {"usuario",           String(nombreUsuario), PgDataType::VARCHAR},
                {"salt",              String(salt),          PgDataType::CHAR},
                {"contrasena_hash",   hash,                  PgDataType::TEXT},
                {"rol",               rolATexto(rol),        PgDataType::ENUM},
                {"intentos_fallidos", "0",                   PgDataType::SMALLINT},
                {"bloqueado",         "FALSE",               PgDataType::BOOLEAN},
                {"activa",            "TRUE",                PgDataType::BOOLEAN}
            }, {"*"});

            exigirExitoDml(res);
            if (res.returningRows.empty()) {
                throw std::runtime_error("RuntimeError: no se pudo recuperar el usuario creado");
            }
            return mapear(res.returningRows[0]);
        }

        static UsuarioDTO actualizarRolYEstado(const char* nombreUsuario, RolUsuario nuevoRol, bool activa) {
            auto res = Usuario
                .filter_eq("usuario", String(nombreUsuario))
                .returning({"*"})
                .update({
                    {"rol",    rolATexto(nuevoRol),      PgDataType::ENUM},
                    {"activa", activa ? "TRUE" : "FALSE", PgDataType::BOOLEAN}
                });

            exigirExitoDml(res);
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: usuario no encontrado para actualizar");
            }
            return mapear(res.returningRows[0]);
        }

        static UsuarioDTO cambiarContrasena(const char* nombreUsuario,
                                            const char* claveActual,
                                            const char* claveNueva,
                                            bool exigirClaveActual) {
            if (!claveNueva || claveNueva[0] == '\0') {
                throw std::invalid_argument("ValueError: la nueva contrasena no puede estar vacia");
            }

            UsuarioDTO u = exigirPorNombreUsuario(nombreUsuario);
            if (exigirClaveActual && !verificarContrasena(u, claveActual)) {
                throw std::runtime_error("AuthenticationError: la contrasena actual no coincide");
            }

            String nuevoHash = calcularHashConSalt(u.salt, claveNueva);
            auto res = Usuario
                .filter_eq("id_usuario", u.idUsuario)
                .returning({"*"})
                .update({
                    {"contrasena_hash",   nuevoHash, PgDataType::TEXT},
                    {"intentos_fallidos", "0",       PgDataType::SMALLINT},
                    {"bloqueado",         "FALSE",   PgDataType::BOOLEAN}
                });

            exigirExitoDml(res);
            return mapear(res.returningRows[0]);
        }

        static UsuarioDTO desbloquearPorNombre(const char* nombreUsuario) {
            auto res = Usuario
                .filter_eq("usuario", String(nombreUsuario))
                .returning({"*"})
                .update({
                    {"intentos_fallidos", "0",     PgDataType::SMALLINT},
                    {"bloqueado",         "FALSE", PgDataType::BOOLEAN}
                });
            exigirExitoDml(res);
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: usuario no encontrado para desbloquear");
            }
            return mapear(res.returningRows[0]);
        }

        static void eliminarPorNombre(const char* nombreUsuario) {
            if (!nombreUsuario || nombreUsuario[0] == '\0') {
                throw std::invalid_argument("ValueError: nombre de usuario vacio");
            }
            if (strcmp(nombreUsuario, "admin") == 0) {
                throw std::runtime_error("PermissionError: no se permite eliminar la cuenta principal 'admin'");
            }
            auto res = Usuario.filter_eq("usuario", String(nombreUsuario)).remove();
            exigirExitoDml(res);
            if (res.affectedRows == 0) {
                throw std::runtime_error("LookupError: el usuario a eliminar no existe");
            }
        }

        static EstadisticasUsuariosDTO obtenerEstadisticasGlobales() {
            PredicateCondition filtroActivos;
            filtroActivos.columnOrExpr = "activa";
            filtroActivos.op           = FilterOp::EQ;
            filtroActivos.valueA       = "TRUE";

            PredicateCondition filtroBloqueados;
            filtroBloqueados.columnOrExpr = "bloqueado";
            filtroBloqueados.op           = FilterOp::EQ;
            filtroBloqueados.valueA       = "TRUE";

            std::vector<PgTuple> res = Usuario.objects()
                .annotate("total_usr", AggregateFunc::COUNT_ALL)
                .annotate_filtered("total_activos", AggregateFunc::COUNT_COL, "id_usuario", filtroActivos)
                .annotate_filtered("total_bloqueados", AggregateFunc::COUNT_COL, "id_usuario", filtroBloqueados)
                .annotate("max_fallos", AggregateFunc::MAX, "intentos_fallidos")
                .all();

            EstadisticasUsuariosDTO stats{0, 0, 0, 0};
            if (!res.empty()) {
                stats.totalUsuarios       = res[0].getInt("total_usr", 0);
                stats.usuariosActivos     = res[0].getInt("total_activos", 0);
                stats.usuariosBloqueados  = res[0].getInt("total_bloqueados", 0);
                stats.maxIntentosFallidos = res[0].getInt("max_fallos", 0);
            }
            return stats;
        }
    };

} // namespace DbRuntime::Repositories