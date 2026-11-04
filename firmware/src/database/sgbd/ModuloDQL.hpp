#pragma once
#include <Arduino.h>
#include <SD.h>
#include <algorithm>
#include "CatalogoSistema.hpp"
#include "ModuloDCL.hpp"

namespace SGBD {

    struct CondicionFiltro {
        String columna;
        Operador op;
        String valor;
    };

    struct ClausulaJoin {
        String tablaDerecha;
        String colIzquierda; // Si está vacío, se autodetecta por FK
        String colDerecha;   // Si está vacío, se autodetecta por FK
        TipoJoin tipo = INNER_JOIN;
    };

    struct FuncionAgregada {
        String funcion;
        String columna;
        String alias;
    };

    struct ConsultaDQL {
        String tabla;
        std::vector<String> proyeccionesRaw;
        std::vector<ClausulaJoin> joins;
        std::vector<CondicionFiltro> filtros;
        std::vector<String> groupByCols;
        String columnaOrden = "";
        Orden orden = ASC;
        int limite = -1;
    };

    class ModuloDQL {
    private:
        ModuloDQL() = default;

        static bool esNumeroValido(const String& s) {
            if (s.isEmpty()) return false;
            int i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
            if (i >= static_cast<int>(s.length())) return false;
            bool pto = false;
            for (; i < static_cast<int>(s.length()); i++) {
                if (s[i] == '.') { if (pto) return false; pto = true; }
                else if (!isdigit(s[i])) return false;
            }
            return true;
        }

        // Optimización de RAM: Solo agrega "Tabla.col" si conPrefijo == true (cuando hay JOINs) y filtra al vuelo si es posible
        static std::vector<Fila> leerTabla(const String& nombreTabla, bool conPrefijo,
                                           const std::vector<CondicionFiltro>* filtrosDirectos = nullptr,
                                           int limiteDirecto = -1) {
            std::vector<Fila> lista;
            String ruta = CatalogoSistema::getInstance().rutaTabla(nombreTabla);
            File f = SD.open(ruta.c_str(), FILE_READ);
            if (!f) return lista;

            while (f.available()) {
                String linea = f.readStringUntil('\n');
                linea.trim();
                if (linea.isEmpty()) continue;

                Fila fila = CatalogoSistema::deserializarFila(linea);
                if (conPrefijo) {
                    Fila enriquecida;
                    for (const auto& par : fila) {
                        enriquecida[par.first] = par.second;
                        enriquecida[nombreTabla + "." + par.first] = par.second;
                    }
                    fila = std::move(enriquecida);
                }

                if (filtrosDirectos) {
                    bool pasa = true;
                    for (const auto& cond : *filtrosDirectos) {
                        if (!evaluarCondicion(fila, cond)) { pasa = false; break; }
                    }
                    if (!pasa) continue;
                }

                lista.push_back(std::move(fila));
                if (limiteDirecto > 0 && static_cast<int>(lista.size()) >= limiteDirecto) {
                    break;
                }
            }
            f.close();
            return lista;
        }

        static bool validarExistenciaColumna(const ConsultaDQL& q, const String& colRef) {
            if (colRef == "*") return true;
            CatalogoSistema& cat = CatalogoSistema::getInstance();

            int pto = colRef.indexOf('.');
            if (pto != -1) {
                String t = colRef.substring(0, pto);
                String c = colRef.substring(pto + 1);
                const EsquemaTabla* esq = cat.obtenerEsquema(t);
                return esq ? esq->existeAtributo(c) : true;
            }

            const EsquemaTabla* esqBase = cat.obtenerEsquema(q.tabla);
            if (!esqBase) return true;
            if (esqBase->existeAtributo(colRef)) return true;

            for (const auto& j : q.joins) {
                const EsquemaTabla* esqJ = cat.obtenerEsquema(j.tablaDerecha);
                if (esqJ && esqJ->existeAtributo(colRef)) return true;
            }
            return false;
        }

        static bool resolverColumnasJoin(const String& tablaIzq, ClausulaJoin& j) {
            if (!j.colIzquierda.isEmpty() && !j.colDerecha.isEmpty()) return true;

            CatalogoSistema& cat = CatalogoSistema::getInstance();
            const EsquemaTabla* esqIzq = cat.obtenerEsquema(tablaIzq);
            const EsquemaTabla* esqDer = cat.obtenerEsquema(j.tablaDerecha);

            if (!esqIzq || !esqDer) return false;

            for (const auto& attr : esqIzq->atributos) {
                if (attr.esFk && attr.fkTablaDestino.equalsIgnoreCase(j.tablaDerecha)) {
                    j.colIzquierda = attr.nombre;
                    j.colDerecha   = attr.fkColumnaDestino;
                    return true;
                }
            }
            for (const auto& attr : esqDer->atributos) {
                if (attr.esFk && attr.fkTablaDestino.equalsIgnoreCase(tablaIzq)) {
                    j.colIzquierda = attr.fkColumnaDestino;
                    j.colDerecha   = attr.nombre;
                    return true;
                }
            }
            return false;
        }

    public:
        static ModuloDQL& getInstance() {
            static ModuloDQL instance;
            return instance;
        }
        ModuloDQL(const ModuloDQL&) = delete;
        ModuloDQL& operator=(const ModuloDQL&) = delete;

        // Búsqueda O(1) en RAM (streaming línea a línea): ideal para validar UNIQUE, PK y FK sin cargar la tabla en memoria
        bool existeRegistro(const String& nombreTabla, const String& columna, const String& valor) const {
            String ruta = CatalogoSistema::getInstance().rutaTabla(nombreTabla);
            File f = SD.open(ruta.c_str(), FILE_READ);
            if (!f) return false;

            CondicionFiltro cond{columna, EQ, valor};
            bool encontrado = false;
            while (f.available()) {
                String linea = f.readStringUntil('\n');
                linea.trim();
                if (linea.isEmpty()) continue;
                Fila fila = CatalogoSistema::deserializarFila(linea);
                if (evaluarCondicion(fila, cond)) {
                    encontrado = true;
                    break;
                }
            }
            f.close();
            return encontrado;
        }

        static bool evaluarCondicion(const Fila& fila, const CondicionFiltro& cond) {
            auto it = fila.find(cond.columna);
            String valFila = (it != fila.end()) ? it->second : "NULL";

            if (cond.op == LIKE) {
                String p = cond.valor;
                p.replace("%", "");
                return valFila.indexOf(p) != -1;
            }
            if (esNumeroValido(valFila) && esNumeroValido(cond.valor)) {
                double nA = valFila.toDouble(), nB = cond.valor.toDouble();
                switch (cond.op) {
                    case EQ:  return nA == nB;
                    case NEQ: return nA != nB;
                    case GT:  return nA >  nB;
                    case GTE: return nA >= nB;
                    case LT:  return nA <  nB;
                    case LTE: return nA <= nB;
                    default:  return false;
                }
            }
            switch (cond.op) {
                case EQ:  return valFila == cond.valor;
                case NEQ: return valFila != cond.valor;
                case GT:  return valFila >  cond.valor;
                case GTE: return valFila >= cond.valor;
                case LT:  return valFila <  cond.valor;
                case LTE: return valFila <= cond.valor;
                default:  return false;
            }
        }

        static bool parsearAgregacion(const String& expr, FuncionAgregada& out) {
            String u = expr; u.trim();
            String up = u; up.toUpperCase();
            const char* funcs[] = { "COUNT(", "SUM(", "AVG(", "MIN(", "MAX(" };
            for (const char* f : funcs) {
                if (up.startsWith(f)) {
                    int cierre = u.indexOf(')');
                    if (cierre == -1) return false;
                    out.funcion = up.substring(0, strlen(f) - 1);
                    out.columna = u.substring(strlen(f), cierre);
                    out.columna.trim();
                    int posAs = up.indexOf(" AS ", cierre);
                    out.alias = (posAs != -1) ? u.substring(posAs + 4) : u.substring(0, cierre + 1);
                    out.alias.trim();
                    return true;
                }
            }
            return false;
        }

        std::vector<Fila> seleccionar(const ConsultaDQL& q, bool silencioso = false) const {
            std::vector<Fila> vacio;
            if (!silencioso && !ModuloDCL::getInstance().autorizar(q.tabla, PERM_SELECT, "SELECT")) {
                return vacio;
            }

            CatalogoSistema& cat = CatalogoSistema::getInstance();

            std::vector<ClausulaJoin> joinsResueltos = q.joins;
            for (auto& j : joinsResueltos) {
                if (!silencioso && !ModuloDCL::getInstance().autorizar(j.tablaDerecha, PERM_SELECT, "JOIN SELECT")) {
                    return vacio;
                }
                if (!cat.existeEsquema(j.tablaDerecha)) {
                    ConsolaSQL::getInstance().reportarError(
                        "42P01", "UNDEFINED_TABLE", "JOIN '" + j.tablaDerecha + "'",
                        "La tabla '" + j.tablaDerecha + "' no existe en el esquema.",
                        "Verifique el nombre de la tabla en iniciarEsquema()."
                    );
                    return vacio;
                }
                if (!resolverColumnasJoin(q.tabla, j)) {
                    ConsolaSQL::getInstance().reportarError(
                        "42703", "AMBIGUOUS_OR_MISSING_JOIN_KEY",
                        "JOIN ENTRE '" + q.tabla + "' Y '" + j.tablaDerecha + "'",
                        "No se encontro una clave foranea (FK) automatica entre ambas tablas.",
                        "Declare .FK(\"" + j.tablaDerecha + "\", \"id\") en el esquema o indique las columnas manualmente: .join(\"" + j.tablaDerecha + "\", \"col_fk\", \"id\")."
                    );
                    return vacio;
                }
            }

            std::vector<String> colsSimples;
            std::vector<FuncionAgregada> agregaciones;

            for (const auto& proy : q.proyeccionesRaw) {
                FuncionAgregada ag;
                if (parsearAgregacion(proy, ag)) {
                    if (!validarExistenciaColumna(q, ag.columna)) {
                        ConsolaSQL::getInstance().reportarError(
                            "42703", "UNDEFINED_COLUMN", "SELECT " + proy,
                            "El atributo '" + ag.columna + "' no existe.",
                            "Revise los atributos de '" + q.tabla + "'."
                        );
                        return vacio;
                    }
                    agregaciones.push_back(ag);
                } else {
                    if (!validarExistenciaColumna(q, proy)) {
                        ConsolaSQL::getInstance().reportarError(
                            "42703", "UNDEFINED_COLUMN", "SELECT FROM '" + q.tabla + "'",
                            "El atributo '" + proy + "' no existe en '" + q.tabla + "' ni en las tablas unidas por JOIN.",
                            "Si pertenece a otra tabla, encadene primero .join(\"OtraTabla\")."
                        );
                        return vacio;
                    }
                    colsSimples.push_back(proy);
                }
            }

            if (!agregaciones.empty() && !colsSimples.empty()) {
                for (const auto& c : colsSimples) {
                    bool enGroup = false;
                    for (const auto& g : q.groupByCols) {
                        if (g.equalsIgnoreCase(c)) { enGroup = true; break; }
                    }
                    if (!enGroup) {
                        ConsolaSQL::getInstance().reportarError(
                            "42803", "GROUPING_ERROR", "SELECT FROM '" + q.tabla + "'",
                            "El atributo '" + c + "' se selecciono junto con " + agregaciones[0].funcion + "() pero no esta en GROUP BY.",
                            "Agregue .groupBy(\"" + c + "\") a su consulta."
                        );
                        return vacio;
                    }
                }
            }

            const bool tieneJoins = !joinsResueltos.empty();
            const bool puedeLimitarEnLectura = !tieneJoins && agregaciones.empty() && q.groupByCols.empty() && q.columnaOrden.isEmpty();

            // Si no hay JOINs, filtramos con WHERE directamente al leer del archivo y sin duplicar claves "Tabla.col"
            std::vector<Fila> filtradas = leerTabla(
                q.tabla,
                tieneJoins,
                tieneJoins ? nullptr : &q.filtros,
                puedeLimitarEnLectura ? q.limite : -1
            );

            if (tieneJoins) {
                std::vector<Fila> trabajo = std::move(filtradas);
                for (const auto& j : joinsResueltos) {
                    std::vector<Fila> filasDerecha = leerTabla(j.tablaDerecha, true);
                    std::vector<Fila> nuevoTrabajo;
                    const EsquemaTabla* esqDer = cat.obtenerEsquema(j.tablaDerecha);

                    String claveIzq = (j.colIzquierda.indexOf('.') != -1) ? j.colIzquierda : (q.tabla + "." + j.colIzquierda);
                    String claveDer = (j.colDerecha.indexOf('.') != -1)   ? j.colDerecha   : (j.tablaDerecha + "." + j.colDerecha);

                    for (const auto& rIzq : trabajo) {
                        String valIzq = "";
                        if (rIzq.count(claveIzq)) valIzq = rIzq.at(claveIzq);
                        else if (rIzq.count(j.colIzquierda)) valIzq = rIzq.at(j.colIzquierda);

                        bool huboMatch = false;
                        for (const auto& rDer : filasDerecha) {
                            String valDer = "";
                            if (rDer.count(claveDer)) valDer = rDer.at(claveDer);
                            else if (rDer.count(j.colDerecha)) valDer = rDer.at(j.colDerecha);

                            if (!valIzq.isEmpty() && valIzq != "NULL" && valIzq == valDer) {
                                huboMatch = true;
                                Fila combinada = rIzq;
                                for (const auto& parDer : rDer) {
                                    if (parDer.first.indexOf('.') != -1 || combinada.find(parDer.first) == combinada.end()) {
                                        combinada[parDer.first] = parDer.second;
                                    }
                                }
                                nuevoTrabajo.push_back(std::move(combinada));
                            }
                        }

                        if (!huboMatch && j.tipo == LEFT_JOIN) {
                            Fila combinada = rIzq;
                            if (esqDer) {
                                for (const auto& a : esqDer->atributos) {
                                    combinada[j.tablaDerecha + "." + a.nombre] = "NULL";
                                    if (combinada.find(a.nombre) == combinada.end()) combinada[a.nombre] = "NULL";
                                }
                            }
                            nuevoTrabajo.push_back(std::move(combinada));
                        }
                    }
                    trabajo = std::move(nuevoTrabajo);
                }

                filtradas.clear();
                for (auto& f : trabajo) {
                    bool pasa = true;
                    for (const auto& cond : q.filtros) {
                        if (!evaluarCondicion(f, cond)) { pasa = false; break; }
                    }
                    if (pasa) filtradas.push_back(std::move(f));
                }
            }

            std::vector<Fila> resultado;
            if (!agregaciones.empty() || !q.groupByCols.empty()) {
                std::map<String, std::vector<Fila>> grupos;
                for (const auto& f : filtradas) {
                    String k = "";
                    for (const auto& g : q.groupByCols) {
                        k += (f.count(g) ? f.at(g) : "NULL") + "|";
                    }
                    grupos[k].push_back(f);
                }
                if (grupos.empty() && q.groupByCols.empty()) {
                    Fila f0;
                    for (const auto& ag : agregaciones) f0[ag.alias] = "0";
                    resultado.push_back(f0);
                }
                for (const auto& pg : grupos) {
                    const auto& fgList = pg.second;
                    Fila resG;
                    for (const auto& c : colsSimples) {
                        if (c != "*" && fgList[0].count(c)) resG[c] = fgList[0].at(c);
                    }
                    for (const auto& ag : agregaciones) {
                        if (ag.funcion == "COUNT") {
                            size_t c = 0;
                            for (const auto& r : fgList) {
                                if (ag.columna == "*" || (r.count(ag.columna) && r.at(ag.columna) != "NULL")) c++;
                            }
                            resG[ag.alias] = String(c);
                        } else if (ag.funcion == "SUM" || ag.funcion == "AVG") {
                            double s = 0;
                            for (const auto& r : fgList) if (r.count(ag.columna)) s += r.at(ag.columna).toDouble();
                            resG[ag.alias] = String((ag.funcion == "AVG" && !fgList.empty()) ? (s / fgList.size()) : s, 2);
                        }
                    }
                    resultado.push_back(std::move(resG));
                }
            } else {
                for (auto& f : filtradas) {
                    if (!colsSimples.empty() && colsSimples[0] != "*") {
                        Fila proy;
                        for (const auto& c : colsSimples) {
                            proy[c] = f.count(c) ? f.at(c) : "NULL";
                        }
                        resultado.push_back(std::move(proy));
                    } else if (tieneJoins) {
                        Fila limpia;
                        for (const auto& kv : f) {
                            if (!kv.first.startsWith(q.tabla + ".")) limpia[kv.first] = kv.second;
                        }
                        resultado.push_back(std::move(limpia));
                    } else {
                        resultado.push_back(std::move(f));
                    }
                }
            }

            if (!q.columnaOrden.isEmpty()) {
                std::sort(resultado.begin(), resultado.end(), [&](const Fila& a, const Fila& b) {
                    String vA = a.count(q.columnaOrden) ? a.at(q.columnaOrden) : "";
                    String vB = b.count(q.columnaOrden) ? b.at(q.columnaOrden) : "";
                    if (esNumeroValido(vA) && esNumeroValido(vB)) {
                        return (q.orden == ASC) ? (vA.toDouble() < vB.toDouble()) : (vA.toDouble() > vB.toDouble());
                    }
                    return (q.orden == ASC) ? (vA < vB) : (vA > vB);
                });
            }
            if (q.limite > 0 && static_cast<int>(resultado.size()) > q.limite) {
                resultado.resize(q.limite);
            }
            return resultado;
        }
    };

}