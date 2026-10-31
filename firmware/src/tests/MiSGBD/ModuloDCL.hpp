#pragma once
#include <Arduino.h>
#include <map>
#include "TiposSGBD.hpp"

namespace SGBD {

    struct ContextoSesion {
        String usuario = "system_admin";
        RolSGBD rol = ROL_ADMIN;
        bool autenticado = true;
    };

    class ModuloDCL {
    private:
        ContextoSesion sesionActual;
        // Mapa: tabla -> (rol -> mascara de permisos)
        std::map<String, std::map<RolSGBD, uint8_t>> aclTablas;

        ModuloDCL() = default;

        uint8_t permisosPorDefecto(RolSGBD rol) const {
            switch (rol) {
                case ROL_ADMIN:    return PERM_ALL;
                case ROL_OPERADOR: return PERM_SELECT | PERM_INSERT | PERM_UPDATE;
                case ROL_LECTOR:   return PERM_SELECT;
                default:           return PERM_NONE;
            }
        }

    public:
        static ModuloDCL& getInstance() {
            static ModuloDCL instance;
            return instance;
        }
        ModuloDCL(const ModuloDCL&) = delete;
        ModuloDCL& operator=(const ModuloDCL&) = delete;

        void establecerSesion(const String& usuario, RolSGBD rol, bool autenticado = true) {
            sesionActual = { usuario, rol, autenticado };
            ConsolaSQL::getInstance().reportarInfo(
                "DCL SESSION",
                "Usuario activo: '" + usuario + "' (Rol=" + String((int)rol) + ")"
            );
        }

        void cerrarSesion() {
            sesionActual = { "anonimo", ROL_ANONIMO, false };
        }

        const ContextoSesion& obtenerSesion() const { return sesionActual; }

        // GRANT explícito sobre una tabla para un rol
        bool grant(uint8_t permisosMask, const String& tabla, RolSGBD rolObjetivo) {
            if (!autorizar(tabla, PERM_DDL, "GRANT")) return false;
            uint8_t actuales = obtenerPermisos(tabla, rolObjetivo);
            aclTablas[tabla][rolObjetivo] = actuales | permisosMask;
            ConsolaSQL::getInstance().reportarInfo(
                "GRANT",
                "Permisos (0x" + String(permisosMask, HEX) + ") otorgados en '" + tabla + "' al Rol " + String((int)rolObjetivo)
            );
            return true;
        }

        // REVOKE explícito sobre una tabla para un rol
        bool revoke(uint8_t permisosMask, const String& tabla, RolSGBD rolObjetivo) {
            if (!autorizar(tabla, PERM_DDL, "REVOKE")) return false;
            uint8_t actuales = obtenerPermisos(tabla, rolObjetivo);
            aclTablas[tabla][rolObjetivo] = actuales & (~permisosMask);
            ConsolaSQL::getInstance().reportarInfo(
                "REVOKE",
                "Permisos (0x" + String(permisosMask, HEX) + ") revocados en '" + tabla + "' al Rol " + String((int)rolObjetivo)
            );
            return true;
        }

        uint8_t obtenerPermisos(const String& tabla, RolSGBD rol) const {
            auto itTabla = aclTablas.find(tabla);
            if (itTabla != aclTablas.end()) {
                auto itRol = itTabla->second.find(rol);
                if (itRol != itTabla->second.end()) {
                    return itRol->second;
                }
            }
            return permisosPorDefecto(rol);
        }

        bool autorizar(const String& tabla, PermisoDCL permisoRequerido, const String& operacionNombre) const {
            if (!sesionActual.autenticado || sesionActual.rol == ROL_ANONIMO) {
                ConsolaSQL::getInstance().reportarError(
                    "28000", "INVALID_AUTHORIZATION_SPECIFICATION",
                    operacionNombre + " ON '" + tabla + "'",
                    "No hay sesion autenticada activa."
                );
                return false;
            }

            if (sesionActual.rol == ROL_ADMIN) return true;

            uint8_t mascara = obtenerPermisos(tabla, sesionActual.rol);
            if ((mascara & permisoRequerido) == 0) {
                ConsolaSQL::getInstance().reportarError(
                    "42501", "INSUFFICIENT_PRIVILEGE",
                    operacionNombre + " ON '" + tabla + "'",
                    "El usuario '" + sesionActual.usuario + "' no tiene privilegios para ejecutar " + operacionNombre + " en '" + tabla + "'."
                );
                return false;
            }
            return true;
        }
    };

}