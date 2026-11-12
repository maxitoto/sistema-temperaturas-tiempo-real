#pragma once
#include <Arduino.h>
#include <vector>
#include <cstring>
#include "../../DB_DDL/standard/PostgresDataTypes.hpp"
#include "../../DB_DDL/engine/PhysicalSchemaCatalog.hpp"

namespace DbRuntime::Storage {

    using DbNotRuntime::Standard::PgDataType;

    // Representa una celda individual tipada sin usar RTTI ni polimorfismo virtual.
    struct PgCell {
        char columnName[32] = {0};
        PgDataType type     = PgDataType::TEXT;
        bool isNull         = false;

        union NumericStorage {
            int64_t i64;
            float   f32;
            bool    b;
            NumericStorage() : i64(0) {}
        } num;

        String rawText = "";

        PgCell() = default;

        // Construye una celda asignando el nombre truncado a 31 bytes y sincronizando el valor numérico en la unión.
        PgCell(const String& col, const String& val, PgDataType dt = PgDataType::TEXT) : type(dt) {
            setColumnName(col);
            setValue(val, dt);
        }

        // Copia el nombre de la columna en el buffer fijo de 32 bytes evitando una alocación de String adicional.
        void setColumnName(const String& col) {
            strncpy(columnName, col.c_str(), sizeof(columnName) - 1);
            columnName[sizeof(columnName) - 1] = '\0';
        }

        // Sincroniza el literal con la unión tipada para que las comparaciones posteriores sean numéricas en 1 ciclo.
        void setValue(const String& literal, PgDataType dt = PgDataType::TEXT) {
            type = dt;
            if (literal.equalsIgnoreCase("NULL")) {
                isNull = true;
                rawText = "NULL";
                num.i64 = 0;
                return;
            }
            isNull  = false;
            rawText = literal;

            switch (dt) {
                case PgDataType::SMALLINT:
                case PgDataType::INTEGER:
                case PgDataType::BIGINT:
                    num.i64 = atoll(literal.c_str());
                    break;
                case PgDataType::DECIMAL:
                case PgDataType::NUMERIC:
                case PgDataType::DECFLOAT:
                case PgDataType::REAL:
                case PgDataType::DOUBLE_PRECISION:
                    num.f32 = literal.toFloat();
                    break;
                case PgDataType::BOOLEAN: {
                    String up = literal;
                    up.toUpperCase();
                    num.b   = (up == "TRUE" || up == "1" || up == "T");
                    rawText = num.b ? "TRUE" : "FALSE";
                    break;
                }
                default:
                    num.i64 = 0;
                    break;
            }
        }

        // Devuelve el valor entero de la celda en O(1).
        int64_t asInt64() const {
            if (isNull) return 0;
            if (type == PgDataType::SMALLINT || type == PgDataType::INTEGER || type == PgDataType::BIGINT) {
                return num.i64;
            }
            return atoll(rawText.c_str());
        }

        // Devuelve el valor en punto flotante simple (FPU hardware del ESP32) en O(1).
        float asFloat() const {
            if (isNull) return 0.0f;
            if (type == PgDataType::REAL || type == PgDataType::DOUBLE_PRECISION ||
                type == PgDataType::NUMERIC || type == PgDataType::DECIMAL) {
                return num.f32;
            }
            return rawText.toFloat();
        }

        // Devuelve el valor booleano en O(1).
        bool asBool() const {
            if (isNull) return false;
            if (type == PgDataType::BOOLEAN) return num.b;
            return rawText.equalsIgnoreCase("TRUE") || rawText == "1" || rawText.equalsIgnoreCase("T");
        }
    };

    // Representa una fila física en un vector plano cache-friendly (reemplaza a std::map<String, String>).
    class PgTuple {
    private:
        std::vector<PgCell> cells;

    public:
        PgTuple() = default;

        // Reserva espacio contiguo para N columnas evitando realocaciones durante la lectura.
        void reserve(size_t n) { cells.reserve(n); }

        // Busca una celda por nombre (case-insensitive o con prefijo tabla.col) en tiempo lineal sobre memoria contigua.
        const PgCell* findCell(const String& colName) const {
            const char* target = colName.c_str();
            // 1. Prioridad absoluta a coincidencia exacta (evita colisión en UPDATE FROM / DELETE USING)
            for (const auto& c : cells) {
                if (strcasecmp(c.columnName, target) == 0) return &c;
            }
            // 2. Fallback sin prefijo "Tabla.columna" para tuplas ya proyectadas
            const char* targetDot = strchr(target, '.');
            for (const auto& c : cells) {
                const char* cellDot = strchr(c.columnName, '.');
                if (cellDot && strcasecmp(cellDot + 1, target) == 0) return &c;
                if (targetDot && strcasecmp(c.columnName, targetDot + 1) == 0) return &c;
            }
            return nullptr;
        }

        // Busca una celda mutable por nombre para actualizaciones in-place.
        PgCell* findCellMutable(const String& colName) {
            const char* target = colName.c_str();
            for (auto& c : cells) {
                if (strcasecmp(c.columnName, target) == 0) return &c;
            }
            return nullptr;
        }

        // Inserta o actualiza una columna en la tupla.
        void set(const String& colName, const String& value, PgDataType dt = PgDataType::TEXT) {
            PgCell* existing = findCellMutable(colName);
            if (existing) {
                existing->setValue(value, dt);
                return;
            }
            cells.emplace_back(colName, value, dt);
        }

        // Elimina una columna de la tupla (útil para proyecciones o exclusiones).
        void removeColumn(const String& colName) {
            const char* target = colName.c_str();
            for (auto it = cells.begin(); it != cells.end(); ++it) {
                if (strcasecmp(it->columnName, target) == 0) {
                    cells.erase(it);
                    return;
                }
            }
        }

        // Obtiene el texto de una columna o un fallback seguro (sin disparar abort() como .at()).
        String getText(const char* colName, const char* fallback = "") const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->rawText : String(fallback);
        }

        int32_t getInt(const char* colName, int32_t fallback = 0) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? static_cast<int32_t>(c->asInt64()) : fallback;
        }

        int64_t getBigInt(const char* colName, int64_t fallback = 0) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asInt64() : fallback;
        }

        float getFloat(const char* colName, float fallback = 0.0f) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asFloat() : fallback;
        }

        bool getBool(const char* colName, bool fallback = false) const {
            const PgCell* c = findCell(colName);
            return (c && !c->isNull) ? c->asBool() : fallback;
        }

        bool isNull(const char* colName) const {
            const PgCell* c = findCell(colName);
            return (!c || c->isNull);
        }

        const std::vector<PgCell>& allCells() const { return cells; }
        std::vector<PgCell>& allCellsMutable() { return cells; }
        bool empty() const { return cells.empty(); }
        size_t size() const { return cells.size(); }

        // Mapea directamente la tupla a cualquier struct C++ del usuario mediante un functor ligero.
        template <typename TDto, typename MapperFn>
        TDto mapToStruct(MapperFn mapper) const {
            return mapper(*this);
        }

        // Serializa la tupla al formato físico TSV compatible con los archivos .tbl de la MicroSD.
        String serializeTsv() const {
            String line = "";
            line.reserve(cells.size() * 20);
            for (size_t i = 0; i < cells.size(); ++i) {
                if (i > 0) line += "\t";
                line += String(cells[i].columnName) + "=" + cells[i].rawText;
            }
            line += "\n";
            return line;
        }

        // Deserializa una línea TSV desde la MicroSD tipando cada celda según el esquema del catálogo DDL.
        static PgTuple deserializeTsv(const String& line, const DbNotRuntime::Engine::TablePhysicalSchema* schema = nullptr) {
            PgTuple tuple;
            String l = line;
            l.trim();
            if (l.isEmpty()) return tuple;

            if (schema) tuple.reserve(schema->columns.size());

            int start = 0;
            const int len = static_cast<int>(l.length());
            while (start < len) {
                int tab = l.indexOf('\t', start);
                if (tab == -1) tab = len;
                int eq = l.indexOf('=', start);
                if (eq != -1 && eq < tab) {
                    String colName = l.substring(start, eq);
                    String valStr  = l.substring(eq + 1, tab);
                    PgDataType dt  = PgDataType::TEXT;
                    if (schema) {
                        const auto* colDef = schema->findColumn(colName);
                        if (colDef) dt = colDef->dataType;
                    }
                    tuple.set(colName, valStr, dt);
                }
                start = tab + 1;
            }
            return tuple;
        }
    };

} // namespace DbRuntime::Storage