// Archivo: ./src/database/orm/RepositorioTabla.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "database/sgbd/MotorSGBD.h"

namespace ORM {

    using namespace SGBD;

    // =========================================================================
    // 1. MODELO CATÁLOGO (Para tablas autoincrementales: sensor, correos, usuarios, ntp)
    // =========================================================================
    template <typename TFila, typename TId, TId TFila::*CampoId>
    class ModeloCatalogo {
      private:
        MotorSGBD* motor;
        IdTabla    tabla;

      public:
        ModeloCatalogo(MotorSGBD* m, IdTabla t) : motor(m), tabla(t) {}

        // Equivalente a prisma.modelo.count()
        size_t contarTotal(const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            return motor ? motor->contarFilas(sesion, tabla) : 0;
        }

        // Equivalente a prisma.modelo.create({ data })
        // Asigna automáticamente el ID autoincremental, activa = 1 y los timestamps
        bool crear(TFila& nuevaFila, uint32_t timestampActual,
                   const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) {
            if (!motor) return false;
            size_t totalActual = motor->contarFilas(sesion, tabla);
            nuevaFila.*CampoId          = (TId)(totalActual + 1);
            nuevaFila.activa            = 1;
            nuevaFila.fecha_creacion     = timestampActual;
            nuevaFila.fecha_modificacion = timestampActual;
            return motor->insertarFila(sesion, tabla, nuevaFila);
        }

        // Equivalente a prisma.modelo.findUnique({ where: { id } }) en O(1)
        bool buscarPorId(TId id, TFila& salida, bool soloActivos = true,
                         const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!motor || id == 0) return false;
            if (!motor->seleccionarPorIndice(sesion, tabla, (size_t)(id - 1), salida)) {
                return false;
            }
            if (soloActivos && salida.activa == 0) return false;
            return (salida.*CampoId == id);
        }

        // Equivalente a prisma.modelo.findFirst({ where: predicado })
        template <typename FnPredicado>
        bool buscarPrimero(TFila& salida, FnPredicado&& predicado, bool soloActivos = true,
                           const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!motor) return false;
            bool encontrado = false;
            motor->escanear<TFila>(sesion, tabla, [&](size_t, const TFila& fila) -> bool {
                if (soloActivos && fila.activa == 0) return true; // Seguir buscando
                if (predicado(fila)) {
                    salida = fila;
                    encontrado = true;
                    return false; // Cortar escaneo (encontrado)
                }
                return true;
            });
            return encontrado;
        }

        // Equivalente a prisma.modelo.findMany({ where: { activa: 1 } })
        template <typename FnVisitante>
        size_t recorrerActivos(FnVisitante&& visitante,
                               const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!motor) return 0;
            size_t cont = 0;
            motor->escanear<TFila>(sesion, tabla, [&](size_t, const TFila& fila) -> bool {
                if (fila.activa == 0) return true;
                cont++;
                return visitante(fila);
            });
            return cont;
        }

        // Equivalente a prisma.modelo.update({ where: { id }, data }) en O(1)
        // Conserva automáticamente id y fecha_creacion originales
        bool actualizarPorId(TId id, TFila& filaModificada, uint32_t timestampActual,
                             const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) {
            if (!motor || id == 0) return false;
            TFila original;
            if (!motor->seleccionarPorIndice(sesion, tabla, (size_t)(id - 1), original)) {
                return false;
            }
            filaModificada.*CampoId          = id;
            filaModificada.fecha_creacion     = original.fecha_creacion;
            filaModificada.fecha_modificacion = timestampActual;
            return motor->actualizarFila(sesion, tabla, (size_t)(id - 1), filaModificada);
        }

        // Baja lógica: UPDATE tabla SET activa = 0, fecha_modificacion = ahora WHERE id = X
        bool desactivarPorId(TId id, uint32_t timestampActual,
                             const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) {
            if (!motor || id == 0) return false;
            TFila fila;
            if (!motor->seleccionarPorIndice(sesion, tabla, (size_t)(id - 1), fila)) {
                return false;
            }
            fila.activa             = 0;
            fila.fecha_modificacion = timestampActual;
            return motor->actualizarFila(sesion, tabla, (size_t)(id - 1), fila);
        }
    };

    // =========================================================================
    // 2. MODELO CIRCULAR (Para series de tiempo e historial: senso, evento_sensor, auditoria)
    // =========================================================================
    template <typename TFila, uint32_t TFila::*CampoId, size_t MaxFilasAnillo>
    class ModeloCircular {
      private:
        MotorSGBD* motor;
        IdTabla    tabla;
        uint32_t   ultimoIdGenerado = 0;

      public:
        ModeloCircular(MotorSGBD* m, IdTabla t) : motor(m), tabla(t) {}

        // Al arrancar el ESP, recupera cuál fue el mayor ID escrito antes del último apagado
        void sincronizarSecuenciaInicial() {
            if (!motor) return;
            uint32_t maxId = 0;
            ContextoSesionSGBD sys = ContextoSesionSGBD::sistemaInterno();
            motor->escanear<TFila>(sys, tabla, [&](size_t, const TFila& fila) -> bool {
                if (fila.*CampoId > maxId) maxId = fila.*CampoId;
                return true;
            });
            ultimoIdGenerado = maxId;
        }

        uint32_t obtenerUltimoId() const { return ultimoIdGenerado; }

        // Inserta en el anillo circular (sobrescribe el slot más viejo al superar MaxFilasAnillo)
        bool registrar(TFila& nuevaFila, uint32_t timestampActual,
                       const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) {
            if (!motor) return false;
            uint32_t nuevoId = ++ultimoIdGenerado;
            nuevaFila.*CampoId          = nuevoId;
            nuevaFila.activa            = 1;
            nuevaFila.fecha_creacion     = timestampActual;
            nuevaFila.fecha_modificacion = timestampActual;
            return motor->escribirCircular(sesion, tabla, nuevoId, MaxFilasAnillo, nuevaFila);
        }

        // Obtiene un registro puntual por su ID histórico (si aún está dentro de la ventana circular)
        bool buscarPorId(uint32_t id, TFila& salida,
                         const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!motor || id == 0) return false;
            size_t slot = (size_t)((id - 1) % MaxFilasAnillo);
            if (!motor->seleccionarPorIndice(sesion, tabla, slot, salida)) return false;
            return (salida.*CampoId == id && salida.activa != 0);
        }

        template <typename FnVisitante>
        size_t recorrerActivos(FnVisitante&& visitante,
                               const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!motor) return 0;
            size_t cont = 0;
            motor->escanear<TFila>(sesion, tabla, [&](size_t, const TFila& fila) -> bool {
                if (fila.activa == 0) return true;
                cont++;
                return visitante(fila);
            });
            return cont;
        }
    };

    // =========================================================================
    // 3. MODELO SINGLETON (Para configuraciones de fila única PK = 1: smtp, lts, conectividad)
    // =========================================================================
    template <typename TFila, uint8_t TFila::*CampoId>
    class ModeloSingleton {
      private:
        MotorSGBD* motor;
        IdTabla    tabla;

      public:
        ModeloSingleton(MotorSGBD* m, IdTabla t) : motor(m), tabla(t) {}

        bool obtener(TFila& salida,
                     const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) const {
            if (!motor) return false;
            if (!motor->leerSingleton(sesion, tabla, salida)) return false;
            return salida.activa != 0;
        }

        // Reemplaza la fila 1 de forma atómica conservando fecha_creacion original
        bool guardar(TFila& datosNuevos, uint32_t timestampActual,
                     const ContextoSesionSGBD& sesion = ContextoSesionSGBD::sistemaInterno()) {
            if (!motor) return false;
            datosNuevos.*CampoId = 1;
            datosNuevos.activa   = 1;
            return motor->guardarSingleton(sesion, tabla, datosNuevos, timestampActual);
        }
    };

}