// Archivo: src/tests/SQLite/sgbd/ModuloDML.hpp
#pragma once
#include "ModuloDDL.hpp"
#include "ModuloDQL.hpp"

namespace SGBD {

    class ModuloDML {
    private:
        ModuloDML() = default;

        bool validarRestricciones(const EsquemaTabla& esq, Fila& fila, bool esUpdate = false) {
            CatalogoSistema& cat = CatalogoSistema::getInstance();

            for (const auto& par : fila) {
                if (!esq.existeAtributo(par.first)) {
                    ConsolaSQL::getInstance().reportarError(
                        "42703", "UNDEFINED_COLUMN", "DML ON '" + esq.nombreTabla + "'",
                        "El atributo '" + par.first + "' no pertenece a '" + esq.nombreTabla + "'.",
                        "Verifique el nombre del atributo en iniciarEsquema()."
                    );
                    return false;
                }
            }

            for (const auto& attr : esq.atributos) {
                bool presente = (fila.find(attr.nombre) != fila.end());
                if (!presente && !esUpdate) {
                    if (attr.esAutoIncrement || attr.esPk) {
                        fila[attr.nombre] = String(cat.siguienteId(esq.nombreTabla));
                        presente = true;
                    } else if (attr.tieneDefault) {
                        fila[attr.nombre] = attr.valorDefault;
                        presente = true;
                    } else if (attr.esNotNull) {
                        ConsolaSQL::getInstance().reportarError(
                            "23502", "NOT_NULL_VIOLATION", "INSERT INTO '" + esq.nombreTabla + "'",
                            "Falta el atributo obligatorio '" + attr.nombre + "'.",
                            "Incluya {\"" + attr.nombre + "\", \"valor\"} en su .insert()."
                        );
                        return false;
                    } else {
                        fila[attr.nombre] = "NULL";
                        presente = true;
                    }
                }
                if (!presente) continue;
                String val = fila[attr.nombre];

                if (attr.esNotNull && (val == "NULL" || (val.isEmpty() && !attr.tieneDefault))) {
                    ConsolaSQL::getInstance().reportarError(
                        "23502", "NOT_NULL_VIOLATION", esq.nombreTabla + "." + attr.nombre,
                        "El atributo '" + attr.nombre + "' no puede ser nulo ni vacio."
                    );
                    return false;
                }
                if (val == "NULL") continue;

                if (attr.maxVarchar > 0 && val.length() > attr.maxVarchar) {
                    ConsolaSQL::getInstance().reportarError(
                        "22001", "VARCHAR_OVERFLOW", esq.nombreTabla + "." + attr.nombre,
                        "El valor '" + val + "' supera el limite varChar(" + String(attr.maxVarchar) + ")."
                    );
                    return false;
                }

                if (attr.fnCheck && !attr.fnCheck(val)) {
                    ConsolaSQL::getInstance().reportarError(
                        "23514", "CHECK_VIOLATION", esq.nombreTabla + "." + attr.nombre,
                        "El valor '" + val + "' no cumple la condicion CHECK (" + attr.exprCheckTexto + ")."
                    );
                    return false;
                }

                if (!esUpdate && (attr.esPk || attr.esUnique)) {
                    ConsultaDQL qU;
                    qU.tabla = esq.nombreTabla;
                    qU.proyeccionesRaw = { attr.nombre };
                    qU.filtros.push_back({ attr.nombre, EQ, val });
                    if (!ModuloDQL::getInstance().seleccionar(qU, true).empty()) {
                        ConsolaSQL::getInstance().reportarError(
                            "23505", "UNIQUE_VIOLATION", esq.nombreTabla + "." + attr.nombre,
                            "Ya existe un registro con " + attr.nombre + " = '" + val + "'."
                        );
                        return false;
                    }
                }

                if (attr.esFk) {
                    ConsultaDQL qFk;
                    qFk.tabla = attr.fkTablaDestino;
                    qFk.proyeccionesRaw = { attr.fkColumnaDestino };
                    qFk.filtros.push_back({ attr.fkColumnaDestino, EQ, val });
                    if (ModuloDQL::getInstance().seleccionar(qFk, true).empty()) {
                        ConsolaSQL::getInstance().reportarError(
                            "23503", "FOREIGN_KEY_VIOLATION", esq.nombreTabla + "." + attr.nombre,
                            "No existe '" + attr.fkColumnaDestino + "=" + val + "' en la tabla padre '" + attr.fkTablaDestino + "'.",
                            "Inserte primero el registro padre en '" + attr.fkTablaDestino + "'."
                        );
                        return false;
                    }
                }
            }
            return true;
        }

    public:
        static ModuloDML& getInstance() {
            static ModuloDML instance;
            return instance;
        }
        ModuloDML(const ModuloDML&) = delete;
        ModuloDML& operator=(const ModuloDML&) = delete;

        bool insertar(const String& tabla, Fila fila) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_INSERT, "INSERT")) return false;
            CatalogoSistema& cat = CatalogoSistema::getInstance();
            const EsquemaTabla* esq = cat.obtenerEsquema(tabla);
            if (esq && !validarRestricciones(*esq, fila, false)) return false;

            String linea = CatalogoSistema::serializarFila(fila);
            bool ok = TarjetaSD::getInstance().anexarArchivo(cat.rutaTabla(tabla).c_str(), linea.c_str());
            if (ok) ConsolaSQL::getInstance().reportarInfo("INSERT", "Fila guardada en '" + tabla + "'.");
            return ok;
        }

        bool actualizar(const String& tabla, const std::vector<CondicionFiltro>& filtros, Fila cambios) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_UPDATE, "UPDATE")) return false;
            CatalogoSistema& cat = CatalogoSistema::getInstance();
            const EsquemaTabla* esq = cat.obtenerEsquema(tabla);
            if (esq && !validarRestricciones(*esq, cambios, true)) return false;

            File fIn = SD.open(cat.rutaTabla(tabla).c_str(), FILE_READ);
            if (!fIn) return false;
            File fOut = SD.open(cat.rutaTmp(tabla).c_str(), FILE_WRITE);
            if (!fOut) { fIn.close(); return false; }

            while (fIn.available()) {
                String l = fIn.readStringUntil('\n'); l.trim();
                if (l.isEmpty()) continue;
                Fila f = CatalogoSistema::deserializarFila(l);
                bool ok = true;
                for (const auto& c : filtros) if (!ModuloDQL::evaluarCondicion(f, c)) { ok = false; break; }
                if (ok) for (const auto& kv : cambios) f[kv.first] = kv.second;
                fOut.print(CatalogoSistema::serializarFila(f));
            }
            fIn.close(); fOut.close();
            SD.remove(cat.rutaTabla(tabla).c_str());
            return SD.rename(cat.rutaTmp(tabla).c_str(), cat.rutaTabla(tabla).c_str());
        }

        bool eliminar(const String& tabla, const std::vector<CondicionFiltro>& filtros) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_DELETE, "DELETE")) return false;
            CatalogoSistema& cat = CatalogoSistema::getInstance();

            ConsultaDQL qB; qB.tabla = tabla; qB.proyeccionesRaw = { "*" }; qB.filtros = filtros;
            auto aBorrar = ModuloDQL::getInstance().seleccionar(qB, true);
            if (aBorrar.empty()) return true;

            for (const auto& parEsq : cat.obtenerTodosLosEsquemas()) {
                const EsquemaTabla& esqHija = parEsq.second;
                for (const auto& attrH : esqHija.atributos) {
                    if (attrH.esFk && attrH.fkTablaDestino.equalsIgnoreCase(tabla)) {
                        for (const auto& fPadre : aBorrar) {
                            String valP = fPadre.at(attrH.fkColumnaDestino);
                            std::vector<CondicionFiltro> fHijo = { { attrH.nombre, EQ, valP } };
                            if (attrH.fkOnDelete == RESTRICT) {
                                ConsultaDQL qH; qH.tabla = esqHija.nombreTabla; qH.proyeccionesRaw = { "*" }; qH.filtros = fHijo;
                                if (!ModuloDQL::getInstance().seleccionar(qH, true).empty()) {
                                    ConsolaSQL::getInstance().reportarError(
                                        "23503", "FK_RESTRICT_VIOLATION", "DELETE FROM '" + tabla + "'",
                                        "Registro referenciado en '" + esqHija.nombreTabla + "' (" + attrH.nombre + "=" + valP + ").",
                                        "Configure la FK con CASCADE o elimine primero los hijos en '" + esqHija.nombreTabla + "'."
                                    );
                                    return false;
                                }
                            } else if (attrH.fkOnDelete == CASCADE) {
                                eliminar(esqHija.nombreTabla, fHijo);
                            } else if (attrH.fkOnDelete == SET_NULL) {
                                actualizar(esqHija.nombreTabla, fHijo, { { attrH.nombre, "NULL" } });
                            }
                        }
                    }
                }
            }

            File fIn = SD.open(cat.rutaTabla(tabla).c_str(), FILE_READ);
            if (!fIn) return false;
            File fOut = SD.open(cat.rutaTmp(tabla).c_str(), FILE_WRITE);
            if (!fOut) { fIn.close(); return false; }

            while (fIn.available()) {
                String l = fIn.readStringUntil('\n'); l.trim();
                if (l.isEmpty()) continue;
                Fila f = CatalogoSistema::deserializarFila(l);
                bool borrar = true;
                for (const auto& c : filtros) if (!ModuloDQL::evaluarCondicion(f, c)) { borrar = false; break; }
                if (!borrar) fOut.print(CatalogoSistema::serializarFila(f));
            }
            fIn.close(); fOut.close();
            SD.remove(cat.rutaTabla(tabla).c_str());
            return SD.rename(cat.rutaTmp(tabla).c_str(), cat.rutaTabla(tabla).c_str());
        }
    };

}