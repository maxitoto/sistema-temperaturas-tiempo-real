// Archivo: src/tests/SQLite/sgbd/CatalogoSistema.hpp
#pragma once
#include <Arduino.h>
#include <map>
#include <vector>
#include "TiposSGBD.hpp"
#include "../../hardware/driver/tarjetasd/TarjetaSD.hpp"

namespace SGBD {

    using Fila = std::map<String, String>;

    struct AtributoDef {
        String nombre = "";
        TipoDato tipo = TYPE_STR;
        bool esPk = false;
        bool esAutoIncrement = false;
        bool esNotNull = false;
        bool esUnique = false;
        size_t maxVarchar = 0;
        bool tieneDefault = false;
        String valorDefault = "";
        CheckPredicado fnCheck = nullptr;
        String exprCheckTexto = "";
        bool esFk = false;
        String fkTablaDestino = "";
        String fkColumnaDestino = "";
        AccionFK fkOnDelete = RESTRICT;
    };

    struct IndiceDef {
        String nombreIndice = "";
        String tabla = "";
        String atributo = "";
        Orden orden = ASC;
    };

    struct EsquemaTabla {
        String nombreTabla = "";
        std::vector<AtributoDef> atributos;

        bool existeAtributo(const String& nombre) const {
            for (const auto& a : atributos) {
                if (a.nombre.equalsIgnoreCase(nombre)) return true;
            }
            return false;
        }

        const AtributoDef* obtenerAtributo(const String& nombre) const {
            for (const auto& a : atributos) {
                if (a.nombre.equalsIgnoreCase(nombre)) return &a;
            }
            return nullptr;
        }
    };

    class CatalogoSistema {
    private:
        std::map<String, EsquemaTabla> esquemas;
        std::map<String, IndiceDef> indices;
        CatalogoSistema() = default;

    public:
        static CatalogoSistema& getInstance() {
            static CatalogoSistema instance;
            return instance;
        }
        CatalogoSistema(const CatalogoSistema&) = delete;
        CatalogoSistema& operator=(const CatalogoSistema&) = delete;

        String rutaBase() const { return "/db"; }
        String rutaTabla(const String& t) const { return "/db/" + t + ".tbl"; }
        String rutaTmp(const String& t) const   { return "/db/" + t + ".tmp"; }
        String rutaSeq(const String& t) const   { return "/db/" + t + ".seq"; }
        String rutaIdx(const String& i) const   { return "/db/" + i + ".idx"; }

        void registrarEsquema(const EsquemaTabla& esq) { esquemas[esq.nombreTabla] = esq; }
        bool existeEsquema(const String& t) const { return esquemas.find(t) != esquemas.end(); }

        const EsquemaTabla* obtenerEsquema(const String& t) const {
            auto it = esquemas.find(t);
            return (it != esquemas.end()) ? &(it->second) : nullptr;
        }

        const std::map<String, EsquemaTabla>& obtenerTodosLosEsquemas() const { return esquemas; }
        void eliminarEsquema(const String& t) { esquemas.erase(t); }

        bool registrarIndice(const IndiceDef& idx) {
            indices[idx.nombreIndice] = idx;
            String meta = idx.tabla + "|" + idx.atributo + "|" + (idx.orden == ASC ? "ASC" : "DESC") + "\n";
            return TarjetaSD::getInstance().escribirArchivo(rutaIdx(idx.nombreIndice).c_str(), meta.c_str());
        }

        bool eliminarIndice(const String& nombreIdx) {
            if (indices.find(nombreIdx) == indices.end()) return false;
            indices.erase(nombreIdx);
            return TarjetaSD::getInstance().eliminarArchivo(rutaIdx(nombreIdx).c_str());
        }

        uint32_t siguienteId(const String& tabla) {
            TarjetaSD& sd = TarjetaSD::getInstance();
            String rSeq = rutaSeq(tabla);
            String contenido = "0";
            if (sd.existeArchivo(rSeq.c_str())) sd.leerArchivo(rSeq.c_str(), contenido);
            uint32_t nuevoId = contenido.toInt() + 1;
            sd.escribirArchivo(rSeq.c_str(), String(nuevoId).c_str());
            return nuevoId;
        }

        static String serializarFila(const Fila& fila) {
            String linea = "";
            for (const auto& par : fila) {
                if (!linea.isEmpty()) linea += "\t";
                linea += par.first + "=" + par.second;
            }
            return linea + "\n";
        }

        static Fila deserializarFila(const String& linea) {
            Fila fila;
            int inicio = 0;
            while (inicio < (int)linea.length()) {
                int tab = linea.indexOf('\t', inicio);
                if (tab == -1) tab = linea.length();
                String token = linea.substring(inicio, tab);
                int eq = token.indexOf('=');
                if (eq != -1) fila[token.substring(0, eq)] = token.substring(eq + 1);
                inicio = tab + 1;
            }
            return fila;
        }
    };

}