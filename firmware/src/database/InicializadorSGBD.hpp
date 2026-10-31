// Archivo: src/tests/SQLite/InicializadorSGBD.hpp
#pragma once
#include "../hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "orm/ORM.hpp"

inline bool iniciarSGBDyORM() {
    if (!TarjetaSD::getInstance().estaInicializada()) {
        SGBD::ConsolaSQL::getInstance().reportarError(
            "58030", "HARDWARE_SD_FAIL", "iniciarSGBDyORM()",
            "No se pudo montar la tarjeta MicroSD por VSPI."
        );
        return false;
    }
    SGBD::MotorSGBD::getInstance().ddl().inicializarAlmacenamiento();
    db.setSession("system_admin", ROL_ADMIN);
    return true;
}