// Archivo: ./src/database/sgbd/ModuloDQL.h
#pragma once
#include "plugins/microsd/MicroSD.h"
#include "database/sgbd/CatalogoSistema.h"

namespace SGBD {

    class ModuloDQL {
      private:
        MicroSD* sd;
        const CatalogoSistema* catalogo;

      public:
        ModuloDQL(MicroSD* driverSd, const CatalogoSistema* cat)
            : sd(driverSd), catalogo(cat) {}

        // SELECT COUNT(*) FROM tabla (Complejidad O(1) usando tamaño del archivo)
        size_t contarFilas(IdTabla tabla) const {
            if (!sd || !catalogo) return 0;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta || meta->tamanoFila == 0) return 0;
            return (size_t)(sd->tamanoArchivo(meta->rutaArchivo) / meta->tamanoFila);
        }

        // SELECT * FROM tabla WHERE row_index = indiceFila (Complejidad O(1) con seek)
        template <typename TFila>
        bool seleccionarPorIndice(IdTabla tabla, size_t indiceFila, TFila& filaSalida) const {
            if (!sd || !catalogo) return false;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta || sizeof(TFila) != meta->tamanoFila) return false;

            HandleSD h = sd->abrirLectura(meta->rutaArchivo);
            if (h == MicroSD::HANDLE_INVALIDO) return false;

            uint64_t offset = (uint64_t)indiceFila * meta->tamanoFila;
            if (offset + meta->tamanoFila > sd->tamanoArchivo(h) || !sd->buscar(h, offset)) {
                sd->cerrar(h);
                return false;
            }

            size_t leidos = sd->leer(h, reinterpret_cast<uint8_t*>(&filaSalida), sizeof(TFila));
            sd->cerrar(h);
            return leidos == sizeof(TFila);
        }

        // SELECT * FROM tabla_singleton (PK = 1, lee siempre el índice 0)
        template <typename TFila>
        bool seleccionarSingleton(IdTabla tabla, TFila& filaSalida) const {
            return seleccionarPorIndice(tabla, 0, filaSalida);
        }

        // SELECT * FROM tabla WHERE predicado(fila) (Abre un único Handle y escanea secuencialmente)
        // Si el callback 'visitante(indice, fila)' retorna false, detiene el escaneo (LIMIT 1).
        template <typename TFila, typename FnVisitante>
        size_t escanear(IdTabla tabla, FnVisitante&& visitante) const {
            if (!sd || !catalogo) return 0;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta || sizeof(TFila) != meta->tamanoFila) return 0;

            HandleSD h = sd->abrirLectura(meta->rutaArchivo);
            if (h == MicroSD::HANDLE_INVALIDO) return 0;

            size_t totalFilas = (size_t)(sd->tamanoArchivo(h) / sizeof(TFila));
            size_t coincidencias = 0;
            TFila bufferFila;

            for (size_t i = 0; i < totalFilas; i++) {
                if (sd->leer(h, reinterpret_cast<uint8_t*>(&bufferFila), sizeof(TFila)) != sizeof(TFila)) {
                    break;
                }
                coincidencias++;
                if (!visitante(i, static_cast<const TFila&>(bufferFila))) {
                    break; // Early-exit
                }
            }

            sd->cerrar(h);
            return coincidencias;
        }
    };

}