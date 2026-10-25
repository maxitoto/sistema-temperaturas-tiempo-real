// Archivo: ./src/database/sgbd/ModuloDCL.h
#pragma once
#include <stdint.h>
#include <string.h>
#include "database/EsquemaFisico.h"
#include "database/sgbd/CatalogoSistema.h"

namespace SGBD {

    struct ContextoSesionSGBD {
        uint16_t idUsuario   = 0;
        uint8_t  rol         = EsquemaFisicoDB::ROL_REGULAR;
        bool     autenticado = false;

        ContextoSesionSGBD() = default;
        ContextoSesionSGBD(uint16_t id, uint8_t r, bool auth)
            : idUsuario(id), rol(r), autenticado(auth) {}

        static ContextoSesionSGBD sistemaInterno() {
            return ContextoSesionSGBD(0, EsquemaFisicoDB::ROL_ADMIN, true);
        }
    };

    class ModuloDCL {
      private:
        CatalogoSistema* catalogo;

      public:
        explicit ModuloDCL(CatalogoSistema* cat) : catalogo(cat) {}

        // GRANT / REVOKE: Define qué rol mínimo se exige para escribir en una tabla
        bool configurarPrivilegioEscritura(IdTabla tabla, uint8_t rolMinimo) {
            return catalogo ? catalogo->cambiarRolMinimoEscritura(tabla, rolMinimo) : false;
        }

        // Security Manager ("Portero" invocado por el Orquestador antes de DML / DQL)
        bool autorizarOperacion(const ContextoSesionSGBD& sesion, IdTabla tabla, OperacionSGBD op) const {
            if (!catalogo || !sesion.autenticado) return false;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta) return false;

            if (op == OP_DQL_LECTURA) {
                return true; // Usuarios autenticados (REGULAR o ADMIN) pueden consultar
            }
            if (op == OP_DML_ESCRITURA) {
                return sesion.rol >= meta->rolMinimoEscritura;
            }
            // DDL y DCL requieren siempre rol ADMIN
            return sesion.rol >= EsquemaFisicoDB::ROL_ADMIN;
        }
    };

}