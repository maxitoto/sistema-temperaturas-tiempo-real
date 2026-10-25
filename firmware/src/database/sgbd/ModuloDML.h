// Archivo: ./src/database/sgbd/ModuloDML.h
#pragma once
#include "plugins/microsd/MicroSD.h"
#include "database/sgbd/CatalogoSistema.h"

namespace SGBD {

    class ModuloDML {
      private:
        MicroSD* sd;
        const CatalogoSistema* catalogo;

      public:
        ModuloDML(MicroSD* driverSd, const CatalogoSistema* cat)
            : sd(driverSd), catalogo(cat) {}

        // INSERT INTO tabla VALUES (...) -> Añade al final del archivo en O(1)
        template <typename TFila>
        bool insertar(IdTabla tabla, const TFila& nuevaFila) {
            if (!sd || !catalogo) return false;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta || meta->esSingleton || sizeof(TFila) != meta->tamanoFila) return false;

            HandleSD h = sd->abrirEscritura(meta->rutaArchivo);
            if (h == MicroSD::HANDLE_INVALIDO) return false;

            size_t escritos = sd->escribir(h, reinterpret_cast<const uint8_t*>(&nuevaFila), sizeof(TFila));
            bool okCierre = sd->cerrarSeguro(h);
            return (escritos == sizeof(TFila)) && okCierre;
        }

        // UPDATE tabla SET ... WHERE row_index = indiceFila -> Sobrescribe in-place en O(1)
        template <typename TFila>
        bool actualizarPorIndice(IdTabla tabla, size_t indiceFila, const TFila& filaModificada) {
            if (!sd || !catalogo) return false;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta || sizeof(TFila) != meta->tamanoFila) return false;

            HandleSD h = sd->abrirLecturaYEscritura(meta->rutaArchivo);
            if (h == MicroSD::HANDLE_INVALIDO) return false;

            uint64_t offset = (uint64_t)indiceFila * sizeof(TFila);
            if (offset + sizeof(TFila) > sd->tamanoArchivo(h) || !sd->buscar(h, offset)) {
                sd->cerrar(h);
                return false;
            }

            size_t escritos = sd->escribir(h, reinterpret_cast<const uint8_t*>(&filaModificada), sizeof(TFila));
            bool okCierre = sd->cerrarSeguro(h);
            return (escritos == sizeof(TFila)) && okCierre;
        }

        // INSERT / UPDATE en Ring-Buffer Circular (ej. senso, evento_sensor, auditoria)
        template <typename TFila>
        bool escribirCircular(IdTabla tabla, uint32_t idAutoincremental, size_t maxFilasAnillo, const TFila& fila) {
            if (idAutoincremental == 0 || maxFilasAnillo == 0) return false;
            size_t slot = (size_t)((idAutoincremental - 1) % maxFilasAnillo);

            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta || sizeof(TFila) != meta->tamanoFila) return false;

            size_t filasActuales = (size_t)(sd->tamanoArchivo(meta->rutaArchivo) / sizeof(TFila));
            if (slot >= filasActuales) {
                return insertar(tabla, fila);
            }
            return actualizarPorIndice(tabla, slot, fila);
        }

        // REPLACE INTO tabla_singleton (PK = 1) -> Conserva fecha_creacion original y escribe vía .tmp
        template <typename TFila>
        bool guardarSingleton(IdTabla tabla, TFila filaNueva, uint32_t timestampActual) {
            if (!sd || !catalogo) return false;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta || !meta->esSingleton || sizeof(TFila) != meta->tamanoFila) return false;

            // Si ya existía el registro 1, conservamos su fecha_creacion original
            TFila existente;
            HandleSD hLect = sd->abrirLectura(meta->rutaArchivo);
            if (hLect != MicroSD::HANDLE_INVALIDO) {
                if (sd->leer(hLect, reinterpret_cast<uint8_t*>(&existente), sizeof(TFila)) == sizeof(TFila) &&
                    existente.fecha_creacion != 0) {
                    filaNueva.fecha_creacion = existente.fecha_creacion;
                }
                sd->cerrar(hLect);
            }
            if (filaNueva.fecha_creacion == 0) {
                filaNueva.fecha_creacion = timestampActual;
            }
            filaNueva.fecha_modificacion = timestampActual;

            // Escritura segura Write-Ahead en archivo .tmp + rename
            if (sd->existe(meta->rutaTmp)) sd->eliminarArchivo(meta->rutaTmp);
            HandleSD hTmp = sd->crearArchivo(meta->rutaTmp);
            if (hTmp == MicroSD::HANDLE_INVALIDO) return false;

            size_t escritos = sd->escribir(hTmp, reinterpret_cast<const uint8_t*>(&filaNueva), sizeof(TFila));
            if (!sd->cerrarSeguro(hTmp) || escritos != sizeof(TFila)) {
                sd->eliminarArchivo(meta->rutaTmp);
                return false;
            }
            return sd->renombrar(meta->rutaTmp, meta->rutaArchivo);
        }
    };

}