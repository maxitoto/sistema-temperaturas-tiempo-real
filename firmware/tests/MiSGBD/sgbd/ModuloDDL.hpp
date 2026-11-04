// Archivo: src/tests/SQLite/sgbd/ModuloDDL.hpp
#pragma once
#include "CatalogoSistema.hpp"
#include "ModuloDCL.hpp"

namespace SGBD {

    class ModuloDDL {
    private:
        ModuloDDL() = default;
    public:
        static ModuloDDL& getInstance() {
            static ModuloDDL instance;
            return instance;
        }
        ModuloDDL(const ModuloDDL&) = delete;
        ModuloDDL& operator=(const ModuloDDL&) = delete;

        bool inicializarAlmacenamiento() {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) {
                ConsolaSQL::getInstance().reportarError(
                    "58030", "IO_SD_ERROR", "INIT_STORAGE",
                    "La tarjeta MicroSD no esta inicializada.",
                    "Llame a iniciarSGBDyORM() al comienzo de setup()."
                );
                return false;
            }
            String base = CatalogoSistema::getInstance().rutaBase();
            return sd.existeArchivo(base.c_str()) || sd.crearDirectorio(base.c_str());
        }

        bool crearTabla(const EsquemaTabla& esquema) {
            if (!ModuloDCL::getInstance().autorizar(esquema.nombreTabla, PERM_DDL, "CREATE TABLE")) return false;
            if (!inicializarAlmacenamiento()) return false;

            CatalogoSistema& cat = CatalogoSistema::getInstance();
            for (const auto& attr : esquema.atributos) {
                if (attr.esFk) {
                    const EsquemaTabla* padre = cat.obtenerEsquema(attr.fkTablaDestino);
                    if (!padre || !padre->existeAtributo(attr.fkColumnaDestino)) {
                        ConsolaSQL::getInstance().reportarError(
                            "42830", "INVALID_FOREIGN_KEY",
                            "CREATE TABLE '" + esquema.nombreTabla + "'",
                            "La tabla destino '" + attr.fkTablaDestino + "' o su atributo '" + attr.fkColumnaDestino + "' no existen.",
                            "Cree primero la tabla padre '" + attr.fkTablaDestino + "' antes de crear '" + esquema.nombreTabla + "'."
                        );
                        return false;
                    }
                }
            }

            cat.registrarEsquema(esquema);
            String ruta = cat.rutaTabla(esquema.nombreTabla);
            if (!TarjetaSD::getInstance().existeArchivo(ruta.c_str())) {
                TarjetaSD::getInstance().escribirArchivo(ruta.c_str(), "");
            }
            ConsolaSQL::getInstance().reportarInfo("CREATE TABLE", "'" + esquema.nombreTabla + "' lista.");
            return true;
        }

        bool eliminarTabla(const String& tabla) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_DDL, "DROP TABLE")) return false;
            CatalogoSistema& cat = CatalogoSistema::getInstance();
            cat.eliminarEsquema(tabla);
            TarjetaSD::getInstance().eliminarArchivo(cat.rutaSeq(tabla).c_str());
            return TarjetaSD::getInstance().eliminarArchivo(cat.rutaTabla(tabla).c_str());
        }

        bool truncarTabla(const String& tabla) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_DDL, "TRUNCATE TABLE")) return false;
            CatalogoSistema& cat = CatalogoSistema::getInstance();
            TarjetaSD::getInstance().eliminarArchivo(cat.rutaSeq(tabla).c_str());
            return TarjetaSD::getInstance().escribirArchivo(cat.rutaTabla(tabla).c_str(), "");
        }
    };

}