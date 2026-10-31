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

    struct FuncionAgregada {
        String funcion; // COUNT, SUM, AVG, MIN, MAX
        String columna; // "*" o nombre de columna
        String alias;   // ej: "cantidad"
    };

    struct ConsultaDQL {
        String tabla;
        std::vector<String> proyeccionesRaw;
        std::vector<CondicionFiltro> filtros;
        std::vector<String> groupByCols;
        String columnaOrden = "";
        Orden orden = ASC;
        int limite = -1;
    };

    // =========================================================================
    // 1. MÓDULO DDL (Singleton)
    // =========================================================================
    class ModuloDDL {
    private:
        ModuloDDL() = default;
    public:
        static ModuloDDL& getInstance() {
            static ModuloDDL instance;
            return instance;
        }

        bool inicializarAlmacenamiento() {
            TarjetaSD& sd = TarjetaSD::getInstance();
            if (!sd.estaInicializada()) {
                ConsolaSQL::getInstance().reportarError("58030", "IO_ERROR", "INIT_STORAGE", "Tarjeta MicroSD no montada.");
                return false;
            }
            String base = CatalogoSistema::getInstance().rutaBase();
            return sd.existeArchivo(base.c_str()) || sd.crearDirectorio(base.c_str());
        }

        bool crearTabla(const EsquemaTabla& esquema) {
            if (!ModuloDCL::getInstance().autorizar(esquema.nombreTabla, PERM_DDL, "CREATE TABLE")) return false;
            if (!inicializarAlmacenamiento()) return false;

            if (esquema.nombreTabla.isEmpty() || esquema.atributos.empty()) {
                ConsolaSQL::getInstance().reportarError("42601", "SYNTAX_ERROR", "CREATE TABLE", "Tabla sin nombre o sin atributos.");
                return false;
            }

            // Validar que las tablas referenciadas por FK existan en el diccionario
            CatalogoSistema& cat = CatalogoSistema::getInstance();
            for (const auto& attr : esquema.atributos) {
                if (attr.esFk) {
                    const EsquemaTabla* esqPadre = cat.obtenerEsquema(attr.fkTablaDestino);
                    if (!esqPadre || !esqPadre->existeAtributo(attr.fkColumnaDestino)) {
                        ConsolaSQL::getInstance().reportarError(
                            "42830", "INVALID_FOREIGN_KEY",
                            "CREATE TABLE '" + esquema.nombreTabla + "'",
                            "La referencia FK '" + attr.fkTablaDestino + "(" + attr.fkColumnaDestino + ")' no existe."
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
            ConsolaSQL::getInstance().reportarInfo("CREATE TABLE", "Tabla '" + esquema.nombreTabla + "' registrada (" + String(esquema.atributos.size()) + " atributos).");
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

    // =========================================================================
    // 2. MÓDULO DQL (Singleton: Evaluador de predicados, GROUP BY, Agregación)
    // =========================================================================
    class ModuloDQL {
    private:
        ModuloDQL() = default;

        static bool esNumeroValido(const String& s) {
            if (s.isEmpty()) return false;
            int inicio = (s[0] == '-' || s[0] == '+') ? 1 : 0;
            if (inicio >= (int)s.length()) return false;
            bool punto = false;
            for (int i = inicio; i < (int)s.length(); i++) {
                if (s[i] == '.') {
                    if (punto) return false;
                    punto = true;
                } else if (!isdigit(s[i])) {
                    return false;
                }
            }
            return true;
        }

    public:
        static ModuloDQL& getInstance() {
            static ModuloDQL instance;
            return instance;
        }

        static bool evaluarCondicion(const Fila& fila, const CondicionFiltro& cond) {
            auto it = fila.find(cond.columna);
            String valFila = (it != fila.end()) ? it->second : "NULL";

            if (cond.op == LIKE) {
                String patron = cond.valor;
                patron.replace("%", "");
                return valFila.indexOf(patron) != -1;
            }

            if (esNumeroValido(valFila) && esNumeroValido(cond.valor)) {
                double nA = valFila.toDouble();
                double nB = cond.valor.toDouble();
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

        static bool parsearExpresionAgregada(const String& expr, FuncionAgregada& out) {
            String u = expr;
            u.trim();
            String upper = u;
            upper.toUpperCase();

            const char* funcs[] = { "COUNT(", "SUM(", "AVG(", "MIN(", "MAX(" };
            for (const char* f : funcs) {
                if (upper.startsWith(f)) {
                    int cierre = u.indexOf(')');
                    if (cierre == -1) return false;
                    out.funcion = upper.substring(0, strlen(f) - 1);
                    out.columna = u.substring(strlen(f), cierre);
                    out.columna.trim();

                    int posAs = upper.indexOf(" AS ", cierre);
                    if (posAs != -1) {
                        out.alias = u.substring(posAs + 4);
                        out.alias.trim();
                    } else {
                        out.alias = u.substring(0, cierre + 1);
                    }
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
            const EsquemaTabla* esquema = cat.obtenerEsquema(q.tabla);

            // Clasificar proyecciones en columnas simples vs funciones de agregación
            std::vector<String> colsSimples;
            std::vector<FuncionAgregada> agregaciones;

            for (const auto& proy : q.proyeccionesRaw) {
                FuncionAgregada ag;
                if (parsearExpresionAgregada(proy, ag)) {
                    if (esquema && ag.columna != "*" && !esquema->existeAtributo(ag.columna)) {
                        ConsolaSQL::getInstance().reportarError(
                            "42703", "UNDEFINED_COLUMN",
                            "SELECT FROM '" + q.tabla + "'",
                            "La columna '" + ag.columna + "' dentro de " + ag.funcion + "() no existe."
                        );
                        return vacio;
                    }
                    agregaciones.push_back(ag);
                } else {
                    if (esquema && proy != "*" && !esquema->existeAtributo(proy)) {
                        ConsolaSQL::getInstance().reportarError(
                            "42703", "UNDEFINED_COLUMN",
                            "SELECT FROM '" + q.tabla + "'",
                            "La columna '" + proy + "' no existe en la tabla '" + q.tabla + "'."
                        );
                        return vacio;
                    }
                    colsSimples.push_back(proy);
                }
            }

            // Validación estricta SQL: Si hay agregaciones (ej. COUNT) y también columnas simples,
            // TODAS las columnas simples deben estar presentes en GROUP BY.
            if (!agregaciones.empty() && !colsSimples.empty()) {
                for (const auto& c : colsSimples) {
                    if (c == "*") {
                        ConsolaSQL::getInstance().reportarError(
                            "42803", "GROUPING_ERROR",
                            "SELECT FROM '" + q.tabla + "'",
                            "No se puede combinar '*' con funciones de agregacion (" + agregaciones[0].funcion + ") sin especificar columnas en GROUP BY."
                        );
                        return vacio;
                    }
                    bool estaEnGroup = false;
                    for (const auto& g : q.groupByCols) {
                        if (g.equalsIgnoreCase(c)) { estaEnGroup = true; break; }
                    }
                    if (!estaEnGroup) {
                        ConsolaSQL::getInstance().reportarError(
                            "42803", "GROUPING_ERROR",
                            "SELECT FROM '" + q.tabla + "'",
                            "La columna '" + c + "' debe aparecer en la clausula GROUP BY o ser usada en una funcion de agregacion."
                        );
                        return vacio;
                    }
                }
            }

            // Leer y filtrar desde la MicroSD
            std::vector<Fila> filtradas;
            File archivo = SD.open(cat.rutaTabla(q.tabla).c_str(), FILE_READ);
            if (!archivo) return vacio;

            while (archivo.available()) {
                String linea = archivo.readStringUntil('\n');
                linea.trim();
                if (linea.isEmpty()) continue;

                Fila fila = CatalogoSistema::deserializarFila(linea);
                bool pasa = true;
                for (const auto& f : q.filtros) {
                    if (!evaluarCondicion(fila, f)) { pasa = false; break; }
                }
                if (pasa) filtradas.push_back(fila);
            }
            archivo.close();

            std::vector<Fila> resultado;

            // Ejecución de GROUP BY / Agregaciones
            if (!agregaciones.empty() || !q.groupByCols.empty()) {
                std::map<String, std::vector<Fila>> grupos;
                for (const auto& f : filtradas) {
                    String claveGrupo = "";
                    for (const auto& colG : q.groupByCols) {
                        auto it = f.find(colG);
                        claveGrupo += ((it != f.end()) ? it->second : "NULL") + "|";
                    }
                    grupos[claveGrupo].push_back(f);
                }

                // Si había COUNT() sin GROUP BY y la tabla estaba vacía, devolver 1 fila con 0
                if (grupos.empty() && q.groupByCols.empty() && !agregaciones.empty()) {
                    Fila fCero;
                    for (const auto& ag : agregaciones) {
                        fCero[ag.alias] = "0";
                    }
                    resultado.push_back(fCero);
                }

                for (const auto& parGrupo : grupos) {
                    const auto& filasGrupo = parGrupo.second;
                    Fila filaResumen;

                    for (const auto& c : colsSimples) {
                        if (c != "*") filaResumen[c] = filasGrupo[0].at(c);
                    }

                    for (const auto& ag : agregaciones) {
                        if (ag.funcion == "COUNT") {
                            size_t cont = 0;
                            for (const auto& fg : filasGrupo) {
                                if (ag.columna == "*") cont++;
                                else {
                                    auto it = fg.find(ag.columna);
                                    if (it != fg.end() && it->second != "NULL" && !it->second.isEmpty()) cont++;
                                }
                            }
                            filaResumen[ag.alias] = String(cont);
                        } else if (ag.funcion == "SUM" || ag.funcion == "AVG") {
                            double suma = 0;
                            for (const auto& fg : filasGrupo) {
                                auto it = fg.find(ag.columna);
                                if (it != fg.end()) suma += it->second.toDouble();
                            }
                            double val = (ag.funcion == "AVG" && !filasGrupo.empty()) ? (suma / filasGrupo.size()) : suma;
                            filaResumen[ag.alias] = String(val, 2);
                        }
                    }
                    resultado.push_back(filaResumen);
                }
            } else {
                // Proyección normal sin agregaciones
                for (auto& f : filtradas) {
                    if (!colsSimples.empty() && colsSimples[0] != "*") {
                        Fila proy;
                        for (const auto& c : colsSimples) proy[c] = f[c];
                        resultado.push_back(proy);
                    } else {
                        resultado.push_back(f);
                    }
                }
            }

            // ORDER BY (ASC / DESC)
            if (!q.columnaOrden.isEmpty()) {
                std::sort(resultado.begin(), resultado.end(), [&](const Fila& a, const Fila& b) {
                    auto itA = a.find(q.columnaOrden);
                    auto itB = b.find(q.columnaOrden);
                    String vA = (itA != a.end()) ? itA->second : "";
                    String vB = (itB != b.end()) ? itB->second : "";
                    if (esNumeroValido(vA) && esNumeroValido(vB)) {
                        double nA = vA.toDouble(), nB = vB.toDouble();
                        return (q.orden == ASC) ? (nA < nB) : (nA > nB);
                    }
                    return (q.orden == ASC) ? (vA < vB) : (vA > vB);
                });
            }

            // LIMIT
            if (q.limite > 0 && (int)resultado.size() > q.limite) {
                resultado.resize(q.limite);
            }

            return resultado;
        }
    };

    // =========================================================================
    // 3. MÓDULO DML (Singleton: Validación de Esquema, CHECK, FK y CASCADE)
    // =========================================================================
    class ModuloDML {
    private:
        ModuloDML() = default;

        bool validarTipoDato(TipoDato tipo, const String& valor) const {
            if (valor == "NULL") return true;
            if (tipo == TYPE_INT) {
                for (size_t i = (valor[0] == '-' ? 1 : 0); i < valor.length(); i++) {
                    if (!isdigit(valor[i])) return false;
                }
                return !valor.isEmpty();
            }
            if (tipo == TYPE_FLOAT) {
                bool punto = false;
                for (size_t i = (valor[0] == '-' ? 1 : 0); i < valor.length(); i++) {
                    if (valor[i] == '.') { if (punto) return false; punto = true; }
                    else if (!isdigit(valor[i])) return false;
                }
                return !valor.isEmpty();
            }
            return true;
        }

        bool validarRestriccionesFila(const EsquemaTabla& esq, Fila& fila, bool esUpdate = false) {
            CatalogoSistema& cat = CatalogoSistema::getInstance();

            // 1. Verificar que no se envíen columnas inexistentes
            for (const auto& par : fila) {
                if (!esq.existeAtributo(par.first)) {
                    ConsolaSQL::getInstance().reportarError(
                        "42703", "UNDEFINED_COLUMN",
                        "DML INTO '" + esq.nombreTabla + "'",
                        "El atributo '" + par.first + "' no existe en el esquema de '" + esq.nombreTabla + "'."
                    );
                    return false;
                }
            }

            // 2. Validar cada atributo definido en el esquema físico
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
                            "23502", "NOT_NULL_VIOLATION",
                            "INSERT INTO '" + esq.nombreTabla + "'",
                            "El atributo obligatorio '" + attr.nombre + "' (NOT NULL) no fue proporcionado."
                        );
                        return false;
                    } else {
                        fila[attr.nombre] = "NULL";
                        presente = true;
                    }
                }

                if (!presente) continue;
                String val = fila[attr.nombre];

                // NOT NULL check
                if (attr.esNotNull && (val == "NULL" || (val.isEmpty() && !attr.tieneDefault))) {
                    ConsolaSQL::getInstance().reportarError(
                        "23502", "NOT_NULL_VIOLATION",
                        "DML ON '" + esq.nombreTabla + "'",
                        "El atributo '" + attr.nombre + "' no admite valores nulos o vacios."
                    );
                    return false;
                }

                if (val == "NULL") continue;

                // Tipo de dato
                if (!validarTipoDato(attr.tipo, val)) {
                    ConsolaSQL::getInstance().reportarError(
                        "22P02", "INVALID_TEXT_REPRESENTATION",
                        "DML ON '" + esq.nombreTabla + "." + attr.nombre + "'",
                        "El valor '" + val + "' no coincide con el tipo de dato esperado."
                    );
                    return false;
                }

                // VARCHAR(N)
                if (attr.maxVarchar > 0 && val.length() > attr.maxVarchar) {
                    ConsolaSQL::getInstance().reportarError(
                        "22001", "STRING_DATA_RIGHT_TRUNCATION",
                        "DML ON '" + esq.nombreTabla + "." + attr.nombre + "'",
                        "Valor '" + val + "' (" + String(val.length()) + " chars) excede VARCHAR(" + String(attr.maxVarchar) + ")."
                    );
                    return false;
                }

                // CHECK(predicado)
                if (attr.fnCheck && !attr.fnCheck(val)) {
                    ConsolaSQL::getInstance().reportarError(
                        "23514", "CHECK_VIOLATION",
                        "DML ON '" + esq.nombreTabla + "." + attr.nombre + "'",
                        "El valor '" + val + "' fallo la restriccion CHECK (" + attr.exprCheckTexto + ")."
                    );
                    return false;
                }

                // UNIQUE / PK (si es INSERT)
                if (!esUpdate && (attr.esPk || attr.esUnique)) {
                    ConsultaDQL qUniq;
                    qUniq.tabla = esq.nombreTabla;
                    qUniq.proyeccionesRaw = { attr.nombre };
                    qUniq.filtros.push_back({ attr.nombre, EQ, val });
                    auto dup = ModuloDQL::getInstance().seleccionar(qUniq, true);
                    if (!dup.empty()) {
                        ConsolaSQL::getInstance().reportarError(
                            "23505", "UNIQUE_VIOLATION",
                            "INSERT INTO '" + esq.nombreTabla + "'",
                            "Valor duplicado '" + val + "' viola la restriccion UNIQUE/PK en '" + attr.nombre + "'."
                        );
                        return false;
                    }
                }

                // FOREIGN KEY (Verificar que exista en la tabla padre)
                if (attr.esFk) {
                    ConsultaDQL qFk;
                    qFk.tabla = attr.fkTablaDestino;
                    qFk.proyeccionesRaw = { attr.fkColumnaDestino };
                    qFk.filtros.push_back({ attr.fkColumnaDestino, EQ, val });
                    auto padre = ModuloDQL::getInstance().seleccionar(qFk, true);
                    if (padre.empty()) {
                        ConsolaSQL::getInstance().reportarError(
                            "23503", "FOREIGN_KEY_VIOLATION",
                            "DML ON '" + esq.nombreTabla + "." + attr.nombre + "'",
                            "La clave (" + attr.nombre + "=" + val + ") no existe en la tabla referenciada '" + attr.fkTablaDestino + "(" + attr.fkColumnaDestino + ")'."
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

        bool insertar(const String& tabla, Fila fila) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_INSERT, "INSERT")) return false;

            CatalogoSistema& cat = CatalogoSistema::getInstance();
            const EsquemaTabla* esq = cat.obtenerEsquema(tabla);
            if (esq) {
                if (!validarRestriccionesFila(*esq, fila, false)) return false;
            } else {
                ModuloDDL::getInstance().inicializarAlmacenamiento();
                if (fila.find("id") == fila.end()) fila["id"] = String(cat.siguienteId(tabla));
            }

            String linea = CatalogoSistema::serializarFila(fila);
            bool ok = TarjetaSD::getInstance().anexarArchivo(cat.rutaTabla(tabla).c_str(), linea.c_str());
            if (ok) ConsolaSQL::getInstance().reportarInfo("INSERT", "1 fila insertada en '" + tabla + "'.");
            return ok;
        }

        bool actualizar(const String& tabla, const std::vector<CondicionFiltro>& filtros, Fila cambios) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_UPDATE, "UPDATE")) return false;

            CatalogoSistema& cat = CatalogoSistema::getInstance();
            const EsquemaTabla* esq = cat.obtenerEsquema(tabla);
            if (esq && !validarRestriccionesFila(*esq, cambios, true)) return false;

            File fIn = SD.open(cat.rutaTabla(tabla).c_str(), FILE_READ);
            if (!fIn) return false;
            File fOut = SD.open(cat.rutaTmp(tabla).c_str(), FILE_WRITE);
            if (!fOut) { fIn.close(); return false; }

            size_t modificadas = 0;
            while (fIn.available()) {
                String linea = fIn.readStringUntil('\n');
                linea.trim();
                if (linea.isEmpty()) continue;

                Fila fila = CatalogoSistema::deserializarFila(linea);
                bool coincide = true;
                for (const auto& c : filtros) {
                    if (!ModuloDQL::evaluarCondicion(fila, c)) { coincide = false; break; }
                }
                if (coincide) {
                    for (const auto& kv : cambios) fila[kv.first] = kv.second;
                    modificadas++;
                }
                fOut.print(CatalogoSistema::serializarFila(fila));
            }
            fIn.close();
            fOut.close();

            SD.remove(cat.rutaTabla(tabla).c_str());
            SD.rename(cat.rutaTmp(tabla).c_str(), cat.rutaTabla(tabla).c_str());
            ConsolaSQL::getInstance().reportarInfo("UPDATE", String(modificadas) + " fila(s) actualizada(s) en '" + tabla + "'.");
            return true;
        }

        // DELETE con soporte de Integridad Referencial (RESTRICT, CASCADE, SET_NULL)
        bool eliminar(const String& tabla, const std::vector<CondicionFiltro>& filtros) {
            if (!ModuloDCL::getInstance().autorizar(tabla, PERM_DELETE, "DELETE")) return false;

            CatalogoSistema& cat = CatalogoSistema::getInstance();
            const EsquemaTabla* esq = cat.obtenerEsquema(tabla);

            // 1. Identificar qué filas van a ser eliminadas para verificar FKs hijas
            ConsultaDQL qBorrar;
            qBorrar.tabla = tabla;
            qBorrar.proyeccionesRaw = { "*" };
            qBorrar.filtros = filtros;
            auto aBorrar = ModuloDQL::getInstance().seleccionar(qBorrar, true);
            if (aBorrar.empty()) return true;

            // 2. Revisar todas las tablas del catálogo que apunten a esta tabla vía FK
            if (esq) {
                for (const auto& parEsq : cat.obtenerTodosLosEsquemas()) {
                    const EsquemaTabla& esqHija = parEsq.second;
                    for (const auto& attrHijo : esqHija.atributos) {
                        if (attrHijo.esFk && attrHijo.fkTablaDestino.equalsIgnoreCase(tabla)) {
                            for (const auto& filaPadre : aBorrar) {
                                String valPadre = filaPadre.at(attrHijo.fkColumnaDestino);
                                std::vector<CondicionFiltro> filtroHijo = { { attrHijo.nombre, EQ, valPadre } };

                                if (attrHijo.fkOnDelete == RESTRICT) {
                                    ConsultaDQL qHijos;
                                    qHijos.tabla = esqHija.nombreTabla;
                                    qHijos.proyeccionesRaw = { "*" };
                                    qHijos.filtros = filtroHijo;
                                    if (!ModuloDQL::getInstance().seleccionar(qHijos, true).empty()) {
                                        ConsolaSQL::getInstance().reportarError(
                                            "23503", "FOREIGN_KEY_VIOLATION",
                                            "DELETE FROM '" + tabla + "'",
                                            "No se puede eliminar (" + attrHijo.fkColumnaDestino + "=" + valPadre + ") porque tiene registros dependientes en '" + esqHija.nombreTabla + "' (ON DELETE RESTRICT)."
                                        );
                                        return false;
                                    }
                                } else if (attrHijo.fkOnDelete == CASCADE) {
                                    ConsolaSQL::getInstance().reportarInfo(
                                        "ON DELETE CASCADE",
                                        "Propagando eliminacion hacia '" + esqHija.nombreTabla + "' donde " + attrHijo.nombre + "=" + valPadre
                                    );
                                    eliminar(esqHija.nombreTabla, filtroHijo);
                                } else if (attrHijo.fkOnDelete == SET_NULL) {
                                    actualizar(esqHija.nombreTabla, filtroHijo, { { attrHijo.nombre, "NULL" } });
                                }
                            }
                        }
                    }
                }
            }

            // 3. Ejecutar el borrado físico reescribiendo el archivo sin las filas eliminadas
            File fIn = SD.open(cat.rutaTabla(tabla).c_str(), FILE_READ);
            if (!fIn) return false;
            File fOut = SD.open(cat.rutaTmp(tabla).c_str(), FILE_WRITE);
            if (!fOut) { fIn.close(); return false; }

            size_t borradas = 0;
            while (fIn.available()) {
                String linea = fIn.readStringUntil('\n');
                linea.trim();
                if (linea.isEmpty()) continue;

                Fila fila = CatalogoSistema::deserializarFila(linea);
                bool coincide = true;
                for (const auto& c : filtros) {
                    if (!ModuloDQL::evaluarCondicion(fila, c)) { coincide = false; break; }
                }
                if (coincide) {
                    borradas++;
                } else {
                    fOut.print(CatalogoSistema::serializarFila(fila));
                }
            }
            fIn.close();
            fOut.close();

            SD.remove(cat.rutaTabla(tabla).c_str());
            SD.rename(cat.rutaTmp(tabla).c_str(), cat.rutaTabla(tabla).c_str());
            ConsolaSQL::getInstance().reportarInfo("DELETE", String(borradas) + " fila(s) eliminada(s) de '" + tabla + "'.");
            return true;
        }
    };

    // =========================================================================
    // 4. MOTOR SGBD (Singleton Orquestador Principal)
    // =========================================================================
    class MotorSGBD {
    private:
        MotorSGBD() = default;
    public:
        static MotorSGBD& getInstance() {
            static MotorSGBD instance;
            return instance;
        }
        MotorSGBD(const MotorSGBD&) = delete;
        MotorSGBD& operator=(const MotorSGBD&) = delete;

        CatalogoSistema& catalogo() { return CatalogoSistema::getInstance(); }
        ModuloDDL&       ddl()      { return ModuloDDL::getInstance(); }
        ModuloDML&       dml()      { return ModuloDML::getInstance(); }
        ModuloDQL&       dql()      { return ModuloDQL::getInstance(); }
        ModuloDCL&       dcl()      { return ModuloDCL::getInstance(); }
        ConsolaSQL&      consola()  { return ConsolaSQL::getInstance(); }
    };

}