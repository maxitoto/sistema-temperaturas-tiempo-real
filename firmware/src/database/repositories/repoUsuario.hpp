#pragma once
#include <Arduino.h>
#include <time.h>
#include <stdexcept>
#include <vector>
#include "../common/PgDtoTypes.hpp"
#include "../common/DiagnosticConsole.hpp"
#include "../DB_DQL_DML/orm/DjangoManager.hpp"

namespace DbRuntime::Repositories {

    using DbRuntime::Orm::Usuario;
    using DbRuntime::Orm::runtimeDb;
    using DbRuntime::Storage::PgCell;
    using DbRuntime::Storage::PgTuple;
    using DbRuntime::Standard::FilterOp;
    using DbRuntime::Standard::AggregateFunc;
    using DbNotRuntime::Engine::DdlDiagnosticConsole;
    using DbNotRuntime::Engine::PhysicalSchemaCatalog;

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
        static constexpr int64_t DURACION_BLOQUEO_SEGUNDOS = 600;

        static int64_t epochActual() {
            const time_t t = time(nullptr);
            return (t > 100000) ? static_cast<int64_t>(t) : static_cast<int64_t>(millis() / 1000UL);
        }

        static void asegurarCatalogo() {
            if (PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
                runtimeDb.bindToPhysicalCatalog();
            }
        }

        static void lanzarUltimoErrorDB() {
            throw std::runtime_error(DdlDiagnosticConsole::getInstance().last().toPsqlFormat().c_str());
        }

        static String calcularHashConSalt(const char* clavePlana, const char* salt) {
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

            char hex[17]{};
            snprintf(hex, sizeof(hex), "%08lX%08lX", static_cast<unsigned long>(h1), static_cast<unsigned long>(h2));
            return String(hex);
        }

        static String generarSaltDeterminista(const char* usuario) {
            uint32_t acc = 0xA5A5F00Du;
            if (usuario != nullptr) {
                for (size_t i = 0; usuario[i] != '\0'; ++i) {
                    acc = (acc * 33u) ^ static_cast<uint8_t>(usuario[i]);
                }
            }
            acc ^= static_cast<uint32_t>(millis());
            char s[9]{};
            snprintf(s, sizeof(s), "S%07lX", static_cast<unsigned long>(acc & 0x0FFFFFFFu));
            return String(s);
        }

        static bool coincideClave(const UsuarioDTO& u, const char* clavePlana) {
            if (clavePlana == nullptr) return false;
            const String hashCalculado = calcularHashConSalt(clavePlana, u.salt.c_str());
            if (hashCalculado.equalsIgnoreCase(u.contrasenaHash.c_str())) {
                return true;
            }
            return strcmp(u.contrasenaHash.c_str(), clavePlana) == 0;
        }

        static void asegurarSemillaPorDefecto() {
            if (Usuario.objects().exists()) {
                return;
            }
            const int64_t ahora = epochActual();
            const String saltAdmin = "SALTADM1";
            const String hashAdmin = calcularHashConSalt("admin1234", saltAdmin.c_str());
            Usuario.create({
                PgCell("usuario",               "admin"),
                PgCell("salt",                  saltAdmin),
                PgCell("contrasena_hash",       hashAdmin),
                PgCell("rol",                   RolUsuario::ADMIN),
                PgCell("intentos_fallidos",     static_cast<int16_t>(0)),
                PgCell("bloqueado",             false),
                PgCell("bloqueado_desde",       static_cast<int64_t>(0)),
                PgCell("bloqueado_hasta",       static_cast<int64_t>(0)),
                PgCell("fecha_ultimo_login",    static_cast<int64_t>(0)),
                PgCell("fecha_cambio_password", ahora),
                PgCell("activa",                true)
            });

            const String saltOp = "SALTOPE1";
            const String hashOp = calcularHashConSalt("operador1234", saltOp.c_str());
            Usuario.create({
                PgCell("usuario",               "operador"),
                PgCell("salt",                  saltOp),
                PgCell("contrasena_hash",       hashOp),
                PgCell("rol",                   RolUsuario::NO_ADMIN),
                PgCell("intentos_fallidos",     static_cast<int16_t>(0)),
                PgCell("bloqueado",             false),
                PgCell("bloqueado_desde",       static_cast<int64_t>(0)),
                PgCell("bloqueado_hasta",       static_cast<int64_t>(0)),
                PgCell("fecha_ultimo_login",    static_cast<int64_t>(0)),
                PgCell("fecha_cambio_password", ahora),
                PgCell("activa",                true)
            });
        }

    public:
        static UsuarioDTO mapear(const PgTuple& r) {
            UsuarioDTO u{};
            r["id_usuario"]            >> u.idUsuario;
            r["usuario"]               >> u.usuario;
            r["salt"]                  >> u.salt;
            r["contrasena_hash"]       >> u.contrasenaHash;
            r["rol"]                   >> u.rol;
            r["intentos_fallidos"]     >> u.intentosFallidos;
            r["bloqueado"]             >> u.bloqueado;
            r["bloqueado_desde"]       >> u.bloqueadoDesde;
            r["bloqueado_hasta"]       >> u.bloqueadoHasta;
            r["fecha_ultimo_login"]    >> u.fechaUltimoLogin;
            r["fecha_cambio_password"] >> u.fechaCambioPassword;
            r["activa"]                >> u.activa;
            r["creado_el"]             >> u.creadoEl;
            r["actualizado_el"]        >> u.actualizadoEl;
            u.actualizadoEpoch = static_cast<PgEpoch>(r.getEpochSeconds("actualizado_el", 0));
            return u;
        }

        static bool obtenerPorNombreUsuario(const char* nombreUsuario, UsuarioDTO& outDto) {
            asegurarCatalogo();
            asegurarSemillaPorDefecto();

            PgTuple fila;
            if (!Usuario.filter_eq("usuario", String(nombreUsuario ? nombreUsuario : "")).first(fila)) {
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
            if (!obtenerPorNombreUsuario(nombreUsuario, u)) {
                throw std::runtime_error("AuthenticationError: credenciales invalidas");
            }

            if (!u.activa) {
                throw std::runtime_error("PermissionError: la cuenta de usuario esta desactivada");
            }

            const int64_t ahora = epochActual();
            if (u.bloqueado) {
                if (u.bloqueadoHasta == 0 || ahora < u.bloqueadoHasta) {
                    throw std::runtime_error("AuthenticationError: usuario bloqueado por maximo de intentos fallidos");
                }
            }

            if (!coincideClave(u, clavePlana)) {
                const int16_t nuevosIntentos = static_cast<int16_t>((u.intentosFallidos < 3) ? (u.intentosFallidos + 1) : 3);
                const bool bloquearAhora     = (nuevosIntentos >= 3);
                const int64_t desde          = bloquearAhora ? ahora : u.bloqueadoDesde;
                const int64_t hasta          = bloquearAhora ? (ahora + DURACION_BLOQUEO_SEGUNDOS) : u.bloqueadoHasta;

                Usuario.filter_eq("id_usuario", u.idUsuario).update({
                    PgCell("intentos_fallidos", nuevosIntentos),
                    PgCell("bloqueado",         bloquearAhora),
                    PgCell("bloqueado_desde",   desde),
                    PgCell("bloqueado_hasta",   hasta)
                });

                if (bloquearAhora) {
                    throw std::runtime_error("AuthenticationError: usuario bloqueado tras 3 intentos fallidos");
                }
                throw std::runtime_error("AuthenticationError: credenciales invalidas");
            }

            auto res = Usuario.filter_eq("id_usuario", u.idUsuario)
                .returning({"*"})
                .update({
                    PgCell("intentos_fallidos",  static_cast<int16_t>(0)),
                    PgCell("bloqueado",          false),
                    PgCell("bloqueado_desde",    static_cast<int64_t>(0)),
                    PgCell("bloqueado_hasta",    static_cast<int64_t>(0)),
                    PgCell("fecha_ultimo_login", ahora)
                });

            if (res.success && !res.returningRows.empty()) {
                return mapear(res.returningRows[0]);
            }
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
            asegurarCatalogo();
            asegurarSemillaPorDefecto();

            auto q = Usuario.objects();
            if (soloActivos) {
                q.filter_eq("activa", "TRUE");
            }
            return q.order_by("id_usuario")
                    .limit(static_cast<int32_t>(limite))
                    .offset(static_cast<int32_t>(offset))
                    .all_as<UsuarioDTO>(&RepoUsuario::mapear);
        }

        static UsuarioDTO crearUsuario(const char* nombreUsuario, const char* clavePlana, RolUsuario rol) {
            asegurarCatalogo();
            asegurarSemillaPorDefecto();

            if (nombreUsuario == nullptr || nombreUsuario[0] == '\0') {
                throw std::invalid_argument("ValueError: el nombre de usuario no puede estar vacio");
            }
            if (clavePlana == nullptr || strlen(clavePlana) < 4) {
                throw std::invalid_argument("ValueError: la contrasena debe tener al menos 4 caracteres");
            }

            const int64_t ahora    = epochActual();
            const String nuevoSalt = generarSaltDeterminista(nombreUsuario);
            const String nuevoHash = calcularHashConSalt(clavePlana, nuevoSalt.c_str());

            auto res = Usuario.create(
                {
                    PgCell("usuario",               nombreUsuario),
                    PgCell("salt",                  nuevoSalt),
                    PgCell("contrasena_hash",       nuevoHash),
                    PgCell("rol",                   rol),
                    PgCell("intentos_fallidos",     static_cast<int16_t>(0)),
                    PgCell("bloqueado",             false),
                    PgCell("bloqueado_desde",       static_cast<int64_t>(0)),
                    PgCell("bloqueado_hasta",       static_cast<int64_t>(0)),
                    PgCell("fecha_ultimo_login",    static_cast<int64_t>(0)),
                    PgCell("fecha_cambio_password", ahora),
                    PgCell("activa",                true)
                },
                {"*"}
            );

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static UsuarioDTO actualizarRolYEstado(const char* nombreUsuario, RolUsuario nuevoRol, bool activa) {
            asegurarCatalogo();

            auto res = Usuario.filter_eq("usuario", String(nombreUsuario ? nombreUsuario : ""))
                .returning({"*"})
                .update({
                    PgCell("rol",    nuevoRol),
                    PgCell("activa", activa)
                });

            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: el usuario a actualizar no existe");
            }
            return mapear(res.returningRows[0]);
        }

        static UsuarioDTO cambiarContrasena(
            const char* nombreUsuario,
            const char* claveActual,
            const char* claveNueva,
            bool verificarClaveActual = true
        ) {
            if (claveNueva == nullptr || strlen(claveNueva) < 4) {
                throw std::invalid_argument("ValueError: la nueva contrasena debe tener al menos 4 caracteres");
            }

            UsuarioDTO actual = exigirPorNombreUsuario(nombreUsuario);
            if (verificarClaveActual && !coincideClave(actual, claveActual)) {
                throw std::runtime_error("AuthenticationError: la contrasena actual es incorrecta");
            }

            const int64_t ahora    = epochActual();
            const String nuevoSalt = generarSaltDeterminista(nombreUsuario);
            const String nuevoHash = calcularHashConSalt(claveNueva, nuevoSalt.c_str());

            auto res = Usuario.filter_eq("id_usuario", actual.idUsuario)
                .returning({"*"})
                .update({
                    PgCell("salt",                  nuevoSalt),
                    PgCell("contrasena_hash",       nuevoHash),
                    PgCell("intentos_fallidos",     static_cast<int16_t>(0)),
                    PgCell("bloqueado",             false),
                    PgCell("bloqueado_desde",       static_cast<int64_t>(0)),
                    PgCell("bloqueado_hasta",       static_cast<int64_t>(0)),
                    PgCell("fecha_cambio_password", ahora)
                });

            if (!res.success || res.returningRows.empty()) {
                lanzarUltimoErrorDB();
            }
            return mapear(res.returningRows[0]);
        }

        static UsuarioDTO desbloquearPorNombre(const char* nombreUsuario) {
            asegurarCatalogo();

            auto res = Usuario.filter_eq("usuario", String(nombreUsuario ? nombreUsuario : ""))
                .returning({"*"})
                .update({
                    PgCell("intentos_fallidos", static_cast<int16_t>(0)),
                    PgCell("bloqueado",         false),
                    PgCell("bloqueado_desde",   static_cast<int64_t>(0)),
                    PgCell("bloqueado_hasta",   static_cast<int64_t>(0))
                });

            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.returningRows.empty()) {
                throw std::runtime_error("LookupError: el usuario a desbloquear no existe");
            }
            return mapear(res.returningRows[0]);
        }

        static void eliminarPorNombre(const char* nombreUsuario) {
            asegurarCatalogo();

            if (nombreUsuario != nullptr && strcasecmp(nombreUsuario, "admin") == 0) {
                throw std::runtime_error("PermissionError: no se puede eliminar la cuenta principal admin");
            }

            auto res = Usuario.filter_eq("usuario", String(nombreUsuario ? nombreUsuario : "")).remove();
            if (!res.success) {
                lanzarUltimoErrorDB();
            }
            if (res.affectedRows == 0) {
                throw std::runtime_error("LookupError: el usuario a eliminar no existe");
            }
        }

        static EstadisticasUsuariosDTO obtenerEstadisticasGlobales() {
            asegurarCatalogo();
            asegurarSemillaPorDefecto();

            DbRuntime::Engine::PredicateCondition filtroActivos;
            filtroActivos.columnOrExpr = "activa";
            filtroActivos.op           = FilterOp::EQ;
            filtroActivos.valueA       = "TRUE";

            DbRuntime::Engine::PredicateCondition filtroBloqueados;
            filtroBloqueados.columnOrExpr = "bloqueado";
            filtroBloqueados.op           = FilterOp::EQ;
            filtroBloqueados.valueA       = "TRUE";

            PgTuple r;
            EstadisticasUsuariosDTO out{};
            if (Usuario.objects()
                    .annotate("total",        AggregateFunc::COUNT_ALL)
                    .annotate_filtered("activos",    AggregateFunc::COUNT_COL, "id_usuario", filtroActivos)
                    .annotate_filtered("bloqueados", AggregateFunc::COUNT_COL, "id_usuario", filtroBloqueados)
                    .annotate("max_intentos", AggregateFunc::MAX, "intentos_fallidos")
                    .first(r)) {
                r["total"]        >> out.totalUsuarios;
                r["activos"]      >> out.usuariosActivos;
                r["bloqueados"]   >> out.usuariosBloqueados;
                r["max_intentos"] >> out.maxIntentosFallidos;
            }
            return out;
        }
    };

}