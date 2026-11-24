// src/tests/web_server/main.cpp
//
// ⚠️ IMPORTANTE: este binario NO ejecuta DDL.
// El DDL (creación de tablas, catálogo, índices en SD) se corrió una vez
// con el binario de provisioning (src/tests/DB_DDL/main.cpp).
//
// Este binario:
//   1. Lee los tipados de los modelos (fields/meta) → arma catálogo en RAM
//   2. Carga los .tbl de la SD (RuntimeStorageAndIndexManager)
//   3. Opera con DQL/DML (Orm::Sensor, Orm::Censo, ModuloDQL, ModuloDML)
//
// NO se tocan archivos en SD. NO se ejecuta ModuloDDL.

#include <Arduino.h>
#include <WiFi.h>
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_log.h"

// mbedTLS
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pem.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"

#include <time.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Tipados de DDL (structs, enums, modelos). NO se ejecuta ModuloDDL.
#include "database/DB_DDL/engine/PhysicalSchemaCatalog.hpp"
#include "database/DB_DDL/models/PhysicalModelsDefinition.hpp"

// Runtime DQL/DML + ORM
#include "database/DB_DQL_DML/orm/DjangoManager.hpp"
#include "database/DB_DQL_DML/storage/RuntimeIndexAccessPath.hpp"

// Sensores
#include "hardware/driver/gestorsensores/GestorSensores.hpp"

// ================ ALIASES ================
namespace Orm      = DbRuntime::Orm;
namespace Storage  = DbRuntime::Storage;
namespace PgStd    = DbNotRuntime::Standard;
namespace SqlRt    = DbRuntime::Standard;
namespace Catalog  = DbNotRuntime::Engine;

using PgDataType = DbNotRuntime::Standard::PgDataType;
using Field      = DbNotRuntime::Models::Field;
using MetaOpts   = DbNotRuntime::Models::MetaOptions;

[[maybe_unused]] static const char *TAG = "WEB_SERVER";

// ================ CONFIG ================
const char* ssid     = "Zhone_96F8";
const char* password = "Ushuaia30";
// ========================================

static httpd_handle_t server = nullptr;

static char cert_pem_buf[4096];
static char key_pem_buf[4096];

static SemaphoreHandle_t dbMutex       = nullptr;
static SemaphoreHandle_t lecturasMutex = nullptr;
static SemaphoreHandle_t loaderDone    = nullptr;

struct LecturaCache {
    bool     activo    = false;
    char     mac[17]   = {0};
    float    tempC     = 0.0f;
    int16_t  estado    = 0;
    uint32_t timestamp = 0;
};
static LecturaCache lecturas[GestorSensores::MAX_SENSORES];

struct SensorCache {
    int32_t idSensor = -1;
    char    mac[17]  = {0};
};
static SensorCache sensorCache[GestorSensores::MAX_SENSORES];

// ==================================================
// Certificado auto-firmado (idéntico al de provisioning)
// ==================================================
static void formatTime(time_t t, char *buf, size_t len) {
    struct tm tm_info;
    gmtime_r(&t, &tm_info);
    strftime(buf, len, "%Y%m%d%H%M%S", &tm_info);
}

static bool generateSelfSignedCert() {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_pk_context pk;
    mbedtls_x509write_cert crt;
    unsigned char serial_bytes[4] = {0x01, 0x02, 0x03, 0x04};
    char not_before[20], not_after[20];

    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_pk_init(&pk);
    mbedtls_x509write_crt_init(&crt);

    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char*)"esp32_https", 11);
    if (ret != 0) { ESP_LOGE(TAG, "DRBG: -0x%04X", -ret); return false; }

    ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) { ESP_LOGE(TAG, "PK setup: -0x%04X", -ret); return false; }

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk), mbedtls_ctr_drbg_random,
                               &ctr_drbg, 2048, 65537);
    if (ret != 0) { ESP_LOGE(TAG, "RSA: -0x%04X", -ret); return false; }

    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_name(&crt, "CN=ESP32 HTTPS Server, O=MyCompany, C=ES");
    mbedtls_x509write_crt_set_issuer_name(&crt, "CN=ESP32 HTTPS Server, O=MyCompany, C=ES");

    ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial_bytes, sizeof(serial_bytes));
    if (ret != 0) { ESP_LOGE(TAG, "Serial: -0x%04X", -ret); return false; }

    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);

    time_t now = time(NULL);
    formatTime(now - 60, not_before, sizeof(not_before));
    formatTime(now + 365 * 24 * 3600, not_after, sizeof(not_after));
    ret = mbedtls_x509write_crt_set_validity(&crt, not_before, not_after);
    if (ret != 0) { ESP_LOGE(TAG, "Validity: -0x%04X", -ret); return false; }

    ret = mbedtls_x509write_crt_pem(&crt, (unsigned char*)cert_pem_buf,
                                     sizeof(cert_pem_buf),
                                     mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) { ESP_LOGE(TAG, "Cert PEM: -0x%04X", -ret); return false; }

    ret = mbedtls_pk_write_key_pem(&pk, (unsigned char*)key_pem_buf, sizeof(key_pem_buf));
    if (ret != 0) { ESP_LOGE(TAG, "Key PEM: -0x%04X", -ret); return false; }

    mbedtls_x509write_crt_free(&crt);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return true;
}

// ==================================================
// REGISTRO DE ENUMS (NO es DDL: solo pobla metadata en RAM)
// ==================================================
static void registerAllEnumsInRam() {
    auto& cat = Catalog::PhysicalSchemaCatalog::getInstance();

    auto reg = [&](const char* name, std::vector<String> labels) {
        PgStd::EnumTypeDefinition def;
        def.typeName = name;
        def.labels   = std::move(labels);
        cat.registerEnum(def);
    };

    reg("rol_usuario_enum",            {"ANONIMO", "LECTOR", "OPERADOR", "ADMIN"});
    reg("criticidad_evento_enum",      {"BAJA", "MEDIA", "CRITICA"});
    reg("tipo_recepcion_correo_enum",  {"TODOS", "EVENTOS", "CRITICOS"});
    reg("estado_envio_correo_enum",    {"PENDIENTE", "ENVIADO", "FALLIDO"});
    reg("medio_conexion_enum",         {"INTERNO", "USB", "WIFI_STA", "WIFI_AP"});
    reg("resultado_auditoria_enum",    {"EXITO", "DENEGADO_ROL", "CREDENCIAL_INVALIDA",
                                        "INTRUSION_TERMINAL", "ERROR_VALIDACION"});
    ESP_LOGI(TAG, "[loader] 6 ENUMs registrados en RAM");
}

// ==================================================
// LOADER: lee los modelos (tipados DDL) y arma el catálogo en RAM.
// NO crea tablas, NO escribe archivos, NO ejecuta ModuloDDL.
// ==================================================
template <typename TModel>
static void registerModelInCatalog(const TModel& model) {
    auto& cat = Catalog::PhysicalSchemaCatalog::getInstance();

    MetaOpts m = model.meta();
    std::vector<Field> fList = model.fields();

    // 1. Construir el TablePhysicalSchema (equivalente a lo que armaría DeterministicTableBuilder)
    Catalog::TablePhysicalSchema tb;
    tb.schemaName  = "public";
    tb.tableName   = m.db_table;
    tb.persistence = m.persistence;
    tb.tablespace  = m.db_tablespace;
    tb.ownerRole   = "postgres";

    for (const auto& f : fList) {
        const auto& c = f.compiled();
        tb.columns.push_back(c);

        // Si la columna es IDENTITY, registrar su secuencia en RAM
        if (c.identityMode != PgStd::IdentityGeneration::NONE) {
            PgStd::SequenceDefinition seq;
            seq.sequenceName  = tb.tableName + "_" + c.columnName + "_seq";
            seq.dataType      = c.dataType;
            seq.startValue    = c.identityStart;
            seq.incrementBy   = c.identityIncrement;
            seq.ownedByTable  = tb.tableName;
            seq.ownedByColumn = c.columnName;
            cat.registerSequence(seq);
        }
    }

    // 2. Restricciones de tabla (UNIQUE compuestos, PK compuesta, etc.)
    for (const auto& uq : m.unique_constraints) {
        PgStd::TableLevelConstraintDef tc;
        tc.constraintName = uq.name;
        tc.type           = PgStd::TableLevelConstraintDef::Type::UNIQUE;
        tc.localColumns   = uq.fields;
        tb.tableConstraints.push_back(tc);
    }

    cat.registerTable(tb);

    // 3. Índices declarados en Meta.indexes
    for (const auto& idx : m.indexes) {
        PgStd::IndexDefinition idxDef;
        idxDef.indexName  = idx.name;
        idxDef.tableName  = m.db_table;
        idxDef.isUnique   = idx.unique;
        idxDef.method     = idx.method;
        idxDef.keyColumns = idx.fields;
        idxDef.tablespace = m.db_tablespace;
        cat.registerIndex(idxDef);
    }

    ESP_LOGI(TAG, "[loader] %s → %u columnas, %u constraints, %u índices",
             tb.tableName.c_str(),
             (unsigned)tb.columns.size(),
             (unsigned)tb.tableConstraints.size(),
             (unsigned)m.indexes.size());
}

static void loaderTask(void* /*param*/) {
    ESP_LOGI(TAG, "[loader] construyendo catálogo en RAM (sin DDL)...");

    registerAllEnumsInRam();

    using namespace DbNotRuntime::PhysicalModels;

    // Mismo orden que migrateAllPhysicalModels, pero solo registrando en RAM.
    registerModelInCatalog(CalibracionModel{});
    registerModelInCatalog(LtsModel{});
    registerModelInCatalog(ConfiguracionConectividadModel{});
    registerModelInCatalog(ServidoresNtpModel{});
    registerModelInCatalog(ConfiguracionSmtpModel{});
    registerModelInCatalog(UsuariosModel{});
    registerModelInCatalog(CorreosModel{});
    registerModelInCatalog(AuditoriaModel{});
    registerModelInCatalog(SensorModel{});
    registerModelInCatalog(CensoModel{});
    registerModelInCatalog(EventoSensorModel{});
    registerModelInCatalog(EventoSistemaModel{});

    ESP_LOGI(TAG, "[loader] catálogo en RAM listo (%u tablas)",
             (unsigned)Catalog::PhysicalSchemaCatalog::getInstance().getAllTables().size());

    xSemaphoreGive(loaderDone);
    vTaskDelete(nullptr);
}

// ==================================================
// Helpers DB (solo DQL/DML)
// ==================================================
static int32_t lookupOrCreateSensor(uint8_t slot) {
    if (!gestorSensores.obtenerSensor(slot).estaActivo()) return -1;

    const char* mac  = gestorSensores.obtenerSensor(slot).obtenerMAC();
    const char* mac2 = gestorSensores.obtenerSensor(slot).obtenerMAC2();

    Storage::PgTuple row;
    if (Orm::Sensor.filter_eq("hexadecimal", String(mac)).first(row)) {
        return row.getInt("id_sensor");
    }

    auto& s = gestorSensores.obtenerSensor(slot);
    std::vector<Storage::PgCell> cells = {
        Storage::PgCell("hexadecimal",           String(mac),                     PgDataType::VARCHAR),
        Storage::PgCell("mac2",                  String(mac2),                    PgDataType::VARCHAR),
        Storage::PgCell("nombre",                String("Sensor ") + String(slot),PgDataType::VARCHAR),
        Storage::PgCell("descripcion",           "",                               PgDataType::TEXT),
        Storage::PgCell("ubicacion",             "Sin asignar",                   PgDataType::VARCHAR),
        Storage::PgCell("offset_calibracion",    String(s.obtenerOffset(), 2),     PgDataType::REAL),
        Storage::PgCell("umbral_superior",       String(s.obtenerUmbralSuperior(), 2), PgDataType::REAL),
        Storage::PgCell("umbral_inferior",       String(s.obtenerUmbralInferior(), 2), PgDataType::REAL),
        Storage::PgCell("resolucion_bits",       String((int)s.obtenerResolucionBits()), PgDataType::SMALLINT),
        Storage::PgCell("alimentacion_parasita", s.esAlimentacionParasita() ? "TRUE" : "FALSE", PgDataType::BOOLEAN),
        Storage::PgCell("estado_actual",         String((int)s.obtenerEstado()),  PgDataType::SMALLINT),
        Storage::PgCell("activa",                "TRUE",                           PgDataType::BOOLEAN),
        Storage::PgCell("fecha_creacion",        String((long)(millis() / 1000)),  PgDataType::BIGINT),
        Storage::PgCell("fecha_modificacion",    String((long)(millis() / 1000)),  PgDataType::BIGINT),
    };

    auto r = Orm::Sensor.create(cells, {"id_sensor"});
    if (r.success && !r.returningRows.empty()) {
        return r.returningRows[0].getInt("id_sensor");
    }
    ESP_LOGE(TAG, "No se pudo crear Sensor para %s", mac);
    return -1;
}

static void insertCenso(uint8_t slot, float tempCruda, float tempCorregida, int16_t estado) {
    int32_t idSensor = sensorCache[slot].idSensor;
    if (idSensor < 0) return;

    std::vector<Storage::PgCell> cells = {
        Storage::PgCell("id_sensor",             String(idSensor),                PgDataType::INTEGER),
        Storage::PgCell("temperatura_cruda",     String(tempCruda, 2),            PgDataType::REAL),
        Storage::PgCell("temperatura_corregida", String(tempCorregida, 2),        PgDataType::REAL),
        Storage::PgCell("estado_sensor",         String((int)estado),             PgDataType::SMALLINT),
        Storage::PgCell("activa",                "TRUE",                          PgDataType::BOOLEAN),
        Storage::PgCell("fecha_creacion",        String((long)(millis() / 1000)), PgDataType::BIGINT),
    };

    auto r = Orm::Censo.create(cells);
    if (!r.success) ESP_LOGW(TAG, "Fallo insert Censo (slot %u)", slot);
}

// ==================================================
// Tarea sensores (solo DQL/DML)
// ==================================================
static void sensorTask(void* /*param*/) {
    uint8_t n = gestorSensores.escanearYAsignarSensores(12, false);
    ESP_LOGI(TAG, "Sensores detectados: %u", n);

    xSemaphoreTake(dbMutex, portMAX_DELAY);
    for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
        sensorCache[i].idSensor = lookupOrCreateSensor(i);
        if (sensorCache[i].idSensor >= 0) {
            strncpy(sensorCache[i].mac, gestorSensores.obtenerSensor(i).obtenerMAC(), 16);
            sensorCache[i].mac[16] = '\0';
            ESP_LOGI(TAG, "slot %u → MAC=%s id_sensor=%ld",
                     i, sensorCache[i].mac, (long)sensorCache[i].idSensor);
        }
    }
    xSemaphoreGive(dbMutex);

    uint32_t lastHotCheck = 0;
    while (true) {
        gestorSensores.solicitarConversionGlobal(false);

        uint32_t t0 = millis();
        while (!gestorSensores.estaConversionCompleta() && (millis() - t0) < 2000) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
            if (!gestorSensores.obtenerSensor(i).estaActivo()) continue;

            EstadoSensor estado = gestorSensores.actualizarLecturaSlot(i);
            float tempCruda     = gestorSensores.obtenerSensor(i).obtenerTempCelsius();
            float tempCorregida = tempCruda;
            int16_t estadoInt   = static_cast<int16_t>(estado);

            xSemaphoreTake(lecturasMutex, portMAX_DELAY);
            lecturas[i].activo    = true;
            strncpy(lecturas[i].mac, gestorSensores.obtenerSensor(i).obtenerMAC(), 16);
            lecturas[i].mac[16]   = '\0';
            lecturas[i].tempC     = tempCorregida;
            lecturas[i].estado    = estadoInt;
            lecturas[i].timestamp = millis();
            xSemaphoreGive(lecturasMutex);

            xSemaphoreTake(dbMutex, portMAX_DELAY);
            insertCenso(i, tempCruda, tempCorregida, estadoInt);
            xSemaphoreGive(dbMutex);
        }

        if (millis() - lastHotCheck > 10000) {
            lastHotCheck = millis();
            if (gestorSensores.verificarCambiosEnCaliente()) {
                ESP_LOGI(TAG, "Cambio en caliente → re-sync");
                gestorSensores.escanearYAsignarSensores(12, false);
                xSemaphoreTake(dbMutex, portMAX_DELAY);
                for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
                    sensorCache[i].idSensor = lookupOrCreateSensor(i);
                }
                xSemaphoreGive(dbMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ==================================================
// HTML helpers
// ==================================================
static const char* HTML_HEADER =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>ESP32 HTTPS</title>"
    "<style>"
    "body{font-family:system-ui,Arial,sans-serif;max-width:960px;margin:20px auto;padding:0 16px;}"
    "h1{color:#333;}table{border-collapse:collapse;width:100%;margin-top:10px;}"
    "th,td{border:1px solid #ccc;padding:6px 10px;text-align:left;font-size:14px;}"
    "th{background:#f0f0f0;}.sensor-card{padding:12px;margin:8px 0;border:1px solid #ccc;"
    "border-radius:8px;background:#f9f9f9;}.ok{color:#0a0;font-weight:bold;}"
    ".err{color:#c00;font-weight:bold;}.nav a{margin-right:16px;color:#07c;text-decoration:none;}"
    ".nav a:hover{text-decoration:underline;}button{padding:6px 14px;cursor:pointer;}"
    "</style></head><body>";

static const char* HTML_FOOTER = "</body></html>";

static const char* NAV =
    "<div class='nav'><a href='/'>Dashboard</a><a href='/sensor'>Tabla Sensor</a>"
    "<a href='/censo'>Tabla Censo</a></div><hr/>";

// ==================================================
// Handlers HTTP
// ==================================================
static esp_err_t rootHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, HTML_HEADER, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NAV, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req,
        "<h1>Dashboard de Sensores</h1>"
        "<p>Datos en tiempo real (SSE, se actualizan cada 2 s)</p>"
        "<div id='sensores'>Conectando...</div>"
        "<script>"
        "const evt = new EventSource('/api/stream');"
        "evt.onmessage = e => {"
        "  const d = JSON.parse(e.data);"
        "  let h = '';"
        "  for (const s of d.sensores) {"
        "    const cls = s.estado === 0 ? 'ok' : 'err';"
        "    h += `<div class='sensor-card'>"
        "      <strong>${s.mac}</strong> — ${s.tempC.toFixed(2)} °C "
        "      <span class='${cls}'>[${s.estadoNombre}]</span>"
        "    </div>`;"
        "  }"
        "  document.getElementById('sensores').innerHTML = h || 'Sin sensores activos';"
        "};"
        "evt.onerror = () => { document.getElementById('sensores').innerHTML = 'Desconectado'; };"
        "</script>",
        HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_FOOTER, HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t streamHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    const char* init = ": connected\n\n";
    if (httpd_resp_send_chunk(req, init, strlen(init)) != ESP_OK) return ESP_FAIL;

    char buf[1024];
    while (true) {
        xSemaphoreTake(lecturasMutex, portMAX_DELAY);

        int len = snprintf(buf, sizeof(buf), "data: {\"sensores\":[");
        bool first = true;
        for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
            if (!lecturas[i].activo) continue;
            if (!first) len += snprintf(buf + len, sizeof(buf) - len, ",");
            first = false;
            len += snprintf(buf + len, sizeof(buf) - len,
                "{\"mac\":\"%s\",\"tempC\":%.2f,\"estado\":%d,\"estadoNombre\":\"%s\"}",
                lecturas[i].mac,
                lecturas[i].tempC,
                (int)lecturas[i].estado,
                GestorSensores::nombreEstadoSensor((EstadoSensor)lecturas[i].estado));
        }
        len += snprintf(buf + len, sizeof(buf) - len, "]}\n\n");

        xSemaphoreGive(lecturasMutex);

        if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t sensorTableHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, HTML_HEADER, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NAV, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, "<h1>Tabla Sensor</h1>", HTTPD_RESP_USE_STRLEN);

    std::vector<Storage::PgTuple> rows;
    xSemaphoreTake(dbMutex, portMAX_DELAY);
    rows = Orm::Sensor.objects().all();
    xSemaphoreGive(dbMutex);

    char buf[256];
    snprintf(buf, sizeof(buf), "<p>Total: %u sensores</p>", (unsigned)rows.size());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    httpd_resp_send_chunk(req,
        "<table><tr>"
        "<th>ID</th><th>Hexadecimal</th><th>MAC2</th><th>Nombre</th>"
        "<th>Ubicación</th><th>Estado</th><th>Activa</th>"
        "</tr>",
        HTTPD_RESP_USE_STRLEN);

    for (const auto& r : rows) {
        snprintf(buf, sizeof(buf),
            "<tr><td>%ld</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%ld</td><td>%s</td></tr>",
            (long)r.getInt("id_sensor"),
            r.getText("hexadecimal").c_str(),
            r.getText("mac2").c_str(),
            r.getText("nombre").c_str(),
            r.getText("ubicacion").c_str(),
            (long)r.getInt("estado_actual"),
            r.getBool("activa") ? "Sí" : "No");
        httpd_resp_send_chunk(req, buf, strlen(buf));
    }

    httpd_resp_send_chunk(req, "</table>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_FOOTER, HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t censoTableHandler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send_chunk(req, HTML_HEADER, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, NAV, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req,
        "<h1>Tabla Censo</h1>"
        "<form method='POST' action='/censo/clear' style='margin-bottom:10px;'>"
        "<button type='submit' onclick=\"return confirm('¿Borrar todos los registros?');\">Borrar todo</button>"
        "</form>",
        HTTPD_RESP_USE_STRLEN);

    static const size_t MAX_ROWS = 50;
    std::vector<Storage::PgTuple> toRender;
    size_t total = 0;
    xSemaphoreTake(dbMutex, portMAX_DELAY);
    {
        auto& all = Storage::RuntimeStorageAndIndexManager::getInstance().getTableRows("Censo");
        total = all.size();
        size_t start = (total > MAX_ROWS) ? total - MAX_ROWS : 0;
        toRender.reserve(total - start);
        for (size_t i = start; i < total; i++) toRender.push_back(all[i]);
    }
    xSemaphoreGive(dbMutex);

    char buf[256];
    snprintf(buf, sizeof(buf), "<p>Total: %u registros (mostrando %u)</p>",
             (unsigned)total, (unsigned)toRender.size());
    httpd_resp_send_chunk(req, buf, strlen(buf));

    httpd_resp_send_chunk(req,
        "<table><tr>"
        "<th>ID</th><th>ID Sensor</th><th>Temp Cruda</th><th>Temp Corregida</th>"
        "<th>Estado</th><th>Fecha (s)</th>"
        "</tr>",
        HTTPD_RESP_USE_STRLEN);

    for (const auto& r : toRender) {
        snprintf(buf, sizeof(buf),
            "<tr><td>%ld</td><td>%ld</td><td>%.2f</td><td>%.2f</td><td>%ld</td><td>%ld</td></tr>",
            (long)r.getInt("id_censo"),
            (long)r.getInt("id_sensor"),
            r.getFloat("temperatura_cruda"),
            r.getFloat("temperatura_corregida"),
            (long)r.getInt("estado_sensor"),
            (long)r.getBigInt("fecha_creacion"));
        httpd_resp_send_chunk(req, buf, strlen(buf));
    }

    httpd_resp_send_chunk(req, "</table>", HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, HTML_FOOTER, HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t clearCensoHandler(httpd_req_t *req) {
    xSemaphoreTake(dbMutex, portMAX_DELAY);
    auto r = Orm::Censo.truncate();
    xSemaphoreGive(dbMutex);

    if (!r.success) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                    "No se pudo limpiar la tabla Censo");
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/censo");
    return httpd_resp_send(req, NULL, 0);
}

// ==================================================
// Tabla de URIs
// ==================================================
static const httpd_uri_t uri_root        = { .uri = "/",            .method = HTTP_GET,  .handler = rootHandler };
static const httpd_uri_t uri_stream      = { .uri = "/api/stream",  .method = HTTP_GET,  .handler = streamHandler };
static const httpd_uri_t uri_sensor      = { .uri = "/sensor",      .method = HTTP_GET,  .handler = sensorTableHandler };
static const httpd_uri_t uri_censo       = { .uri = "/censo",       .method = HTTP_GET,  .handler = censoTableHandler };
static const httpd_uri_t uri_clear_censo = { .uri = "/censo/clear", .method = HTTP_POST, .handler = clearCensoHandler };

// ==================================================
// SETUP
// ==================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n[boot] ESP32 HTTPS + Sensores (solo DQL/DML)");

    // 1. WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.printf("\n✅ WiFi: https://%s\n", WiFi.localIP().toString().c_str());

    // 2. Diagnóstico de memoria ANTES del catálogo
    Serial.printf("[mem] pre-loader: free=%u max_alloc=%u\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());

    // 3. Catálogo en RAM SOLO con Sensor y Censo
    loaderDone = xSemaphoreCreateBinary();
    xTaskCreate(loaderTask, "loaderTask", 16384, nullptr, 5, nullptr);
    if (xSemaphoreTake(loaderDone, pdMS_TO_TICKS(60000)) != pdTRUE) {
        Serial.println("❌ Loader timeout");
        while (true) delay(1000);
    }
    Serial.println("✅ Catálogo en RAM listo");

    Serial.printf("[mem] post-loader: free=%u max_alloc=%u\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());

    // 4. Cargar .tbl de la SD (solo Sensor y Censo)
    if (!Storage::RuntimeStorageAndIndexManager::getInstance().synchronizeWithDdlCatalog()) {
        Serial.println("❌ synchronizeWithDdlCatalog falló");
        while (true) delay(1000);
    }
    Serial.println("✅ Storage sincronizado");

    Serial.printf("[mem] post-storage: free=%u max_alloc=%u\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());

    // 5. Certificado
    if (!generateSelfSignedCert()) {
        Serial.println("❌ Error generando certificado");
        while (true) delay(1000);
    }

    // 6. Mutexes + tarea de sensores
    dbMutex       = xSemaphoreCreateMutex();
    lecturasMutex = xSemaphoreCreateMutex();
    xTaskCreate(sensorTask, "sensorTask", 16384, nullptr, 5, nullptr);

    // 7. Servidor HTTPS con sockets reducidos
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.httpd.server_port      = 443;
    config.httpd.max_uri_handlers = 8;
    config.httpd.max_open_sockets = 2;      // ← CRÍTICO
    config.httpd.stack_size       = 12288;

    config.servercert     = (const uint8_t*)cert_pem_buf;
    config.servercert_len = strlen(cert_pem_buf) + 1;
    config.prvtkey_pem    = (const uint8_t*)key_pem_buf;
    config.prvtkey_len    = strlen(key_pem_buf) + 1;

    Serial.printf("[mem] pre-https: free=%u max_alloc=%u\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());

    esp_err_t err = httpd_ssl_start(&server, &config);
    if (err != ESP_OK) {
        Serial.printf("❌ httpd_ssl_start: %s\n", esp_err_to_name(err));
        while (true) delay(1000);
    }

    httpd_register_uri_handler(server, &uri_root);
    httpd_register_uri_handler(server, &uri_stream);
    httpd_register_uri_handler(server, &uri_sensor);
    httpd_register_uri_handler(server, &uri_censo);
    httpd_register_uri_handler(server, &uri_clear_censo);

    Serial.printf("[mem] post-https: free=%u max_alloc=%u\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());
    Serial.println("✅ Servidor HTTPS en :443");
}

void loop() {
    delay(1000);
}