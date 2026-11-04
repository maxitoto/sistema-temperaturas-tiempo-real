// Archivo: src/tests/SQLite/sgbd/ModuloDCL.hpp
#pragma once
#include <Arduino.h>
#include <map>
#include "TiposSGBD.hpp"
#include "ConsolaSQL.hpp"

namespace SGBD {

    struct ContextoSesion {
        String usuario = "system_admin";
        RolSGBD rol = ROL_ADMIN;
        bool autenticado = true;
    };

    class ModuloDCL {
    private:
        ContextoSesion sesionActual;
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
            ConsolaSQL::getInstance().reportarInfo("DCL SESSION", "Usuario: '" + usuario + "' (Rol=" + String((int)rol) + ")");
        }

        uint8_t obtenerPermisos(const String& tabla, RolSGBD rol) const {
            auto itT = aclTablas.find(tabla);
            if (itT != aclTablas.end()) {
                auto itR = itT->second.find(rol);
                if (itR != itT->second.end()) return itR->second;
            }
            return permisosPorDefecto(rol);
        }

        bool grant(uint8_t mask, const String& tabla, RolSGBD rol) {
            if (!autorizar(tabla, PERM_DDL, "GRANT")) return false;
            aclTablas[tabla][rol] = obtenerPermisos(tabla, rol) | mask;
            ConsolaSQL::getInstance().reportarInfo("GRANT", "Permisos otorgados en '" + tabla + "' al Rol " + String((int)rol));
            return true;
        }

        bool revoke(uint8_t mask, const String& tabla, RolSGBD rol) {
            if (!autorizar(tabla, PERM_DDL, "REVOKE")) return false;
            aclTablas[tabla][rol] = obtenerPermisos(tabla, rol) & (~mask);
            ConsolaSQL::getInstance().reportarInfo("REVOKE", "Permisos revocados en '" + tabla + "' al Rol " + String((int)rol));
            return true;
        }

        bool autorizar(const String& tabla, PermisoDCL perm, const String& op) const {
            if (!sesionActual.autenticado || sesionActual.rol == ROL_ANONIMO) {
                ConsolaSQL::getInstance().reportarError(
                    "28000", "NOT_AUTHENTICATED", op + " ON '" + tabla + "'",
                    "No hay sesion activa.",
                    "Ejecute db.setSession(\"usuario\", ROL_ADMIN) antes de operar."
                );
                return false;
            }
            if (sesionActual.rol == ROL_ADMIN) return true;
            if ((obtenerPermisos(tabla, sesionActual.rol) & perm) == 0) {
                ConsolaSQL::getInstance().reportarError(
                    "42501", "INSUFFICIENT_PRIVILEGE", op + " ON '" + tabla + "'",
                    "El usuario '" + sesionActual.usuario + "' no tiene permiso para " + op + ".",
                    "Use db.grant(PERM_...).onTable(\"" + tabla + "\").toRole(...) como administrador."
                );
                return false;
            }
            return true;
        }
    };

}