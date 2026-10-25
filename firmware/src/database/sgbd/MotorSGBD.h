// Archivo: ./src/database/sgbd/MotorSGBD.h
#pragma once
#include "plugins/microsd/MicroSD.h"
#include "database/sgbd/CatalogoSistema.h"
#include "database/sgbd/ModuloDDL.h"
#include "database/sgbd/ModuloDQL.h"
#include "database/sgbd/ModuloDML.h"
#include "database/sgbd/ModuloDCL.h"

namespace SGBD {

    class MotorSGBD {
      public:
        CatalogoSistema catalogo;
        ModuloDDL       ddl;
        ModuloDQL       dql;
        ModuloDML       dml;
        ModuloDCL       dcl;

      private:
        MicroSD* almacenamiento;

      public:
        explicit MotorSGBD(MicroSD* driverSd)
            : catalogo(),
              ddl(driverSd, &catalogo),
              dql(driverSd, &catalogo),
              dml(driverSd, &catalogo),
              dcl(&catalogo),
              almacenamiento(driverSd) {}

        bool iniciar() {
            if (!almacenamiento) return false;
            if (!almacenamiento->estaMontada() && !almacenamiento->inicializar()) {
                return false;
            }
            return ddl.inicializarEsquemaFisico();
        }

        // --- VÍA ORQUESTADA DQL ---
        size_t contarFilas(const ContextoSesionSGBD& sesion, IdTabla tabla) const {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DQL_LECTURA)) return 0;
            return dql.contarFilas(tabla);
        }

        template <typename TFila>
        bool seleccionarPorIndice(const ContextoSesionSGBD& sesion, IdTabla tabla, size_t idx, TFila& salida) const {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DQL_LECTURA)) return false;
            return dql.seleccionarPorIndice(tabla, idx, salida);
        }

        template <typename TFila>
        bool leerSingleton(const ContextoSesionSGBD& sesion, IdTabla tabla, TFila& salida) const {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DQL_LECTURA)) return false;
            return dql.seleccionarSingleton(tabla, salida);
        }

        template <typename TFila, typename FnVisitante>
        size_t escanear(const ContextoSesionSGBD& sesion, IdTabla tabla, FnVisitante&& visitante) const {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DQL_LECTURA)) return 0;
            return dql.escanear<TFila>(tabla, visitante);
        }

        // --- VÍA ORQUESTADA DML ---
        template <typename TFila>
        bool insertarFila(const ContextoSesionSGBD& sesion, IdTabla tabla, const TFila& fila) {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DML_ESCRITURA)) return false;
            return dml.insertar(tabla, fila);
        }

        template <typename TFila>
        bool actualizarFila(const ContextoSesionSGBD& sesion, IdTabla tabla, size_t idx, const TFila& fila) {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DML_ESCRITURA)) return false;
            return dml.actualizarPorIndice(tabla, idx, fila);
        }

        template <typename TFila>
        bool escribirCircular(const ContextoSesionSGBD& sesion, IdTabla tabla, uint32_t idAuto, size_t maxFilas, const TFila& fila) {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DML_ESCRITURA)) return false;
            return dml.escribirCircular(tabla, idAuto, maxFilas, fila);
        }

        template <typename TFila>
        bool guardarSingleton(const ContextoSesionSGBD& sesion, IdTabla tabla, const TFila& fila, uint32_t ahoraSeg) {
            if (!dcl.autorizarOperacion(sesion, tabla, OP_DML_ESCRITURA)) return false;
            return dml.guardarSingleton(tabla, fila, ahoraSeg);
        }

        bool apagarSeguro() {
            return almacenamiento ? almacenamiento->cerrarSeguro() : false;
        }
    };

}