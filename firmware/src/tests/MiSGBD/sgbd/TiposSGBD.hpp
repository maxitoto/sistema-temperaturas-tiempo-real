// Archivo: src/tests/SQLite/sgbd/TiposSGBD.hpp
#pragma once
#include <Arduino.h>
#include <functional>

namespace SGBD {

    // Dirección de ordenamiento para ORDER BY e Índices
    enum Orden : uint8_t { ASC = 0, DESC = 1 };

    // Comportamiento ante borrado de padres en claves foráneas (FK)
    enum AccionFK : uint8_t {
        RESTRICT = 0, // Bloquea el borrado si existen registros hijos vinculados
        CASCADE  = 1, // Borra automáticamente los registros hijos vinculados
        SET_NULL = 2  // Deja en NULL la clave foránea de los registros hijos
    };

    // Tipos de unión relacional entre tablas
    enum TipoJoin : uint8_t {
        INNER_JOIN = 0, // Devuelve solo filas que coinciden en ambas tablas
        LEFT_JOIN  = 1  // Devuelve todas las filas de la izquierda aunque no tengan par a la derecha
    };

    // Operadores de comparación para filtros WHERE
    enum Operador : uint8_t {
        EQ = 0, NEQ, GT, GTE, LT, LTE, LIKE
    };

    // Tipos de columna soportados por el diccionario de datos
    enum TipoDato : uint8_t {
        TYPE_INT = 0, TYPE_FLOAT, TYPE_STR, TYPE_BOOL
    };

    // Niveles de rol del sistema (DCL)
    enum RolSGBD : uint8_t {
        ROL_ANONIMO  = 0,
        ROL_LECTOR   = 1,
        ROL_OPERADOR = 2,
        ROL_ADMIN    = 3
    };

    // Máscara de bits para permisos DCL (GRANT / REVOKE)
    enum PermisoDCL : uint8_t {
        PERM_NONE   = 0,
        PERM_SELECT = 1 << 0,
        PERM_INSERT = 1 << 1,
        PERM_UPDATE = 1 << 2,
        PERM_DELETE = 1 << 3,
        PERM_DDL    = 1 << 4,
        PERM_ALL    = 0x1F
    };

    using CheckPredicado = std::function<bool(const String&)>;

}

// Exposición global de palabras clave SQL
using SGBD::ASC;
using SGBD::DESC;
using SGBD::RESTRICT;
using SGBD::CASCADE;
using SGBD::SET_NULL;
using SGBD::INNER_JOIN;
using SGBD::LEFT_JOIN;
using SGBD::EQ;
using SGBD::NEQ;
using SGBD::GT;
using SGBD::GTE;
using SGBD::LT;
using SGBD::LTE;
using SGBD::LIKE;
using SGBD::ROL_ANONIMO;
using SGBD::ROL_LECTOR;
using SGBD::ROL_OPERADOR;
using SGBD::ROL_ADMIN;
using SGBD::PERM_SELECT;
using SGBD::PERM_INSERT;
using SGBD::PERM_UPDATE;
using SGBD::PERM_DELETE;
using SGBD::PERM_DDL;
using SGBD::PERM_ALL;