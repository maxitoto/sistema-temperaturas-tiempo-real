#pragma once
#include "MotorSGBD.hpp"

class ORM {
private:
    ORM() = default;

public:
    static ORM& getInstance() {
        static ORM instance;
        return instance;
    }

    // =========================================================================
    // 1. TABLE BUILDER (Estilo Django: createTable / alterTable)
    // =========================================================================
    class TableBuilder {
    private:
        SGBD::EsquemaTabla esquema;
        bool esAlter = false;
        bool tieneError = false;
        bool comiteado = false;

        SGBD::AtributoDef* attrActual() {
            if (esquema.atributos.empty()) return nullptr;
            return &esquema.atributos.back();
        }

    public:
        TableBuilder(const String& nombre, bool alter = false) : esAlter(alter) {
            esquema.nombreTabla = nombre;
            if (alter) {
                const auto* existente = SGBD::CatalogoSistema::getInstance().obtenerEsquema(nombre);
                if (!existente) {
                    SGBD::ConsolaSQL::getInstance().reportarError(
                        "42P01", "UNDEFINED_TABLE",
                        "ALTER TABLE '" + nombre + "'",
                        "La tabla '" + nombre + "' no existe."
                    );
                    tieneError = true;
                } else {
                    esquema = *existente;
                }
            }
        }

        // RAII: Al llegar al ';' final de la cadena, valida y guarda automáticamente
        ~TableBuilder() {
            if (!comiteado) execute();
        }

        TableBuilder& addAttribute(const String& nombreAttr) {
            if (esquema.existeAtributo(nombreAttr)) {
                SGBD::ConsolaSQL::getInstance().reportarError(
                    "42701", "DUPLICATE_COLUMN",
                    (esAlter ? "ALTER TABLE '" : "CREATE TABLE '") + esquema.nombreTabla + "'",
                    "El atributo '" + nombreAttr + "' ya fue definido en la tabla '" + esquema.nombreTabla + "'."
                );
                tieneError = true;
                return *this;
            }
            SGBD::AtributoDef nuevo;
            nuevo.nombre = nombreAttr;
            esquema.atributos.push_back(nuevo);
            return *this;
        }

        // Alias exacto para soportar addAtribute(...)
        TableBuilder& addAtribute(const String& nombreAttr) { return addAttribute(nombreAttr); }

        TableBuilder& dropAttribute(const String& nombreAttr) {
            for (auto it = esquema.atributos.begin(); it != esquema.atributos.end(); ++it) {
                if (it->nombre.equalsIgnoreCase(nombreAttr)) {
                    esquema.atributos.erase(it);
                    return *this;
                }
            }
            SGBD::ConsolaSQL::getInstance().reportarError(
                "42703", "UNDEFINED_COLUMN",
                "ALTER TABLE '" + esquema.nombreTabla + "' DROP ATTRIBUTE",
                "No existe el atributo '" + nombreAttr + "'."
            );
            tieneError = true;
            return *this;
        }
        TableBuilder& dropAtribute(const String& nombreAttr) { return dropAttribute(nombreAttr); }

        TableBuilder& typeInt()   { if (auto* a = attrActual()) a->tipo = SGBD::TYPE_INT;   return *this; }
        TableBuilder& typeFloat() { if (auto* a = attrActual()) a->tipo = SGBD::TYPE_FLOAT; return *this; }
        TableBuilder& typeStr()   { if (auto* a = attrActual()) a->tipo = SGBD::TYPE_STR;   return *this; }
        TableBuilder& typeSrt()   { return typeStr(); } // Alias para typeSrt()
        TableBuilder& typeBool()  { if (auto* a = attrActual()) a->tipo = SGBD::TYPE_BOOL;  return *this; }

        TableBuilder& pk() {
            if (auto* a = attrActual()) {
                a->esPk = true;
                a->esNotNull = true;
                a->esUnique = true;
            }
            return *this;
        }

        TableBuilder& autoIncrement() { if (auto* a = attrActual()) a->esAutoIncrement = true; return *this; }
        TableBuilder& notNull()       { if (auto* a = attrActual()) a->esNotNull = true;       return *this; }
        TableBuilder& unique()        { if (auto* a = attrActual()) a->esUnique = true;        return *this; }

        TableBuilder& varChar(size_t maxLen) {
            if (auto* a = attrActual()) {
                a->tipo = SGBD::TYPE_STR;
                a->maxVarchar = maxLen;
            }
            return *this;
        }

        TableBuilder& valueDefault(const String& val) {
            if (auto* a = attrActual()) {
                a->tieneDefault = true;
                a->valorDefault = val;
            }
            return *this;
        }

        TableBuilder& check(SGBD::CheckPredicado fn, const char* descripcionRegla = "CHECK_PREDICATE") {
            if (auto* a = attrActual()) {
                a->fnCheck = fn;
                a->exprCheckTexto = descripcionRegla;
            }
            return *this;
        }

        TableBuilder& fk(const String& tablaPadre, const String& colPadre, SGBD::AccionFK onDelete = SGBD::RESTRICT) {
            if (auto* a = attrActual()) {
                a->esFk = true;
                a->fkTablaDestino = tablaPadre;
                a->fkColumnaDestino = colPadre;
                a->fkOnDelete = onDelete;
            }
            return *this;
        }
        TableBuilder& FK(const String& tablaPadre, const String& colPadre, SGBD::AccionFK onDelete = SGBD::RESTRICT) {
            return fk(tablaPadre, colPadre, onDelete);
        }

        bool execute() {
            comiteado = true;
            if (tieneError) {
                SGBD::ConsolaSQL::getInstance().reportarError(
                    "42000", "DDL_ABORTED",
                    "TABLE '" + esquema.nombreTabla + "'",
                    "Operacion cancelada debido a errores previos en la definicion del esquema."
                );
                return false;
            }
            return SGBD::MotorSGBD::getInstance().ddl().crearTabla(esquema);
        }
    };

    // =========================================================================
    // 2. INDEX BUILDER (createIndex / alterIndex)
    // =========================================================================
    class IndexBuilder {
    private:
        SGBD::IndiceDef idx;
        bool esAlter = false;
        bool comiteado = false;

    public:
        IndexBuilder(const String& nombreIdx, bool alter = false) : esAlter(alter) {
            idx.nombreIndice = nombreIdx;
        }

        ~IndexBuilder() {
            if (!comiteado) execute();
        }

        IndexBuilder& attachTable(const String& tabla) { idx.tabla = tabla; return *this; }
        IndexBuilder& onAttribute(const String& attr)  { idx.atributo = attr; return *this; }
        IndexBuilder& onAtribute(const String& attr)   { return onAttribute(attr); }
        IndexBuilder& orderBy(SGBD::Orden ord)         { idx.orden = ord; return *this; }

        bool execute() {
            comiteado = true;
            if (!SGBD::ModuloDCL::getInstance().autorizar(idx.tabla, SGBD::PERM_DDL, esAlter ? "ALTER INDEX" : "CREATE INDEX")) {
                return false;
            }
            auto* esq = SGBD::CatalogoSistema::getInstance().obtenerEsquema(idx.tabla);
            if (!esq || !esq->existeAtributo(idx.atributo)) {
                SGBD::ConsolaSQL::getInstance().reportarError(
                    "42703", "UNDEFINED_COLUMN",
                    "INDEX '" + idx.nombreIndice + "'",
                    "La tabla '" + idx.tabla + "' o el atributo '" + idx.atributo + "' no existen."
                );
                return false;
            }
            bool ok = SGBD::CatalogoSistema::getInstance().registrarIndice(idx);
            if (ok) {
                SGBD::ConsolaSQL::getInstance().reportarInfo(
                    esAlter ? "ALTER INDEX" : "CREATE INDEX",
                    "Indice '" + idx.nombreIndice + "' sobre " + idx.tabla + "(" + idx.atributo + ") [" + (idx.orden == SGBD::ASC ? "ASC" : "DESC") + "]."
                );
            }
            return ok;
        }
    };

    // =========================================================================
    // 3. DCL BUILDER (grant / revoke fluido)
    // =========================================================================
    class DCLBuilder {
    private:
        uint8_t permisos = 0;
        String tablaObjetivo = "";
        bool esGrant = true;

    public:
        DCLBuilder(uint8_t mask, bool grantMode) : permisos(mask), esGrant(grantMode) {}

        DCLBuilder& onTable(const String& tabla) { tablaObjetivo = tabla; return *this; }

        bool toRole(SGBD::RolSGBD rol) {
            return SGBD::ModuloDCL::getInstance().grant(permisos, tablaObjetivo, rol);
        }

        bool fromRole(SGBD::RolSGBD rol) {
            return SGBD::ModuloDCL::getInstance().revoke(permisos, tablaObjetivo, rol);
        }
    };

    // =========================================================================
    // 4. QUERY BUILDER (Estilo Supabase con Predicados Descriptivos)
    // =========================================================================
    class QueryBuilder {
    private:
        SGBD::ConsultaDQL consulta;

    public:
        explicit QueryBuilder(const String& tabla) { consulta.tabla = tabla; }

        QueryBuilder& select(const String& proyeccion = "*") {
            consulta.proyeccionesRaw.clear();
            int inicio = 0;
            while (inicio < (int)proyeccion.length()) {
                int coma = proyeccion.indexOf(',', inicio);
                if (coma == -1) coma = proyeccion.length();
                String token = proyeccion.substring(inicio, coma);
                token.trim();
                if (!token.isEmpty()) consulta.proyeccionesRaw.push_back(token);
                inicio = coma + 1;
            }
            return *this;
        }

        QueryBuilder& where(const String& col, SGBD::Operador op, const String& val) {
            consulta.filtros.push_back({ col, op, val });
            return *this;
        }

        // Atajos descriptivos estilo Supabase
        QueryBuilder& eq(const String& col, const String& val)   { return where(col, SGBD::EQ, val); }
        QueryBuilder& neq(const String& col, const String& val)  { return where(col, SGBD::NEQ, val); }
        QueryBuilder& gt(const String& col, const String& val)   { return where(col, SGBD::GT, val); }
        QueryBuilder& gte(const String& col, const String& val)  { return where(col, SGBD::GTE, val); }
        QueryBuilder& lt(const String& col, const String& val)   { return where(col, SGBD::LT, val); }
        QueryBuilder& lte(const String& col, const String& val)  { return where(col, SGBD::LTE, val); }
        QueryBuilder& like(const String& col, const String& val) { return where(col, SGBD::LIKE, val); }

        QueryBuilder& groupBy(const String& col) {
            consulta.groupByCols.push_back(col);
            return *this;
        }

        // Uso obligatorio de constante descriptiva ASC o DESC
        QueryBuilder& orderBy(const String& col, SGBD::Orden direccion = SGBD::ASC) {
            consulta.columnaOrden = col;
            consulta.orden = direccion;
            return *this;
        }

        QueryBuilder& limit(int n) {
            consulta.limite = n;
            return *this;
        }

        std::vector<SGBD::Fila> execute() {
            return SGBD::MotorSGBD::getInstance().dql().seleccionar(consulta);
        }

        bool insert(const SGBD::Fila& fila) {
            return SGBD::MotorSGBD::getInstance().dml().insertar(consulta.tabla, fila);
        }

        bool update(const SGBD::Fila& cambios) {
            return SGBD::MotorSGBD::getInstance().dml().actualizar(consulta.tabla, consulta.filtros, cambios);
        }

        bool remove() {
            return SGBD::MotorSGBD::getInstance().dml().eliminar(consulta.tabla, consulta.filtros);
        }
    };

    // --- ENTRYPOINTS DEL ORM ---
    TableBuilder createTable(const String& nombre) { return TableBuilder(nombre, false); }
    TableBuilder alterTable(const String& nombre)  { return TableBuilder(nombre, true); }
    bool dropTable(const String& nombre)           { return SGBD::MotorSGBD::getInstance().ddl().eliminarTabla(nombre); }
    bool truncateTable(const String& nombre)       { return SGBD::MotorSGBD::getInstance().ddl().truncarTabla(nombre); }

    IndexBuilder createIndex(const String& nombreIdx) { return IndexBuilder(nombreIdx, false); }
    IndexBuilder alterIndex(const String& nombreIdx)  { return IndexBuilder(nombreIdx, true); }
    bool dropIndex(const String& nombreIdx)           { return SGBD::CatalogoSistema::getInstance().eliminarIndice(nombreIdx); }

    DCLBuilder grant(uint8_t permisosMask)  { return DCLBuilder(permisosMask, true); }
    DCLBuilder revoke(uint8_t permisosMask) { return DCLBuilder(permisosMask, false); }
    void setSession(const String& usr, SGBD::RolSGBD rol) { SGBD::ModuloDCL::getInstance().establecerSesion(usr, rol); }

    QueryBuilder from(const String& tabla) { return QueryBuilder(tabla); }
};