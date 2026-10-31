// Archivo: src/tests/SQLite/orm/ORM.hpp
#pragma once
#include "../sgbd/MotorSGBD.hpp"

class ORM {
private:
    ORM() = default;

public:
    static ORM& getInstance() {
        static ORM instance;
        return instance;
    }

    // --- 1. BUILDER DE TABLAS (DDL Estilo Django) ---
    class TableBuilder {
    private:
        SGBD::EsquemaTabla esquema;
        bool esAlter = false, tieneError = false, comiteado = false;
        SGBD::AtributoDef* actual() { return esquema.atributos.empty() ? nullptr : &esquema.atributos.back(); }

    public:
        TableBuilder(const String& nombre, bool alter = false) : esAlter(alter) {
            esquema.nombreTabla = nombre;
            if (alter) {
                const auto* ex = SGBD::CatalogoSistema::getInstance().obtenerEsquema(nombre);
                if (!ex) {
                    SGBD::ConsolaSQL::getInstance().reportarError("42P01", "UNDEFINED_TABLE", "ALTER TABLE", "No existe '" + nombre + "'.");
                    tieneError = true;
                } else esquema = *ex;
            }
        }
        ~TableBuilder() { if (!comiteado) execute(); }

        TableBuilder& addAttribute(const String& n) {
            if (esquema.existeAtributo(n)) {
                SGBD::ConsolaSQL::getInstance().reportarError(
                    "42701", "DUPLICATE_COLUMN", "TABLE '" + esquema.nombreTabla + "'",
                    "El atributo '" + n + "' esta repetido.",
                    "Elimine la llamada duplicada a .addAtribute(\"" + n + "\")."
                );
                tieneError = true;
                return *this;
            }
            SGBD::AtributoDef a; a.nombre = n;
            esquema.atributos.push_back(a);
            return *this;
        }
        TableBuilder& addAtribute(const String& n) { return addAttribute(n); }

        TableBuilder& typeInt()   { if (auto* a = actual()) a->tipo = SGBD::TYPE_INT;   return *this; }
        TableBuilder& typeFloat() { if (auto* a = actual()) a->tipo = SGBD::TYPE_FLOAT; return *this; }
        TableBuilder& typeStr()   { if (auto* a = actual()) a->tipo = SGBD::TYPE_STR;   return *this; }
        TableBuilder& typeSrt()   { return typeStr(); }
        TableBuilder& typeBool()  { if (auto* a = actual()) a->tipo = SGBD::TYPE_BOOL;  return *this; }

        TableBuilder& pk() { if (auto* a = actual()) { a->esPk = a->esNotNull = a->esUnique = true; } return *this; }
        TableBuilder& notNull() { if (auto* a = actual()) a->esNotNull = true; return *this; }
        TableBuilder& unique()  { if (auto* a = actual()) a->esUnique = true;  return *this; }
        TableBuilder& varChar(size_t len) { if (auto* a = actual()) { a->tipo = SGBD::TYPE_STR; a->maxVarchar = len; } return *this; }
        TableBuilder& valueDefault(const String& v) { if (auto* a = actual()) { a->tieneDefault = true; a->valorDefault = v; } return *this; }

        TableBuilder& check(SGBD::CheckPredicado fn, const char* desc = "CHECK") {
            if (auto* a = actual()) { a->fnCheck = fn; a->exprCheckTexto = desc; }
            return *this;
        }

        TableBuilder& fk(const String& tPadre, const String& cPadre, SGBD::AccionFK onDel = SGBD::RESTRICT) {
            if (auto* a = actual()) { a->esFk = true; a->fkTablaDestino = tPadre; a->fkColumnaDestino = cPadre; a->fkOnDelete = onDel; }
            return *this;
        }
        TableBuilder& FK(const String& tPadre, const String& cPadre, SGBD::AccionFK onDel = SGBD::RESTRICT) {
            return fk(tPadre, cPadre, onDel);
        }

        bool execute() {
            comiteado = true;
            if (tieneError) return false;
            return SGBD::MotorSGBD::getInstance().ddl().crearTabla(esquema);
        }
    };

    // --- 2. BUILDER DE ÍNDICES ---
    class IndexBuilder {
    private:
        SGBD::IndiceDef idx;
        bool comiteado = false;
    public:
        explicit IndexBuilder(const String& n) { idx.nombreIndice = n; }
        ~IndexBuilder() { if (!comiteado) execute(); }
        IndexBuilder& attachTable(const String& t) { idx.tabla = t; return *this; }
        IndexBuilder& onAttribute(const String& a) { idx.atributo = a; return *this; }
        IndexBuilder& onAtribute(const String& a)  { return onAttribute(a); }
        IndexBuilder& orderBy(SGBD::Orden o)       { idx.orden = o; return *this; }
        bool execute() {
            comiteado = true;
            return SGBD::CatalogoSistema::getInstance().registrarIndice(idx);
        }
    };

    // --- 3. BUILDER DCL (GRANT / REVOKE) ---
    class DCLBuilder {
    private:
        uint8_t mask; bool modoGrant; String tabla;
    public:
        DCLBuilder(uint8_t m, bool g) : mask(m), modoGrant(g) {}
        DCLBuilder& onTable(const String& t) { tabla = t; return *this; }
        bool toRole(SGBD::RolSGBD r)   { return SGBD::ModuloDCL::getInstance().grant(mask, tabla, r); }
        bool fromRole(SGBD::RolSGBD r) { return SGBD::ModuloDCL::getInstance().revoke(mask, tabla, r); }
    };

    // --- 4. BUILDER DE CONSULTAS (DQL / DML + JOINs) ---
    class QueryBuilder {
    private:
        SGBD::ConsultaDQL q;
    public:
        explicit QueryBuilder(const String& t) { q.tabla = t; }

        QueryBuilder& select(const String& proyeccion = "*") {
            q.proyeccionesRaw.clear();
            int ini = 0;
            while (ini < (int)proyeccion.length()) {
                int coma = proyeccion.indexOf(',', ini);
                if (coma == -1) coma = proyeccion.length();
                String col = proyeccion.substring(ini, coma);
                col.trim();
                if (!col.isEmpty()) q.proyeccionesRaw.push_back(col);
                ini = coma + 1;
            }
            return *this;
        }

        // JOIN automático usando la FK declarada en el esquema: .join("Usuario")
        QueryBuilder& join(const String& tablaDerecha, SGBD::TipoJoin tipo = SGBD::INNER_JOIN) {
            q.joins.push_back({ tablaDerecha, "", "", tipo });
            return *this;
        }

        // JOIN explícito indicando las columnas: .join("Usuario", "id_usuario_fk", "id", INNER_JOIN)
        QueryBuilder& join(const String& tablaDerecha, const String& colIzq, const String& colDer, SGBD::TipoJoin tipo = SGBD::INNER_JOIN) {
            q.joins.push_back({ tablaDerecha, colIzq, colDer, tipo });
            return *this;
        }

        QueryBuilder& innerJoin(const String& tablaDerecha, const String& colIzq = "", const String& colDer = "") {
            return join(tablaDerecha, colIzq, colDer, SGBD::INNER_JOIN);
        }

        QueryBuilder& leftJoin(const String& tablaDerecha, const String& colIzq = "", const String& colDer = "") {
            return join(tablaDerecha, colIzq, colDer, SGBD::LEFT_JOIN);
        }

        QueryBuilder& where(const String& c, SGBD::Operador op, const String& v) { q.filtros.push_back({ c, op, v }); return *this; }
        QueryBuilder& eq(const String& c, const String& v)   { return where(c, SGBD::EQ, v); }
        QueryBuilder& neq(const String& c, const String& v)  { return where(c, SGBD::NEQ, v); }
        QueryBuilder& gt(const String& c, const String& v)   { return where(c, SGBD::GT, v); }
        QueryBuilder& gte(const String& c, const String& v)  { return where(c, SGBD::GTE, v); }
        QueryBuilder& lt(const String& c, const String& v)   { return where(c, SGBD::LT, v); }
        QueryBuilder& lte(const String& c, const String& v)  { return where(c, SGBD::LTE, v); }
        QueryBuilder& like(const String& c, const String& v) { return where(c, SGBD::LIKE, v); }

        QueryBuilder& groupBy(const String& c) { q.groupByCols.push_back(c); return *this; }
        QueryBuilder& orderBy(const String& c, SGBD::Orden o = SGBD::ASC) { q.columnaOrden = c; q.orden = o; return *this; }
        QueryBuilder& limit(int n) { q.limite = n; return *this; }

        std::vector<SGBD::Fila> execute() { return SGBD::MotorSGBD::getInstance().dql().seleccionar(q); }
        bool insert(const SGBD::Fila& f)  { return SGBD::MotorSGBD::getInstance().dml().insertar(q.tabla, f); }
        bool update(const SGBD::Fila& c)  { return SGBD::MotorSGBD::getInstance().dml().actualizar(q.tabla, q.filtros, c); }
        bool remove()                     { return SGBD::MotorSGBD::getInstance().dml().eliminar(q.tabla, q.filtros); }
    };

    TableBuilder createTable(const String& n) { return TableBuilder(n, false); }
    TableBuilder alterTable(const String& n)  { return TableBuilder(n, true); }
    bool truncateTable(const String& n)       { return SGBD::MotorSGBD::getInstance().ddl().truncarTabla(n); }
    IndexBuilder createIndex(const String& n) { return IndexBuilder(n); }
    DCLBuilder grant(uint8_t m)               { return DCLBuilder(m, true); }
    DCLBuilder revoke(uint8_t m)              { return DCLBuilder(m, false); }
    void setSession(const String& u, SGBD::RolSGBD r) { SGBD::ModuloDCL::getInstance().establecerSesion(u, r); }
    QueryBuilder from(const String& t)        { return QueryBuilder(t); }
};

// Instancia global cómoda
inline ORM& db = ORM::getInstance();