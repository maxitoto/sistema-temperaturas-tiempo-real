# Guía Maestra 3: Implementación Completa Lista para Aplicar (Puntos 1, 2 y 3)

En este documento tienes todos los bloques de código rectificados para solucionar los 3 puntos planteados antes de la migración a ESP32-S:
1. **Solución a `USUARIO_O_PIN_INVALIDO`** y reemplazo de la tabla `conexion_firmware` de Electron por un archivo JSON ligero (`conexion_firmware.json`).
2. **Fusión de los esquemas de Base de Datos en el Firmware** (`sensor_fw` absorbe `nombre`, `descripcion`, `ubicacion`, `ubicacion_descripcion`, renombra `correcion_superior` y `correcion_inferior`, y suma `altura` y `presion_atmosferica`; `configuracion_conectividad_fw` suma `puerto_tcp` y `baudios_usb`).
3. **Actualización de toda la traza RPC (Frontend $\leftrightarrow$ Electron $\leftrightarrow$ Firmware)** incluyendo el envío en vivo de las direcciones IP `ipSta` e `ipAp` asignadas al ESP32.

---

## PARTE 1: Archivos del Firmware (ESP32)

### 1.1. `src/database/EsquemaFisico.h` (Esquema Físico Fusionado y Alineado sin Trampas de Padding)

Reemplaza `src/database/EsquemaFisico.h` por esta versión. Nota cómo todas las estructuras usan la regla $4\text{B} \rightarrow 2\text{B} \rightarrow 1\text{B}$ para que los `float` y `uint32_t` estén siempre alineados en múltiplos de $4$:

```cpp
// Archivo: ./src/database/EsquemaFisico.h
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace EsquemaFisicoDB {

    enum EstadoSensor : uint8_t {
        ESTADO_OK               = 0,
        ESTADO_DESCONEXION_WIRE = 1,
        ESTADO_RUIDO            = 2,
        ESTADO_SUPERO_INFERIOR  = 3,
        ESTADO_SUPERO_SUPERIOR  = 4
    };

    enum CriticidadEvento : uint8_t {
        CRITICIDAD_BAJA    = 0,
        CRITICIDAD_MEDIA   = 1,
        CRITICIDAD_CRITICA = 2
    };

    enum TipoRecepcionCorreo : uint8_t {
        RECIBIR_TODOS            = 0,
        RECIBIR_EVENTOS          = 1,
        RECIBIR_EVENTOS_CRITICOS = 2
    };

    enum EstadoEnvioCorreo : uint8_t {
        ENVIO_PENDIENTE = 0,
        ENVIO_ENVIADO   = 1,
        ENVIO_FALLIDO   = 2
    };

    enum RolUsuario : uint8_t {
        ROL_REGULAR = 0,
        ROL_ADMIN   = 1
    };

    // =========================================================================
    // 1. TABLA FUSIONADA: sensor (244 bytes - 0 bytes de padding manual)
    // =========================================================================
    struct FilaSensor {
        // Grupo 4B (32 bytes: Offsets 0..31)
        float    correcion_superior;      // Offset 0  (4B)
        float    correcion_inferior;      // Offset 4  (4B)
        float    altura;                  // Offset 8  (4B)
        float    presion_atmosferica;     // Offset 12 (4B)
        float    umbral_superior;         // Offset 16 (4B)
        float    umbral_inferior;         // Offset 20 (4B)
        uint32_t fecha_creacion;          // Offset 24 (4B)
        uint32_t fecha_modificacion;      // Offset 28 (4B)
        // Grupo 2B (2 bytes: Offset 32..33)
        uint16_t id_sensor;               // Offset 32 (2B)
        // Grupo 1B / char[] (210 bytes: Offsets 34..243)
        uint8_t  activa;                  // Offset 34 (1B)
        char     hexadecimal[17];         // Offset 35 (17B) -> 32+2+1+17 = 52
        char     nombre[32];              // Offset 52 (32B)
        char     descripcion[64];         // Offset 84 (64B)
        char     ubicacion[32];           // Offset 148 (32B)
        char     ubicacion_descripcion[64]; // Offset 180 (64B) -> Total = 244B
    };
    static_assert(sizeof(FilaSensor) == 244, "FilaSensor debe medir exactamente 244 bytes");

    // =========================================================================
    // 2. TABLA: senso (28 bytes)
    // =========================================================================
    struct FilaSenso {
        uint32_t id_senso;
        uint16_t id_sensor;
        uint8_t  _pad0[2];
        float    temperatura_cruda;
        float    temperatura_corregida;
        uint8_t  estado_sensor;
        uint8_t  activa;
        uint8_t  _pad1[2];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaSenso) == 28, "FilaSenso debe medir exactamente 28 bytes");

    // =========================================================================
    // 3. TABLA: evento_sensor (24 bytes)
    // =========================================================================
    struct FilaEventoSensor {
        uint32_t id_evento_sensor;
        uint16_t id_sensor;
        uint8_t  _pad0[2];
        uint32_t id_senso;
        uint8_t  estado_previo_sensor;
        uint8_t  evento;
        uint8_t  criticidad;
        uint8_t  activa;
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaEventoSensor) == 24, "FilaEventoSensor debe medir exactamente 24 bytes");

    // =========================================================================
    // 4. TABLA: auditoria (64 bytes)
    // =========================================================================
    struct FilaAuditoria {
        uint32_t id_auditoria;
        uint16_t id_usuario;
        char     accion[24];
        char     detalle[24];
        uint8_t  activa;
        uint8_t  _pad0[1];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaAuditoria) == 64, "FilaAuditoria debe medir exactamente 64 bytes");

    // =========================================================================
    // 5. TABLA: correos (112 bytes)
    // =========================================================================
    struct FilaCorreo {
        uint16_t id_correo;
        char     nombre[24];
        char     apellido[24];
        char     correo_electronico[48];
        uint8_t  tipo_recepcion_mensajes;
        uint8_t  estado_envio;
        uint8_t  activa;
        uint8_t  _pad0[3];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaCorreo) == 112, "FilaCorreo debe medir exactamente 112 bytes");

    // =========================================================================
    // 6. TABLA: usuarios (64 bytes)
    // =========================================================================
    struct FilaUsuario {
        uint16_t id_usuario;
        char     usuario[20];
        uint8_t  contrasena[32];
        uint8_t  rol;
        uint8_t  activa;
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaUsuario) == 64, "FilaUsuario debe medir exactamente 64 bytes");

    // =========================================================================
    // 7. TABLA SINGLETON (PK=1): configuracion_smtp (192 bytes)
    // =========================================================================
    struct FilaConfiguracionSMTP {
        uint8_t  id_configuracion_smtp;
        char     host_smtp[40];
        uint8_t  _pad0[1];
        uint16_t puerto;
        char     usuario[40];
        char     password[48];
        char     email_remitente[48];
        uint8_t  activa;
        uint8_t  _pad1[3];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaConfiguracionSMTP) == 192, "FilaConfiguracionSMTP debe medir 192 bytes");

    // =========================================================================
    // 8. TABLA: servidores_ntp (52 bytes)
    // =========================================================================
    struct FilaServidorNTP {
        uint8_t  id_server;
        char     server[40];
        uint8_t  activa;
        uint8_t  _pad0[2];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaServidorNTP) == 52, "FilaServidorNTP debe medir 52 bytes");

    // =========================================================================
    // 9. TABLA SINGLETON (PK=1): lts (208 bytes)
    // =========================================================================
    struct FilaLTS {
        uint8_t  id_lts;
        char     nombre_certificado[24];
        char     certificado[64];
        uint8_t  clave_privada[32];
        char     cadena_certificados[64];
        uint8_t  _pad0[3];
        uint32_t fecha_inicio_vigencia;
        uint32_t fecha_fin_vigencia;
        uint8_t  activa;
        uint8_t  _pad1[3];
        uint32_t fecha_creacion;
        uint32_t fecha_modificacion;
    };
    static_assert(sizeof(FilaLTS) == 208, "FilaLTS debe medir 208 bytes");

    // =========================================================================
    // 10. TABLA SINGLETON (PK=1): configuracion_conectividad (216 bytes)
    // =========================================================================
    struct FilaConfiguracionConectividad {
        // Grupo 4B (12 bytes: Offsets 0..11)
        uint32_t fecha_creacion;                // Offset 0  (4B)
        uint32_t fecha_modificacion;            // Offset 4  (4B)
        uint32_t baudios_usb;                   // Offset 8  (4B)
        // Grupo 2B (4 bytes: Offsets 12..15)
        uint16_t puerto_tcp;                    // Offset 12 (2B)
        uint16_t puerto_udp;                    // Offset 14 (2B)
        // Grupo 1B / char[] (198 bytes: Offsets 16..213)
        uint8_t  id_configuracion_conectividad; // Offset 16 (1B)
        uint8_t  activa;                        // Offset 17 (1B)
        char     SSID_STA[33];                  // Offset 18 (33B)
        char     STA_password[65];              // Offset 51 (65B)
        char     SSID_AP[33];                   // Offset 116 (33B)
        char     AP_password[65];               // Offset 149 (65B)
        uint8_t  _padFinal[2];                  // Offset 214 (2B) -> Total = 216B
    };
    static_assert(sizeof(FilaConfiguracionConectividad) == 216, "FilaConfiguracionConectividad debe medir 216 bytes");

}
```

---

### 1.2. Exposición del Módem y Puertos TCP/UDP en `Modem.h` y `ServidorWiFi.h`

En `src/communication/conectividad/modem/ServidorWiFi.h`, permite configurar `puertoTcp` y `puertoUdp` de forma independiente agregando este método sobrecargado:
```cpp
    void cambiarPuertos(int nuevoPuertoTcp, int nuevoPuertoUdp) {
        if (nuevoPuertoTcp <= 0 || nuevoPuertoTcp > 65534) return;
        if (nuevoPuertoUdp <= 0 || nuevoPuertoUdp > 65534) return;
        if (nuevoPuertoTcp == puertoTcp && nuevoPuertoUdp == puertoUdp) return;

        bool estabaActivo = servidorActivo;
        if (estabaActivo) detener();
        puertoTcp = nuevoPuertoTcp;
        puertoUdp = nuevoPuertoUdp;
        servidor = WiFiServer(puertoTcp);
        if (estabaActivo) iniciar();
    }
```

Y al final de `src/communication/conectividad/modem/Modem.h`, agrega los *getters* separados de puerto TCP/UDP y la declaración `extern Modem modem;`:
```cpp
    uint16_t obtenerPuertoTCP() const {
        return tcp ? (uint16_t)tcp->obtenerPuerto() : 80;
    }

    uint16_t obtenerPuertoUDP() const {
        return tcp ? (uint16_t)tcp->obtenerPuertoUDP() : 81;
    }
};

extern Modem modem;
```

---

### 1.3. `src/database/repositories/FirmwareRepositories.h` (Repositorio de Sensores Desacoplado)

En `SensorFirmwareRepository`, reemplaza `guardarSensor` para que reciba la estructura/DTO completo y soporte todos los nuevos campos sin explotar la lista de argumentos:

```cpp
        bool guardarSensor(const FilaSensor& datosEntrada,
                           uint32_t ahoraSeg,
                           FilaSensor& outGuardado,
                           const ContextoSesionSGBD& sesion) {
            if (!orm || datosEntrada.hexadecimal[0] == '\0') return false;
            FilaSensor existente = {};
            if (orm->buscarSensorPorHex(datosEntrada.hexadecimal, existente, sesion)) {
                strncpy(existente.nombre, datosEntrada.nombre, sizeof(existente.nombre) - 1);
                strncpy(existente.descripcion, datosEntrada.descripcion, sizeof(existente.descripcion) - 1);
                strncpy(existente.ubicacion, datosEntrada.ubicacion, sizeof(existente.ubicacion) - 1);
                strncpy(existente.ubicacion_descripcion, datosEntrada.ubicacion_descripcion, sizeof(existente.ubicacion_descripcion) - 1);
                existente.correcion_superior  = datosEntrada.correcion_superior;
                existente.correcion_inferior  = datosEntrada.correcion_inferior;
                existente.altura              = datosEntrada.altura;
                existente.presion_atmosferica = datosEntrada.presion_atmosferica;
                existente.umbral_superior     = datosEntrada.umbral_superior;
                existente.umbral_inferior     = datosEntrada.umbral_inferior;
                if (!orm->sensor.actualizarPorId(existente.id_sensor, existente, ahoraSeg, sesion)) return false;
                outGuardado = existente;
                return true;
            }
            FilaSensor nuevo = datosEntrada;
            if (!orm->sensor.crear(nuevo, ahoraSeg, sesion)) return false;
            outGuardado = nuevo;
            return true;
        }
```

---

### 1.4. `src/stubs/contratos.h` (DTOs y Serializadores Actualizados con los Nuevos Campos)

En `src/stubs/contratos.h`, actualiza `GuardarSensorInputDTO`, `GuardarRedInputDTO` y `serializarFilaSensor`:

```cpp
    struct GuardarSensorInputDTO {
        char  hexadecimal[17]          = {0};
        char  nombre[32]               = {0};
        char  descripcion[64]          = {0};
        char  ubicacion[32]            = {0};
        char  ubicacionDescripcion[64] = {0};
        float correcionSuperior        = 0.0f;
        float correcionInferior        = 0.0f;
        float altura                   = 0.0f;
        float presionAtmosferica       = 1013.25f;
        float umbralSuperior           = 126.0f;
        float umbralInferior           = -126.0f;

        bool deserializar(LectorMsgPack& r) {
            return r.leerTexto(hexadecimal, sizeof(hexadecimal)) &&
                   r.leerTexto(nombre, sizeof(nombre)) &&
                   r.leerTexto(descripcion, sizeof(descripcion)) &&
                   r.leerTexto(ubicacion, sizeof(ubicacion)) &&
                   r.leerTexto(ubicacionDescripcion, sizeof(ubicacionDescripcion)) &&
                   r.leerFloat(correcionSuperior) &&
                   r.leerFloat(correcionInferior) &&
                   r.leerFloat(altura) &&
                   r.leerFloat(presionAtmosferica) &&
                   r.leerFloat(umbralSuperior) &&
                   r.leerFloat(umbralInferior);
        }

        FilaSensor aFilaSensor() const {
            FilaSensor f = {};
            strncpy(f.hexadecimal, hexadecimal, sizeof(f.hexadecimal) - 1);
            strncpy(f.nombre, nombre, sizeof(f.nombre) - 1);
            strncpy(f.descripcion, descripcion, sizeof(f.descripcion) - 1);
            strncpy(f.ubicacion, ubicacion, sizeof(f.ubicacion) - 1);
            strncpy(f.ubicacion_descripcion, ubicacionDescripcion, sizeof(f.ubicacion_descripcion) - 1);
            f.correcion_superior  = correcionSuperior;
            f.correcion_inferior  = correcionInferior;
            f.altura              = altura;
            f.presion_atmosferica = presionAtmosferica;
            f.umbral_superior     = umbralSuperior;
            f.umbral_inferior     = umbralInferior;
            return f;
        }
    };

    struct GuardarRedInputDTO {
        char     ssidSta[33]        = {0};
        uint8_t  passStaCifrada[64] = {0};
        size_t   lenPassSta         = 0;
        char     ssidAp[33]         = {0};
        uint8_t  passApCifrada[64]  = {0};
        size_t   lenPassAp          = 0;
        uint32_t puertoTcp          = 80;
        uint32_t puertoUdp          = 81;
        uint32_t baudiosUsb         = 115200;

        bool deserializar(LectorMsgPack& r) {
            if (!r.leerTexto(ssidSta, sizeof(ssidSta)) ||
                !r.leerBinario(passStaCifrada, sizeof(passStaCifrada), lenPassSta) ||
                !r.leerTexto(ssidAp, sizeof(ssidAp)) ||
                !r.leerBinario(passApCifrada, sizeof(passApCifrada), lenPassAp)) {
                return false;
            }
            // Campos opcionales con fallback por defecto
            if (r.hayDatos()) r.leerUInt32(puertoTcp);
            if (r.hayDatos()) r.leerUInt32(puertoUdp);
            if (r.hayDatos()) r.leerUInt32(baudiosUsb);
            return true;
        }
    };

    inline bool serializarFilaSensor(EscritorMsgPack& w, const FilaSensor& f) {
        return w.iniciarArray(15) &&
               w.escribirUInt(f.id_sensor) &&
               w.escribirTexto(f.hexadecimal) &&
               w.escribirTexto(f.nombre) &&
               w.escribirTexto(f.descripcion) &&
               w.escribirTexto(f.ubicacion) &&
               w.escribirTexto(f.ubicacion_descripcion) &&
               w.escribirFloat(f.correcion_superior) &&
               w.escribirFloat(f.correcion_inferior) &&
               w.escribirFloat(f.altura) &&
               w.escribirFloat(f.presion_atmosferica) &&
               w.escribirFloat(f.umbral_superior) &&
               w.escribirFloat(f.umbral_inferior) &&
               w.escribirUInt(f.activa) &&
               w.escribirUInt(f.fecha_creacion) &&
               w.escribirUInt(f.fecha_modificacion);
    }
```

---

### 1.5. `src/stubs/serverExample.h` (Corrección de Semillas de Usuario, Sensores e IPs de Red)

En `src/stubs/serverExample.h`, realiza estos 4 ajustes puntuales:

1. **Arreglo del Punto 1 (`USUARIO_O_PIN_INVALIDO`) y Semilla del Sensor en `inicializarBaseDeDatosORM`:**
```cpp
        // B. Semilla de Usuarios sincronizada con los perfiles del Frontend
        // Si el primer usuario tiene el nombre viejo "admin", truncamos para resembrar
        FilaUsuario primerUsr = {};
        if (orm.usuarios.buscarPorId(1, primerUsr) && strcmp(primerUsr.usuario, "admin") == 0) {
            orm.usuarios = ModeloCatalogo<FilaUsuario, uint16_t, &FilaUsuario::id_usuario>(&motorSgbd, TABLA_USUARIOS);
            motorSgbd.ddl.truncarTabla(TABLA_USUARIOS);
        }
        if (orm.usuarios.contarTotal() == 0) {
            auto sembrarUsuario = [&](const char* user, const char* pass, uint8_t rol) {
                FilaUsuario u = {};
                strncpy(u.usuario, user, sizeof(u.usuario) - 1);
                strncpy(reinterpret_cast<char*>(u.contrasena), pass, sizeof(u.contrasena) - 1);
                u.rol = rol;
                orm.usuarios.crear(u, tsBoot);
            };
            sembrarUsuario("silvana_admin", "HASH_ADMIN_2026", ROL_ADMIN);
            sembrarUsuario("juan_operador", "HASH_OPER_2026",  ROL_REGULAR);
            sembrarUsuario("encargado2",    "HASH_OPER_2026",  ROL_REGULAR);
        }

        // C. Semilla de Sensor por defecto con los nuevos campos fusionados
        if (orm.sensor.contarTotal() == 0) {
            FilaSensor sensorInicial = {};
            strncpy(sensorInicial.hexadecimal, "28FF641E82160450", sizeof(sensorInicial.hexadecimal) - 1);
            strncpy(sensorInicial.nombre, "Sensor Principal Heladera", sizeof(sensorInicial.nombre) - 1);
            strncpy(sensorInicial.descripcion, "Sonda DS18B20 sumergida en glicol", sizeof(sensorInicial.descripcion) - 1);
            strncpy(sensorInicial.ubicacion, "Laboratorio UNTDF", sizeof(sensorInicial.ubicacion) - 1);
            strncpy(sensorInicial.ubicacion_descripcion, "Sector Biología - Heladera 01", sizeof(sensorInicial.ubicacion_descripcion) - 1);
            sensorInicial.correcion_inferior  = 0.0f;
            sensorInicial.correcion_superior  = 0.0f;
            sensorInicial.altura              = 12.0f;
            sensorInicial.presion_atmosferica = 1011.8f;
            sensorInicial.umbral_inferior     = -18.0f;
            sensorInicial.umbral_superior     = -12.0f;
            orm.sensor.crear(sensorInicial, tsBoot);
        }
```

2. **En `FirmwareImpl::guardarConfig` y `FirmwareImpl::guardarSensor`:**
```cpp
            FilaSensor sensorFila = {};
            if (repositorios().sensores.buscarPorId(1, sensorFila, sesionUsr)) {
                sensorFila.umbral_superior = cfg.tempMax;
                sensorFila.umbral_inferior = cfg.tempMin;
                repositorios().sensores.guardarSensor(sensorFila, ahoraSeg, sensorFila, sesionUsr);
            }
```
Y en `guardarSensor`:
```cpp
        bool guardarSensor(const UsuarioCredencialDTO& cred, const GuardarSensorInputDTO& in, FilaSensor& out) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            uint32_t ahora = gEstadoSistema.ultimoUnixTimestamp;
            FilaSensor datos = in.aFilaSensor();
            if (!repositorios().sensores.guardarSensor(datos, ahora, out, sesion)) {
                return false;
            }
            char det[24] = {0};
            snprintf(det, sizeof(det), "S#%u:%.1f..%.1f", (unsigned)out.id_sensor, in.umbralInferior, in.umbralSuperior);
            repositorios().auditorias.registrar(sesion.idUsuario, "UPSERT_SENSOR", det, ahora, sesion);
            gEstadoSistema.totalLlamadasRecibidas++;
            return true;
        }
```

3. **En `FirmwareImpl::guardarConfigRed` (Persistencia de `puerto_tcp`, `puerto_udp` y `baudios_usb`):**
```cpp
        bool guardarConfigRed(const UsuarioCredencialDTO& cred, const GuardarRedInputDTO& in,
                              FilaConfiguracionConectividad& outRed) override {
            ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
            uint32_t ahora = gEstadoSistema.ultimoUnixTimestamp;
            outRed = {};
            strncpy(outRed.SSID_STA, in.ssidSta, sizeof(outRed.SSID_STA) - 1);
            desencriptarCampoSensible(in.passStaCifrada, in.lenPassSta, outRed.STA_password, sizeof(outRed.STA_password));
            strncpy(outRed.SSID_AP, in.ssidAp, sizeof(outRed.SSID_AP) - 1);
            desencriptarCampoSensible(in.passApCifrada, in.lenPassAp, outRed.AP_password, sizeof(outRed.AP_password));
            outRed.puerto_tcp  = (uint16_t)(in.puertoTcp > 0 ? in.puertoTcp : 80);
            outRed.puerto_udp  = (uint16_t)(in.puertoUdp > 0 ? in.puertoUdp : 81);
            outRed.baudios_usb = in.baudiosUsb > 0 ? in.baudiosUsb : 115200;

            if (!repositorios().configuraciones.guardarConectividad(outRed, ahora, sesion)) return false;
            repositorios().auditorias.registrar(sesion.idUsuario, "CAMBIO_WIFI", outRed.SSID_STA, ahora, sesion);
            gEstadoSistema.totalLlamadasRecibidas++;
            return true;
        }
```

4. **En `registrarSkeletonFirmware` (Envío de `ipSta`, `ipAp`, `puertoTcp`, `puertoUdp`, `macWiFi`):**
```cpp
        appRpc.onRequest(RPC_DB_GUARDAR_SENSOR, [](size_t, LectorMsgPack& args,
                                                   EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            GuardarSensorInputDTO in;
            if (!auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaSensor guardado = {};
            if (!servicioFirmware()->guardarSensor(auth.usuario, in, guardado)) return false;
            return resp.iniciarArray(2) && resp.escribirTexto("OK") && serializarFilaSensor(resp, guardado);
        });

        appRpc.onRequest(RPC_DB_OBTENER_RED_Y_NTP, [](size_t, LectorMsgPack& args,
                                                      EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            if (!auth.deserializar(args)) return false;

            FilaConfiguracionConectividad red = {};
            FilaServidorNTP ntp[4];
            size_t totalNtp = 0;
            servicioFirmware()->obtenerConfigRedYNtp(auth.usuario, red, ntp, totalNtp);

            uint16_t ptoTcp = red.puerto_tcp > 0 ? red.puerto_tcp : modem.obtenerPuertoTCP();
            uint16_t ptoUdp = red.puerto_udp > 0 ? red.puerto_udp : modem.obtenerPuertoUDP();
            uint32_t baudios = red.baudios_usb > 0 ? red.baudios_usb : 115200;
            String ipSta = modem.obtenerIPSta();
            String ipAp  = modem.obtenerIPAp();
            String mac   = modem.obtenerMAC();

            if (!resp.iniciarArray(14) ||
                !resp.escribirTexto("OK") ||
                !resp.escribirTexto(red.SSID_STA) ||
                !resp.escribirTexto(red.SSID_AP) ||
                !resp.escribirUInt(ptoTcp) ||
                !resp.escribirUInt(ptoUdp) ||
                !resp.escribirUInt(baudios) ||
                !resp.escribirUInt(red.fecha_modificacion) ||
                !resp.escribirBool(modem.estaStaConectado()) ||
                !resp.escribirTexto(ipSta.c_str()) ||
                !resp.escribirBool(modem.estaApActiva()) ||
                !resp.escribirTexto(ipAp.c_str()) ||
                !resp.escribirTexto(mac.c_str()) ||
                !resp.escribirUInt(modem.obtenerClientesUDPActivos()) ||
                !resp.iniciarArray(totalNtp)) {
                return false;
            }
            for (size_t i = 0; i < totalNtp; i++) {
                if (!resp.escribirTexto(ntp[i].server)) return false;
            }
            return true;
        });

        appRpc.onRequest(RPC_DB_GUARDAR_RED, [](size_t, LectorMsgPack& args,
                                                EscritorMsgPack& resp, const MetaCall&) -> bool {
            CredencialRpcDTO auth;
            GuardarRedInputDTO in;
            if (!auth.deserializar(args) || !in.deserializar(args)) return false;

            FilaConfiguracionConectividad red = {};
            if (!servicioFirmware()->guardarConfigRed(auth.usuario, in, red)) return false;
            return resp.iniciarArray(7) &&
                   resp.escribirTexto("OK") &&
                   resp.escribirTexto(red.SSID_STA) &&
                   resp.escribirTexto(red.SSID_AP) &&
                   resp.escribirUInt(red.puerto_tcp) &&
                   resp.escribirUInt(red.puerto_udp) &&
                   resp.escribirUInt(red.baudios_usb) &&
                   resp.escribirUInt(red.fecha_modificacion);
        });
```

---

## PARTE 2: Archivos del Backend (Electron)

### 2.1. Reemplazo de SQLite/Prisma por un Archivo JSON para `conexion_firmware`

Reemplaza `./src/backend/database/consultas/repositories/conexionFirmwareRepository.ts` por esta implementación directa en archivo JSON (`conexion_firmware.json`) sin depender de Prisma:

```ts
// Archivo: ./src/backend/database/consultas/repositories/conexionFirmwareRepository.ts
import * as fs from "node:fs";
import * as path from "node:path";

export interface ConexionFirmwareJSON {
  id_conexion: number;
  ip_dhcp: string;
  puerto_tcp: number;
  puerto_udp: number;
  puerto_usb: string;
  baudios_usb: number;
  fecha_modificacion: string;
}

type ConexionInput = Partial<Omit<ConexionFirmwareJSON, "id_conexion" | "fecha_modificacion">>;

const rootDir = process.env.APP_ROOT || process.cwd();
const rutaJson = path.resolve(rootDir, "src", "backend", "database", "conexion_firmware.json");

const DEFAULT_CONEXION: ConexionFirmwareJSON = {
  id_conexion: 1,
  ip_dhcp: "192.168.4.1",
  puerto_tcp: 80,
  puerto_udp: 81,
  puerto_usb: "/tmp/virtual_serial",
  baudios_usb: 115200,
  fecha_modificacion: new Date().toISOString(),
};

function leerJsonLocal(): ConexionFirmwareJSON {
  try {
    if (!fs.existsSync(rutaJson)) {
      escribirJsonLocal(DEFAULT_CONEXION);
      return { ...DEFAULT_CONEXION };
    }
    const raw = fs.readFileSync(rutaJson, "utf-8");
    const parsed = JSON.parse(raw) as Partial<ConexionFirmwareJSON>;
    return { ...DEFAULT_CONEXION, ...parsed, id_conexion: 1 };
  } catch {
    return { ...DEFAULT_CONEXION };
  }
}

function escribirJsonLocal(datos: ConexionFirmwareJSON): ConexionFirmwareJSON {
  const dir = path.dirname(rutaJson);
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
  fs.writeFileSync(rutaJson, JSON.stringify(datos, null, 2), "utf-8");
  return datos;
}

export const conexionFirmwareRepository = {
  getConexionFirmware: async (): Promise<ConexionFirmwareJSON> => leerJsonLocal(),

  upsertConexionFirmware: async (data: ConexionInput): Promise<ConexionFirmwareJSON> => {
    const actual = leerJsonLocal();
    return escribirJsonLocal({
      ...actual,
      ...data,
      id_conexion: 1,
      fecha_modificacion: new Date().toISOString(),
    });
  },

  setIpDhcp: async (ip_dhcp: string): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ ip_dhcp }),

  setPuertoTcp: async (puerto_tcp: number): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ puerto_tcp }),

  setPuertoUdp: async (puerto_udp: number): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ puerto_udp }),

  setPuertoUsb: async (puerto_usb: string, baudios_usb: number): Promise<ConexionFirmwareJSON> =>
    conexionFirmwareRepository.upsertConexionFirmware({ puerto_usb, baudios_usb }),
};
```

---

### 2.2. Actualización de `./src/backend/stubs/contratos.ts`

Actualiza las interfaces de Sensor y de Red en `contratos.ts` para que reflejen los campos fusionados y las IPs en vivo:

```ts
export interface SensorFirmwareDTO {
  idSensor: number;
  hexadecimal: string;
  nombre: string;
  descripcion: string;
  ubicacion: string;
  ubicacionDescripcion: string;
  correcionSuperior: number;
  correcionInferior: number;
  altura: number;
  presionAtmosferica: number;
  umbralSuperior: number;
  umbralInferior: number;
  activa: number;
  fechaCreacion: number;
  fechaModificacion: number;
}

export interface GuardarSensorFirmwareInput {
  credencial: CredencialRpcDTO;
  hexadecimal: string;
  nombre: string;
  descripcion?: string;
  ubicacion: string;
  ubicacionDescripcion?: string;
  correcionSuperior: number;
  correcionInferior: number;
  altura: number;
  presionAtmosferica: number;
  umbralSuperior: number;
  umbralInferior: number;
}

export interface ConfigRedYNtpFirmwareDTO {
  ssidSta: string;
  ssidAp: string;
  puertoTcp: number;
  puertoUdp: number;
  baudiosUsb: number;
  fechaModificacion: number;
  staConectado: boolean;
  ipSta: string;
  apActivo: boolean;
  ipAp: string;
  macWiFi: string;
  clientesUdpActivos: number;
  servidoresNtp: string[];
}

export interface GuardarRedFirmwareInput {
  credencial: CredencialRpcDTO;
  ssidSta: string;
  passwordStaPlana: string;
  ssidAp: string;
  passwordApPlana: string;
  puertoTcp?: number;
  puertoUdp?: number;
  baudiosUsb?: number;
}

export interface ResultadoGuardarRedFirmwareDTO {
  ssidSta: string;
  ssidAp: string;
  puertoTcp: number;
  puertoUdp: number;
  baudiosUsb: number;
  fechaModificacion: number;
  requiereReinicio: boolean;
  mensaje: string;
}
```

---

### 2.3. Actualización de `./src/backend/stubs/clientExample.ts`

Actualiza `TuplaSensor`, `mapearTuplaSensor`, `guardarSensorFirmware`, `obtenerConfigRedYNtpFirmware` y `guardarConfigRedFirmware`:

```ts
type TuplaSensor = [
  number, string, string, string, string, string,
  number, number, number, number, number, number,
  number, number, number
];

function mapearTuplaSensor(t: TuplaSensor): SensorFirmwareDTO {
  return {
    idSensor: Number(t[0]),
    hexadecimal: String(t[1]),
    nombre: String(t[2] ?? ""),
    descripcion: String(t[3] ?? ""),
    ubicacion: String(t[4] ?? ""),
    ubicacionDescripcion: String(t[5] ?? ""),
    correcionSuperior: Number(t[6]),
    correcionInferior: Number(t[7]),
    altura: Number(t[8]),
    presionAtmosferica: Number(t[9]),
    umbralSuperior: Number(t[10]),
    umbralInferior: Number(t[11]),
    activa: Number(t[12]),
    fechaCreacion: Number(t[13]),
    fechaModificacion: Number(t[14]),
  };
}
```

Y dentro de `FirmwareStub`:

```ts
  public readonly guardarSensorFirmware = async (
    input: GuardarSensorFirmwareInput,
    opts?: OpcionesLlamada
  ): Promise<SensorFirmwareDTO> => {
    const [tupla] = await pila.appRpc.call<[TuplaSensor]>(
      RPC.DB_GUARDAR_SENSOR,
      [
        ...serializarCredencial(input.credencial),
        input.hexadecimal,
        input.nombre,
        input.descripcion ?? "",
        input.ubicacion,
        input.ubicacionDescripcion ?? "",
        Number(input.correcionSuperior),
        Number(input.correcionInferior),
        Number(input.altura),
        Number(input.presionAtmosferica),
        Number(input.umbralSuperior),
        Number(input.umbralInferior),
      ],
      opts
    );
    return mapearTuplaSensor(tupla);
  };

  public readonly obtenerConfigRedYNtpFirmware = async (
    cred: CredencialRpcDTO,
    opts?: OpcionesLlamada
  ): Promise<ConfigRedYNtpFirmwareDTO> => {
    const [
      ssidSta, ssidAp, puertoTcp, puertoUdp, baudiosUsb,
      fechaModificacion, staConectado, ipSta, apActivo,
      ipAp, macWiFi, clientesUdpActivos, servidoresNtp,
    ] = await pila.appRpc.call<
      [string, string, number, number, number, number, boolean, string, boolean, string, string, number, string[]]
    >(RPC.DB_OBTENER_RED_Y_NTP, [...serializarCredencial(cred)], opts);

    return {
      ssidSta: String(ssidSta),
      ssidAp: String(ssidAp),
      puertoTcp: Number(puertoTcp),
      puertoUdp: Number(puertoUdp),
      baudiosUsb: Number(baudiosUsb),
      fechaModificacion: Number(fechaModificacion),
      staConectado: Boolean(staConectado),
      ipSta: String(ipSta),
      apActivo: Boolean(apActivo),
      ipAp: String(ipAp),
      macWiFi: String(macWiFi),
      clientesUdpActivos: Number(clientesUdpActivos),
      servidoresNtp: (servidoresNtp ?? []).map(String),
    };
  };

  public readonly guardarConfigRedFirmware = async (
    input: GuardarRedFirmwareInput,
    opts?: OpcionesLlamada
  ): Promise<ResultadoGuardarRedFirmwareDTO> => {
    const passStaBin = encriptarCampoSensible(input.passwordStaPlana);
    const passApBin = encriptarCampoSensible(input.passwordApPlana);

    const [ssidSta, ssidAp, puertoTcp, puertoUdp, baudiosUsb, fechaModificacion] =
      await pila.appRpc.call<[string, string, number, number, number, number]>(
        RPC.DB_GUARDAR_RED,
        [
          ...serializarCredencial(input.credencial),
          input.ssidSta,
          passStaBin,
          input.ssidAp,
          passApBin,
          Number(input.puertoTcp ?? 80),
          Number(input.puertoUdp ?? 81),
          Number(input.baudiosUsb ?? 115200),
        ],
        opts
      );

    return {
      ssidSta: String(ssidSta),
      ssidAp: String(ssidAp),
      puertoTcp: Number(puertoTcp),
      puertoUdp: Number(puertoUdp),
      baudiosUsb: Number(baudiosUsb),
      fechaModificacion: Number(fechaModificacion),
      requiereReinicio: true,
      mensaje: "Configuración guardada en /db/wifi.bin. Reinicie el ESP32 para aplicar los cambios de red.",
    };
  };
```

---

## PARTE 3: Ajustes en el Frontend (React)

1. **En `src/backend/database/consultas/consultasMixtas.ts`:**
   Dado que `sensor_fw` en el ESP32 ya retorna `nombre`, `descripcion`, `ubicacion`, `ubicacionDescripcion`, `correcionSuperior`, `correcionInferior`, `altura` y `presionAtmosferica`, `obtenerSensoresEnriquecidosConFirmware` ahora simplemente delega en `firmware.listarSensoresFirmware(cred, opts)`:
   ```ts
   obtenerSensoresEnriquecidosConFirmware: async (
     cred: CredencialRpcDTO,
     opts?: OpcionesLlamada
   ) => {
     return await firmware.listarSensoresFirmware(cred, opts);
   }
   ```

2. **En `src/frontend/components/DatabaseExplorer.tsx`:**
   * En `SensoresTab`: Cambia `s.errorInferior` / `s.errorSuperior` por `s.correcionInferior` / `s.correcionSuperior` y muestra `s.altura` ($\text{msnm}$) y `s.presionAtmosferica` ($\text{hPa}$).
   * En `RedNtpTab`: Ahora `data` ya trae directamente `data.ipSta`, `data.ipAp`, `data.puertoTcp`, `data.puertoUdp` y `data.macWiFi`, por lo que puedes mostrarlos en las filas del detalle de Red para copiar la IP y conectarte por Wi-Fi durante los tests.