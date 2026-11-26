/*
 * BENCHMARK — JSON vs MessagePack vs CBOR vs TLV Binario
 *
 * Correcciones:
 *   - RAM medida por PICO REAL (antes daba 0 porque se medía post-destrucción).
 *   - Barrera de compilador para que no se optimice la deserialización.
 *   - Tabla absoluta + tabla con porcentajes relativos a JSON.
 *   - serialize_cbor con la API de streaming de TinyCBOR.
 *   - getCpuFrequencyMhz() con %lu.
 *   - char sensor[16] para evitar punteros colgantes.
 *   - Buffer TLV ampliado a 64 bytes.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cbor.h>
#include <cstring>

// ═══════════════════════════════════════════════════════════
// DATOS DE PRUEBA
// ═══════════════════════════════════════════════════════════
struct Telemetry {
  float     temp;
  float     hum;
  uint16_t  co2;
  uint8_t   id;
  uint32_t  timestamp;
  bool      ok;
  char      sensor[16];
};

Telemetry data = {23.5f, 65.2f, 412, 1, 1699999999, true, "Temperature"};

// ═══════════════════════════════════════════════════════════
// BUFFERS
// ═══════════════════════════════════════════════════════════
uint8_t buf_json[256];
uint8_t buf_msgpack[128];
uint8_t buf_cbor[128];
uint8_t buf_tlv[64];

// ═══════════════════════════════════════════════════════════
// RESULTADOS
// ═══════════════════════════════════════════════════════════
struct Result {
  const char* name;
  size_t      size;
  uint32_t    t_serialize;
  uint32_t    t_deserialize;
  size_t      ram_peak;    // ← era ram_used (siempre 0); ahora pico real
  uint32_t    t_uart_ms;
};

Result results[4];

// ═══════════════════════════════════════════════════════════
// SEGUIMIENTO DEL PICO DE RAM
// ═══════════════════════════════════════════════════════════
// El tracking solo se activa durante una pasada específica (1 llamada)
// para no añadir overhead a las pasadas de medición de tiempo.
static size_t g_peak_ram_ser   = 0;
static size_t g_peak_ram_deser = 0;
static bool   g_track_ram      = false;

// ═══════════════════════════════════════════════════════════
// SERIALIZERS
// ═══════════════════════════════════════════════════════════

size_t serialize_json(uint8_t* buf, size_t buf_size) {
  size_t h_before = g_track_ram ? ESP.getFreeHeap() : 0;
  JsonDocument doc;
  doc["temp"]   = data.temp;
  doc["hum"]    = data.hum;
  doc["co2"]    = data.co2;
  doc["id"]     = data.id;
  doc["ts"]     = data.timestamp;
  doc["ok"]     = data.ok;
  doc["sensor"] = data.sensor;
  if (g_track_ram) {
    size_t used = h_before - ESP.getFreeHeap();
    if (used > g_peak_ram_ser) g_peak_ram_ser = used;
  }
  return serializeJson(doc, buf, buf_size);
}

size_t serialize_msgpack(uint8_t* buf, size_t buf_size) {
  size_t h_before = g_track_ram ? ESP.getFreeHeap() : 0;
  JsonDocument doc;
  doc["temp"]   = data.temp;
  doc["hum"]    = data.hum;
  doc["co2"]    = data.co2;
  doc["id"]     = data.id;
  doc["ts"]     = data.timestamp;
  doc["ok"]     = data.ok;
  doc["sensor"] = data.sensor;
  if (g_track_ram) {
    size_t used = h_before - ESP.getFreeHeap();
    if (used > g_peak_ram_ser) g_peak_ram_ser = used;
  }
  return serializeMsgPack(doc, buf, buf_size);
}

// TinyCBOR no asigna heap: escribe directo al buffer.
// Su pico de RAM = 0.
size_t serialize_cbor(uint8_t* buf, size_t buf_size) {
  CborEncoder encoder;
  cbor_encoder_init(&encoder, buf, buf_size, 0);

  CborEncoder map;
  CborError err = cbor_encoder_create_map(&encoder, &map, 7);
  if (err != CborNoError) return 0;

  cbor_encode_text_stringz(&map, "temp");  cbor_encode_float(&map, data.temp);
  cbor_encode_text_stringz(&map, "hum");   cbor_encode_float(&map, data.hum);
  cbor_encode_text_stringz(&map, "co2");   cbor_encode_uint(&map, data.co2);
  cbor_encode_text_stringz(&map, "id");    cbor_encode_uint(&map, data.id);
  cbor_encode_text_stringz(&map, "ts");    cbor_encode_uint(&map, data.timestamp);
  cbor_encode_text_stringz(&map, "ok");    cbor_encode_boolean(&map, data.ok);
  cbor_encode_text_stringz(&map, "sensor");cbor_encode_text_stringz(&map, data.sensor);

  err = cbor_encoder_close_container(&encoder, &map);
  if (err != CborNoError) return 0;

  return cbor_encoder_get_buffer_size(&encoder, buf);
}

// TLV tampoco asigna heap: escribe directo. Pico = 0.
size_t serialize_tlv(uint8_t* buf, size_t buf_size) {
  int i = 0;
  buf[i++] = 0x01; memcpy(&buf[i], &data.temp, 4);        i += 4;
  buf[i++] = 0x02; memcpy(&buf[i], &data.hum, 4);         i += 4;
  buf[i++] = 0x03; buf[i++] = data.co2 >> 8; buf[i++] = data.co2 & 0xFF;
  buf[i++] = 0x04; buf[i++] = data.id;
  buf[i++] = 0x05; memcpy(&buf[i], &data.timestamp, 4);   i += 4;
  buf[i++] = 0x06; buf[i++] = data.ok ? 1 : 0;
  buf[i++] = 0x07; buf[i++] = strlen(data.sensor);
  memcpy(&buf[i], data.sensor, strlen(data.sensor));      i += strlen(data.sensor);
  return i;
}

// ═══════════════════════════════════════════════════════════
// DESERIALIZERS
// ═══════════════════════════════════════════════════════════

Telemetry deserialize_json(uint8_t* buf, size_t len) {
  size_t h_before = g_track_ram ? ESP.getFreeHeap() : 0;
  Telemetry t = {};
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, buf, len);
  if (g_track_ram) {
    size_t used = h_before - ESP.getFreeHeap();
    if (used > g_peak_ram_deser) g_peak_ram_deser = used;
  }
  if (err) return t;

  t.temp = doc["temp"]; t.hum = doc["hum"]; t.co2 = doc["co2"];
  t.id = doc["id"]; t.timestamp = doc["ts"]; t.ok = doc["ok"];
  const char* s = doc["sensor"];
  if (s) strncpy(t.sensor, s, sizeof(t.sensor) - 1);
  return t;
}

Telemetry deserialize_msgpack(uint8_t* buf, size_t len) {
  size_t h_before = g_track_ram ? ESP.getFreeHeap() : 0;
  Telemetry t = {};
  JsonDocument doc;
  DeserializationError err = deserializeMsgPack(doc, buf, len);
  if (g_track_ram) {
    size_t used = h_before - ESP.getFreeHeap();
    if (used > g_peak_ram_deser) g_peak_ram_deser = used;
  }
  if (err) return t;

  t.temp = doc["temp"]; t.hum = doc["hum"]; t.co2 = doc["co2"];
  t.id = doc["id"]; t.timestamp = doc["ts"]; t.ok = doc["ok"];
  const char* s = doc["sensor"];
  if (s) strncpy(t.sensor, s, sizeof(t.sensor) - 1);
  return t;
}

// TinyCBOR parser no asigna heap. Pico = 0.
Telemetry deserialize_cbor(uint8_t* buf, size_t len) {
  Telemetry t = {};
  CborParser parser; CborValue it;
  CborError err = cbor_parser_init(buf, len, 0, &parser, &it);
  if (err != CborNoError || !cbor_value_is_map(&it)) return t;

  CborValue map;
  err = cbor_value_enter_container(&it, &map);
  if (err != CborNoError) return t;

  char key[16];
  while (!cbor_value_at_end(&map)) {
    if (!cbor_value_is_text_string(&map)) break;
    size_t key_len = sizeof(key);
    err = cbor_value_copy_text_string(&map, key, &key_len, &map);
    if (err != CborNoError) break;

    if (strcmp(key, "temp") == 0 && cbor_value_is_float(&map)) {
      cbor_value_get_float(&map, &t.temp);
    } else if (strcmp(key, "hum") == 0 && cbor_value_is_float(&map)) {
      cbor_value_get_float(&map, &t.hum);
    } else if (strcmp(key, "co2") == 0 && cbor_value_is_unsigned_integer(&map)) {
      uint64_t v = 0; cbor_value_get_uint64(&map, &v); t.co2 = (uint16_t)v;
    } else if (strcmp(key, "id") == 0 && cbor_value_is_unsigned_integer(&map)) {
      uint64_t v = 0; cbor_value_get_uint64(&map, &v); t.id = (uint8_t)v;
    } else if (strcmp(key, "ts") == 0 && cbor_value_is_unsigned_integer(&map)) {
      uint64_t v = 0; cbor_value_get_uint64(&map, &v); t.timestamp = (uint32_t)v;
    } else if (strcmp(key, "ok") == 0 && cbor_value_is_boolean(&map)) {
      bool v = false; cbor_value_get_boolean(&map, &v); t.ok = v;
    } else if (strcmp(key, "sensor") == 0 && cbor_value_is_text_string(&map)) {
      size_t slen = sizeof(t.sensor);
      cbor_value_copy_text_string(&map, t.sensor, &slen, NULL);
    }
    err = cbor_value_advance(&map);
    if (err != CborNoError) break;
  }
  return t;
}

// TLV no asigna heap. Pico = 0.
Telemetry deserialize_tlv(uint8_t* buf, size_t len) {
  Telemetry t = {};
  int i = 0;
  while (i < (int)len) {
    uint8_t tag = buf[i++];
    switch (tag) {
      case 0x01: memcpy(&t.temp, &buf[i], 4); i += 4; break;
      case 0x02: memcpy(&t.hum, &buf[i], 4);  i += 4; break;
      case 0x03: t.co2 = (buf[i] << 8) | buf[i+1]; i += 2; break;
      case 0x04: t.id = buf[i]; i += 1; break;
      case 0x05: memcpy(&t.timestamp, &buf[i], 4); i += 4; break;
      case 0x06: t.ok = buf[i] != 0; i += 1; break;
      case 0x07: {
        int slen = buf[i++];
        if (slen > (int)sizeof(t.sensor) - 1) slen = sizeof(t.sensor) - 1;
        memcpy(t.sensor, &buf[i], slen); t.sensor[slen] = '\0'; i += slen;
        break;
      }
    }
  }
  return t;
}

// ═══════════════════════════════════════════════════════════
// BENCHMARK ENGINE
// ═══════════════════════════════════════════════════════════
#define BENCH_ITERS 100

void run_bench(const char* name,
               size_t (*serialize)(uint8_t*, size_t),
               Telemetry (*deserialize)(uint8_t*, size_t),
               uint8_t* buf, size_t buf_size,
               int idx) {
  size_t size = 0;

  // ─── 1) Timing de serialización (sin tracking de RAM) ──
  g_track_ram = false;
  uint32_t t0 = micros();
  for (int i = 0; i < BENCH_ITERS; i++) {
    size = serialize(buf, buf_size);
    __asm__ __volatile__("" : : "r"(size) : "memory");
  }
  uint32_t t1 = micros();
  uint32_t t_serialize = (t1 - t0) / BENCH_ITERS;

  // ─── 2) Timing de deserialización (sin tracking) ───────
  uint32_t t2 = micros();
  for (int i = 0; i < BENCH_ITERS; i++) {
    Telemetry t = deserialize(buf, size);
    __asm__ __volatile__("" : : "r"(&t) : "memory");
  }
  uint32_t t3 = micros();
  uint32_t t_deserialize = (t3 - t2) / BENCH_ITERS;

  // ─── 3) Pico de RAM (una sola llamada con tracking) ────
  g_peak_ram_ser   = 0;
  g_peak_ram_deser = 0;
  g_track_ram = true;
  serialize(buf, buf_size);
  {
    Telemetry t = deserialize(buf, size);
    __asm__ __volatile__("" : : "r"(&t) : "memory");
  }
  g_track_ram = false;

  size_t ram_peak = (g_peak_ram_ser > g_peak_ram_deser)
                    ? g_peak_ram_ser : g_peak_ram_deser;

  // ─── 4) Tiempo UART a 115200 baud (11520 B/s) ──────────
  uint32_t t_uart_ms = (size * 1000) / 11520;

  results[idx] = {
    .name          = name,
    .size          = size,
    .t_serialize   = t_serialize,
    .t_deserialize = t_deserialize,
    .ram_peak      = ram_peak,
    .t_uart_ms     = t_uart_ms
  };
}

// ═══════════════════════════════════════════════════════════
// TABLAS
// ═══════════════════════════════════════════════════════════

static void print_abs_table() {
  Serial.println("╔═══════════╦════════════╦═══════════╦════════════╦══════════════╦═══════════╗");
  Serial.println("║ Formato   ║ Tamaño (B) ║ Ser (µs)  ║ Deser (µs) ║ RAM pico (B) ║ UART (ms) ║");
  Serial.println("╠═══════════╬════════════╬═══════════╬════════════╬══════════════╬═══════════╣");
  for (int i = 0; i < 4; i++) {
    Serial.printf("║ %-9s ║ %10u ║ %9lu ║ %10lu ║ %12u ║ %9lu ║\n",
                  results[i].name,
                  (unsigned)results[i].size,
                  (unsigned long)results[i].t_serialize,
                  (unsigned long)results[i].t_deserialize,
                  (unsigned)results[i].ram_peak,
                  (unsigned long)results[i].t_uart_ms);
  }
  Serial.println("╚═══════════╩════════════╩═══════════╩════════════╩══════════════╩═══════════╝");
}

static float pct(float v, float base) {
  return (base > 0.0f) ? (v / base * 100.0f) : 0.0f;
}

static void print_rel_table() {
  const Result& b = results[0];  // JSON es la base
  Serial.println("  Comparación relativa a JSON (JSON = 100%):");
  Serial.println("╔═══════════╦════════╦════════╦══════════╦════════════╦════════╗");
  Serial.println("║ Formato   ║ Tamaño ║ Ser    ║ Deser    ║ RAM pico   ║ UART   ║");
  Serial.println("╠═══════════╬════════╬════════╬══════════╬════════════╬════════╣");
  for (int i = 0; i < 4; i++) {
    Serial.printf("║ %-9s ║ %5.0f%% ║ %5.0f%% ║ %7.0f%% ║ %9.0f%% ║ %5.0f%% ║\n",
                  results[i].name,
                  pct((float)results[i].size,          (float)b.size),
                  pct((float)results[i].t_serialize,   (float)b.t_serialize),
                  pct((float)results[i].t_deserialize, (float)b.t_deserialize),
                  pct((float)results[i].ram_peak,      (float)b.ram_peak),
                  pct((float)results[i].t_uart_ms,     (float)b.t_uart_ms));
  }
  Serial.println("╚═══════════╩════════╩════════╩══════════╩════════════╩════════╝");
}

static void print_summary() {
  int best_size = 0, best_ser = 0, best_deser = 0, best_ram = 0;
  for (int i = 1; i < 4; i++) {
    if (results[i].size          < results[best_size].size)          best_size  = i;
    if (results[i].t_serialize   < results[best_ser].t_serialize)    best_ser   = i;
    if (results[i].t_deserialize < results[best_deser].t_deserialize)best_deser = i;
    if (results[i].ram_peak      < results[best_ram].ram_peak)       best_ram   = i;
  }
  Serial.println("\n  Resumen (mejor por métrica):");
  Serial.printf("    Más compacto:       %-9s (%u B)\n",
                results[best_size].name,  (unsigned)results[best_size].size);
  Serial.printf("    Ser más rápida:     %-9s (%lu µs)\n",
                results[best_ser].name,   (unsigned long)results[best_ser].t_serialize);
  Serial.printf("    Deser más rápida:   %-9s (%lu µs)\n",
                results[best_deser].name, (unsigned long)results[best_deser].t_deserialize);
  Serial.printf("    Menor RAM pico:     %-9s (%u B)\n",
                results[best_ram].name,   (unsigned)results[best_ram].ram_peak);
}

// ═══════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("╔══════════════════════════════════════════════════════╗");
  Serial.println("║  BENCHMARK — Serialización en ESP32                 ║");
  Serial.println("╚══════════════════════════════════════════════════════╝");
  Serial.printf("  CPU: %lu MHz\n", (unsigned long)getCpuFrequencyMhz());
  Serial.printf("  Heap libre inicial: %lu bytes\n\n", (unsigned long)ESP.getFreeHeap());

  Serial.println("  Datos de prueba:");
  Serial.printf("    temp=%.2f  hum=%.2f  co2=%u  id=%u\n",
                data.temp, data.hum, (unsigned)data.co2, (unsigned)data.id);
  Serial.printf("    ts=%lu  ok=%d  sensor=\"%s\"\n\n",
                (unsigned long)data.timestamp, data.ok, data.sensor);
  Serial.printf("  Iteraciones por benchmark: %d\n\n", BENCH_ITERS);

  run_bench("JSON",    serialize_json,    deserialize_json,    buf_json,    sizeof(buf_json),    0);
  run_bench("MsgPack", serialize_msgpack, deserialize_msgpack, buf_msgpack, sizeof(buf_msgpack), 1);
  run_bench("CBOR",    serialize_cbor,    deserialize_cbor,    buf_cbor,    sizeof(buf_cbor),    2);
  run_bench("TLV",     serialize_tlv,     deserialize_tlv,     buf_tlv,     sizeof(buf_tlv),     3);

  // ─── Tablas ─────────────────────────────────────────────
  Serial.println("  Valores absolutos:");
  print_abs_table();
  Serial.println();
  print_rel_table();
  print_summary();

  // ─── Verificación de integridad ─────────────────────────
  Serial.println("\n  Verificación (deserializar y comparar):");
  auto verify = [](const char* name,
                   size_t (*ser)(uint8_t*, size_t),
                   Telemetry (*deser)(uint8_t*, size_t),
                   uint8_t* buf, size_t buf_sz) {
    size_t sz = ser(buf, buf_sz);
    Telemetry t = deser(buf, sz);
    Serial.printf("    %-9s: temp=%.2f hum=%.2f co2=%u id=%u ts=%lu ok=%d sensor=%s\n",
                  name, t.temp, t.hum, (unsigned)t.co2, (unsigned)t.id,
                  (unsigned long)t.timestamp, t.ok, t.sensor);
  };
  verify("JSON",    serialize_json,    deserialize_json,    buf_json,    sizeof(buf_json));
  verify("MsgPack", serialize_msgpack, deserialize_msgpack, buf_msgpack, sizeof(buf_msgpack));
  verify("CBOR",    serialize_cbor,    deserialize_cbor,    buf_cbor,    sizeof(buf_cbor));
  verify("TLV",     serialize_tlv,     deserialize_tlv,     buf_tlv,     sizeof(buf_tlv));

  // ─── Dump hex ───────────────────────────────────────────
  Serial.println("\n  Dump hex de cada formato:");
  auto dump = [](const char* name,
                 size_t (*ser)(uint8_t*, size_t),
                 uint8_t* buf, size_t buf_sz) {
    size_t sz = ser(buf, buf_sz);
    Serial.printf("  %-9s (%u B): ", name, (unsigned)sz);
    for (int i = 0; i < sz && i < 64; i++) Serial.printf("%02X ", buf[i]);
    if (sz > 64) Serial.print("...");
    Serial.println();
  };
  dump("JSON",    serialize_json,    buf_json,    sizeof(buf_json));
  dump("MsgPack", serialize_msgpack, buf_msgpack, sizeof(buf_msgpack));
  dump("CBOR",    serialize_cbor,    buf_cbor,    sizeof(buf_cbor));
  dump("TLV",     serialize_tlv,     buf_tlv,     sizeof(buf_tlv));

  Serial.println("\n  [FIN]");
}

void loop() {
  delay(10000);
}