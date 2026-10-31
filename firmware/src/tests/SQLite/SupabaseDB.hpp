#pragma once
#include <Arduino.h>
#include <sqlite3.h>
#include <vector>
#include <map>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>

enum class Rol : uint8_t { NINGUNO = 0, REGULAR = 1, ADMIN = 2 };
using FilaDB = std::map<String, String>;

struct RespuestaDB {
    bool ok = false;
    String error = "";
    std::vector<FilaDB> data;
};

class SupabaseDB {
private:
    sqlite3* db = nullptr;
    Rol rolActual = Rol::NINGUNO;
    String usuarioActual = "";
    uint8_t claveAes[32];

    SupabaseDB() {
        sqlite3_initialize();
        memcpy(claveAes, "UNTDF_ESP32_AES256_MASTER_KEY!!", 32);
    }

public:
    static SupabaseDB& getInstance() {
        static SupabaseDB instance;
        return instance;
    }

    bool abrir(const char* rutaEnSd = "/sd/sistema.db") {
        if (db) sqlite3_close(db);
        if (sqlite3_open(rutaEnSd, &db) != SQLITE_OK) return false;
        
        // Tabla interna de usuarios para Autenticación y Autorización
        ejecutarRaw("CREATE TABLE IF NOT EXISTS _usuarios ("
                    "usuario TEXT PRIMARY KEY, pass_hash TEXT, rol INTEGER);");
        return true;
    }

    // --- CRIPTOGRAFÍA HARDWARE (SHA-256 & AES-256) ---
    String sha256(const String& texto) {
        uint8_t hash[32];
        mbedtls_md_Init_Context:
        mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                   (const uint8_t*)texto.c_str(), texto.length(), hash);
        char hex[65];
        for (int i = 0; i < 32; i++) sprintf(hex + (i * 2), "%02x", hash[i]);
        return String(hex);
    }

    String encriptarCampo(const String& claro) {
        size_t len = ((claro.length() + 15) / 16) * 16;
        std::vector<uint8_t> in(len, 0), out(len, 0);
        memcpy(in.data(), claro.c_str(), claro.length());

        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, claveAes, 256);
        for (size_t i = 0; i < len; i += 16)
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, &in[i], &out[i]);
        mbedtls_aes_free(&aes);

        String hex = "";
        char buf[3];
        for (uint8_t b : out) { sprintf(buf, "%02x", b); hex += buf; }
        return hex;
    }

    String desencriptarCampo(const String& hex) {
        size_t len = hex.length() / 2;
        if (len == 0 || len % 16 != 0) return "";
        std::vector<uint8_t> in(len), out(len + 1, 0);
        for (size_t i = 0; i < len; i++)
            in[i] = strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);

        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_dec(&aes, claveAes, 256);
        for (size_t i = 0; i < len; i += 16)
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, &in[i], &out[i]);
        mbedtls_aes_free(&aes);
        return String((char*)out.data());
    }

    // --- AUTENTICACIÓN Y AUTORIZACIÓN ---
    bool registrarUsuario(const String& user, const String& pass, Rol rol) {
        char* sql = sqlite3_mprintf("INSERT OR REPLACE INTO _usuarios VALUES (%Q, %Q, %d);",
                                    user.c_str(), sha256(pass).c_str(), (int)rol);
        bool ok = ejecutarRaw(sql).ok;
        sqlite3_free(sql);
        return ok;
    }

    bool signIn(const String& user, const String& pass) {
        char* sql = sqlite3_mprintf("SELECT rol FROM _usuarios WHERE usuario=%Q AND pass_hash=%Q;",
                                    user.c_str(), sha256(pass).c_str());
        RespuestaDB r = ejecutarRaw(sql);
        sqlite3_free(sql);
        if (!r.ok || r.data.empty()) { signOut(); return false; }
        usuarioActual = user;
        rolActual = (Rol)r.data[0]["rol"].toInt();
        return true;
    }

    void signOut() { usuarioActual = ""; rolActual = Rol::NINGUNO; }
    Rol obtenerRol() const { return rolActual; }

    RespuestaDB ejecutarRaw(const char* sql) {
        RespuestaDB res;
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql, [](void* data, int argc, char** argv, char** colName) -> int {
            auto* vec = static_cast<std::vector<FilaDB>*>(data);
            FilaDB fila;
            for (int i = 0; i < argc; i++) fila[colName[i]] = argv[i] ? argv[i] : "NULL";
            vec->push_back(fila);
            return 0;
        }, &res.data, &errMsg);

        if (rc != SQLITE_OK) {
            res.ok = false;
            res.error = errMsg ? errMsg : "SQL Error";
            if (errMsg) sqlite3_free(errMsg);
        } else {
            res.ok = true;
        }
        return res;
    }

    // --- FLUENT QUERY BUILDER (ESTILO SUPABASE) ---
    class QueryBuilder {
    private:
        SupabaseDB& padre;
        String tabla, cols = "*", filtro = "", orden = "";
        int limite = -1;
        Rol rolMinimoEscritura = Rol::ADMIN;

    public:
        QueryBuilder(SupabaseDB& p, const String& t) : padre(p), tabla(t) {}

        QueryBuilder& select(const String& columnas = "*") { cols = columnas; return *this; }

        QueryBuilder& eq(const String& col, const String& val) {
            char* esc = sqlite3_mprintf("%s = %Q", col.c_str(), val.c_str());
            filtro += (filtro.isEmpty() ? " WHERE " : " AND ") + String(esc);
            sqlite3_free(esc);
            return *this;
        }

        QueryBuilder& orderBy(const String& col, bool asc = true) {
            orden = " ORDER BY " + col + (asc ? " ASC" : " DESC");
            return *this;
        }

        QueryBuilder& limit(int n) { limite = n; return *this; }

        // SELECT final
        RespuestaDB execute() {
            if (padre.obtenerRol() == Rol::NINGUNO) return { false, "401 No Autenticado", {} };
            String sql = "SELECT " + cols + " FROM " + tabla + filtro + orden;
            if (limite > 0) sql += " LIMIT " + String(limite);
            sql += ";";
            return padre.ejecutarRaw(sql.c_str());
        }

        // INSERT protegido por Rol
        RespuestaDB insert(const FilaDB& datos, Rol rolRequerido = Rol::REGULAR) {
            if (padre.obtenerRol() < rolRequerido) return { false, "403 Prohibido por Rol", {} };
            String k = "", v = "";
            for (auto const& par : datos) {
                char* valEsc = sqlite3_mprintf("%Q", par.second.c_str());
                if (!k.isEmpty()) { k += ","; v += ","; }
                k += par.first; v += valEsc;
                sqlite3_free(valEsc);
            }
            String sql = "INSERT INTO " + tabla + " (" + k + ") VALUES (" + v + ");";
            return padre.ejecutarRaw(sql.c_str());
        }

        // UPDATE protegido por Rol
        RespuestaDB update(const FilaDB& datos, Rol rolRequerido = Rol::ADMIN) {
            if (padre.obtenerRol() < rolRequerido) return { false, "403 Requiere ADMIN", {} };
            String sets = "";
            for (auto const& par : datos) {
                char* valEsc = sqlite3_mprintf("%s=%Q", par.first.c_str(), par.second.c_str());
                if (!sets.isEmpty()) sets += ",";
                sets += valEsc;
                sqlite3_free(valEsc);
            }
            String sql = "UPDATE " + tabla + " SET " + sets + filtro + ";";
            return padre.ejecutarRaw(sql.c_str());
        }
    };

    QueryBuilder from(const String& nombreTabla) { return QueryBuilder(*this, nombreTabla); }
};