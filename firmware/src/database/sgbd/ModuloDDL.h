// Archivo: ./src/database/sgbd/ModuloDDL.h
#pragma once
#include "plugins/microsd/MicroSD.h"
#include "database/sgbd/CatalogoSistema.h"

namespace SGBD {

    class ModuloDDL {
      private:
        MicroSD* sd;
        CatalogoSistema* catalogo;

        // Si un reemplazo de archivo previo se cortó por energía, restaura el .tmp
        void recuperarArchivoSiIncompleto(const MetadatoTabla& meta) {
            if (!sd->existe(meta.rutaArchivo) && sd->existe(meta.rutaTmp)) {
                sd->renombrar(meta.rutaTmp, meta.rutaArchivo);
            }
        }

      public:
        ModuloDDL(MicroSD* driverSd, CatalogoSistema* cat)
            : sd(driverSd), catalogo(cat) {}

        // CREATE DATABASE + CREATE TABLE IF NOT EXISTS para todo el esquema
        bool inicializarEsquemaFisico() {
            if (!sd || !catalogo || !sd->estaMontada()) return false;
            if (!sd->crearDirectorio(CatalogoSistema::directorioBase())) return false;

            for (uint8_t i = 0; i < TOTAL_TABLAS_SISTEMA; i++) {
                if (!crearTabla((IdTabla)i)) return false;
            }
            return true;
        }

        // CREATE TABLE IF NOT EXISTS (crea la cáscara física en la SD)
        bool crearTabla(IdTabla tabla) {
            if (!sd || !catalogo) return false;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta) return false;

            recuperarArchivoSiIncompleto(*meta);
            if (sd->existe(meta->rutaArchivo)) return true;

            HandleSD h = sd->crearArchivo(meta->rutaArchivo);
            if (h == MicroSD::HANDLE_INVALIDO) return false;
            return sd->cerrarSeguro(h);
        }

        // DROP TABLE
        bool eliminarTabla(IdTabla tabla) {
            if (!sd || !catalogo) return false;
            const MetadatoTabla* meta = catalogo->obtener(tabla);
            if (!meta) return false;
            if (sd->existe(meta->rutaTmp)) sd->eliminarArchivo(meta->rutaTmp);
            if (!sd->existe(meta->rutaArchivo)) return true;
            return sd->eliminarArchivo(meta->rutaArchivo);
        }

        // TRUNCATE TABLE (deja la cáscara física en 0 bytes)
        bool truncarTabla(IdTabla tabla) {
            if (!eliminarTabla(tabla)) return false;
            return crearTabla(tabla);
        }
    };

}