#pragma once
#include "orm/ORM.hpp"
#include "../hardware/driver/gestorsensores/EstadoSensor.hpp"

enum class CriticidadEvento : uint8_t {
    CRITICIDAD_BAJA    = 0,
    CRITICIDAD_MEDIA   = 1,
    CRITICIDAD_CRITICA = 2
};

enum class TipoRecepcionCorreo : uint8_t {
    RECIBIR_TODOS            = 0,
    RECIBIR_EVENTOS          = 1,
    RECIBIR_EVENTOS_CRITICOS = 2
};

enum class EstadoEnvioCorreo : uint8_t {
    ENVIO_PENDIENTE = 0,
    ENVIO_ENVIADO   = 1,
    ENVIO_FALLIDO   = 2
};

enum class MedioConexion : uint8_t {
    MEDIO_INTERNO  = 0,
    MEDIO_USB      = 1,
    MEDIO_WIFI_STA = 2,
    MEDIO_WIFI_AP  = 3
};

enum class ResultadoAuditoria : uint8_t {
    RESULTADO_EXITO               = 0,
    RESULTADO_DENEGADO_ROL        = 1,
    RESULTADO_CREDENCIAL_INVALIDA = 2,
    RESULTADO_INTRUSION_TERMINAL  = 3,
    RESULTADO_ERROR_VALIDACION    = 4
};

inline void iniciarEsquema() {
    db.setSession("system_boot", ROL_ADMIN);

    // 1. CALIBRACIÓN ATMOSFÉRICA (Singleton PK=1)
    db.createTable("Calibracion")
      .addAtribute("id_calibracion").typeInt().pk().valueDefault("1")
      .addAtribute("altura").typeFloat().notNull().valueDefault("0.0")
          .check([](const String& h) { return h.toFloat() >= -500.0f && h.toFloat() <= 9000.0f; }, "altura en [-500..9000]")
      .addAtribute("presion_atmosferica").typeFloat().notNull().valueDefault("1013.25")
          .check([](const String& p) { return p.toFloat() >= 300.0f && p.toFloat() <= 1100.0f; }, "presion en [300..1100]")
      .addAtribute("temp_congelacion_ref").typeFloat().notNull().valueDefault("0.0")
      .addAtribute("temp_ebullicion_ref").typeFloat().notNull().valueDefault("100.0")
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 2. SEGURIDAD DE CANAL Y CERTIFICADOS (Singleton PK=1)
    db.createTable("Lts")
      .addAtribute("id_lts").typeInt().pk().valueDefault("1")
      .addAtribute("nombre_certificado").typeSrt().notNull().varChar(24)
      .addAtribute("certificado_fingerprint").typeSrt().notNull().varChar(64)
      .addAtribute("clave_aes_canal").typeSrt().notNull().varChar(64)
      .addAtribute("secreto_hmac_sesion").typeSrt().notNull().varChar(64)
      .addAtribute("cadena_certificados").typeSrt().varChar(64).valueDefault("CA-UNTDF-ROOT")
      .addAtribute("fecha_inicio_vigencia").typeInt().valueDefault("0")
      .addAtribute("fecha_fin_vigencia").typeInt().valueDefault("0")
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 3. CONECTIVIDAD WIFI STA/AP Y PUERTOS (Singleton PK=1)
    db.createTable("ConfiguracionConectividad")
      .addAtribute("id_configuracion_conectividad").typeInt().pk().valueDefault("1")
      .addAtribute("SSID_STA").typeSrt().notNull().varChar(33)
      .addAtribute("STA_password_aes").typeSrt().varChar(128).valueDefault("")
      .addAtribute("SSID_AP").typeSrt().notNull().varChar(33)
      .addAtribute("AP_password_aes").typeSrt().varChar(128).valueDefault("")
      .addAtribute("puerto_tcp").typeInt().notNull().valueDefault("80")
          .check([](const String& p) { int v = p.toInt(); return v > 0 && v <= 65535; }, "puerto_tcp 1..65535")
      .addAtribute("puerto_udp").typeInt().notNull().valueDefault("81")
          .check([](const String& p) { int v = p.toInt(); return v > 0 && v <= 65535; }, "puerto_udp 1..65535")
      .addAtribute("baudios_usb").typeInt().notNull().valueDefault("115200")
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 4. SERVIDORES NTP CON PRIORIDAD DE FAILOVER
    db.createTable("ServidoresNtp")
      .addAtribute("id_server").typeInt().pk()
      .addAtribute("server").typeSrt().unique().notNull().varChar(40)
      .addAtribute("prioridad").typeInt().notNull().valueDefault("1")
          .check([](const String& p) { int v = p.toInt(); return v >= 1 && v <= 10; }, "prioridad 1..10")
      .addAtribute("timeout_ms").typeInt().notNull().valueDefault("3000")
      .addAtribute("fallos_consecutivos").typeInt().notNull().valueDefault("0")
      .addAtribute("ultimo_sync_exitoso").typeInt().valueDefault("0")
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 5. CONFIGURACIÓN DEL SERVIDOR SMTP (Singleton PK=1)
    db.createTable("ConfiguracionSmtp")
      .addAtribute("id_configuracion_smtp").typeInt().pk().valueDefault("1")
      .addAtribute("host_smtp").typeSrt().notNull().varChar(40)
      .addAtribute("puerto").typeInt().notNull().valueDefault("465")
      .addAtribute("usar_tls").typeInt().notNull().valueDefault("1")
      .addAtribute("usuario").typeSrt().notNull().varChar(48)
      .addAtribute("password_aes").typeSrt().notNull().varChar(128)
      .addAtribute("email_remitente").typeSrt().notNull().varChar(48)
      .addAtribute("nombre_remitente").typeSrt().varChar(32).valueDefault("Heladera Lab UNTDF")
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 6. USUARIOS DEL DISPOSITIVO
    db.createTable("Usuarios")
      .addAtribute("id_usuario").typeInt().pk()
      .addAtribute("usuario").typeSrt().unique().notNull().varChar(20)
      .addAtribute("salt").typeSrt().notNull().varChar(16)
      .addAtribute("contrasena_hash").typeSrt().notNull().varChar(64)
      .addAtribute("rol").typeEnum<SGBD::RolSGBD>({
          ROL_ANONIMO, ROL_LECTOR, ROL_OPERADOR, ROL_ADMIN
      }).valueDefault(ROL_LECTOR)
      .addAtribute("intentos_fallidos").typeInt().notNull().valueDefault("0")
      .addAtribute("bloqueado").typeInt().notNull().valueDefault("0")
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 7. DESTINATARIOS DE CORREO ELECTRÓNICO
    db.createTable("Correos")
      .addAtribute("id_correo").typeInt().pk()
      .addAtribute("nombre").typeSrt().notNull().varChar(24)
      .addAtribute("apellido").typeSrt().varChar(24).valueDefault("")
      .addAtribute("correo_electronico").typeSrt().unique().notNull().varChar(48)
          .check([](const String& e) { return e.indexOf('@') > 0 && e.indexOf('.') > 2; }, "formato email valido")
      .addAtribute("tipo_recepcion_mensajes").typeEnum<TipoRecepcionCorreo>({
          TipoRecepcionCorreo::RECIBIR_TODOS,
          TipoRecepcionCorreo::RECIBIR_EVENTOS,
          TipoRecepcionCorreo::RECIBIR_EVENTOS_CRITICOS
      }).valueDefault(TipoRecepcionCorreo::RECIBIR_TODOS)
      .addAtribute("ultimo_estado_envio").typeEnum<EstadoEnvioCorreo>({
          EstadoEnvioCorreo::ENVIO_PENDIENTE,
          EstadoEnvioCorreo::ENVIO_ENVIADO,
          EstadoEnvioCorreo::ENVIO_FALLIDO
      }).valueDefault(EstadoEnvioCorreo::ENVIO_PENDIENTE)
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 8. AUDITORÍA INMUTABLE (NO REPUDIO Y REGISTRO DE INTRUSIONES)
    db.createTable("Auditoria")
      .addAtribute("id_auditoria").typeInt().pk()
      .addAtribute("id_usuario").typeInt().notNull().FK("Usuarios", "id_usuario", RESTRICT)
      .addAtribute("accion").typeSrt().notNull().varChar(32)
      .addAtribute("tabla_afectada").typeSrt().varChar(32).valueDefault("NINGUNA")
      .addAtribute("valor_previo").typeSrt().varChar(64).valueDefault("")
      .addAtribute("valor_nuevo").typeSrt().varChar(64).valueDefault("")
      .addAtribute("detalle").typeSrt().notNull().varChar(120)
      .addAtribute("medio_conexion").typeEnum<MedioConexion>({
          MedioConexion::MEDIO_INTERNO, MedioConexion::MEDIO_USB,
          MedioConexion::MEDIO_WIFI_STA, MedioConexion::MEDIO_WIFI_AP
      }).valueDefault(MedioConexion::MEDIO_INTERNO)
      .addAtribute("origen_remoto").typeSrt().varChar(24).valueDefault("LOCAL")
      .addAtribute("resultado").typeEnum<ResultadoAuditoria>({
          ResultadoAuditoria::RESULTADO_EXITO,
          ResultadoAuditoria::RESULTADO_DENEGADO_ROL,
          ResultadoAuditoria::RESULTADO_CREDENCIAL_INVALIDA,
          ResultadoAuditoria::RESULTADO_INTRUSION_TERMINAL,
          ResultadoAuditoria::RESULTADO_ERROR_VALIDACION
      }).valueDefault(ResultadoAuditoria::RESULTADO_EXITO)
      .addAtribute("criticidad").typeEnum<CriticidadEvento>({
          CriticidadEvento::CRITICIDAD_BAJA,
          CriticidadEvento::CRITICIDAD_MEDIA,
          CriticidadEvento::CRITICIDAD_CRITICA
      }).valueDefault(CriticidadEvento::CRITICIDAD_BAJA)
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0");

    // 9. CATÁLOGO FÍSICO DE SENSORES (HASTA 6 SONDAS DS18B20)
    db.createTable("Sensor")
      .addAtribute("id_sensor").typeInt().pk()
      .addAtribute("hexadecimal").typeSrt().unique().notNull().varChar(16)
          .check([](const String& m) { return m.length() == 16; }, "ROM hex de 16 chars")
      .addAtribute("mac2").typeSrt().notNull().varChar(2)
      .addAtribute("nombre").typeSrt().notNull().varChar(32).valueDefault("Sensor Nuevo")
      .addAtribute("descripcion").typeSrt().varChar(64).valueDefault("")
      .addAtribute("ubicacion").typeSrt().notNull().varChar(32).valueDefault("Sin asignar")
      .addAtribute("ubicacion_descripcion").typeSrt().varChar(64).valueDefault("")
      .addAtribute("offset_calibracion").typeFloat().notNull().valueDefault("0.0")
      .addAtribute("correcion_superior").typeFloat().notNull().valueDefault("0.0")
      .addAtribute("correcion_inferior").typeFloat().notNull().valueDefault("0.0")
      .addAtribute("umbral_superior").typeFloat().notNull().valueDefault("125.0")
          .check([](const String& u) { float v = u.toFloat(); return v >= -55.0f && v <= 125.0f; }, "umbral_sup [-55.0..125.0]")
      .addAtribute("umbral_inferior").typeFloat().notNull().valueDefault("-55.0")
          .check([](const String& u) { float v = u.toFloat(); return v >= -55.0f && v <= 125.0f; }, "umbral_inf [-55.0..125.0]")
      .addAtribute("resolucion_bits").typeInt().notNull().valueDefault("12")
          .check([](const String& r) { int v = r.toInt(); return v >= 9 && v <= 12; }, "resolucion 9..12 bits")
      .addAtribute("alimentacion_parasita").typeInt().notNull().valueDefault("0")
      .addAtribute("estado_actual").typeEnum<EstadoSensor>({
          EstadoSensor::OK, EstadoSensor::ALARMA_SUPERIOR, EstadoSensor::ALARMA_INFERIOR,
          EstadoSensor::SLOT_VACIO, EstadoSensor::DIRECCION_ROM_INVALIDA,
          EstadoSensor::FAMILIA_NO_COMPATIBLE, EstadoSensor::SCRATCHPAD_VACIO_CEROS,
          EstadoSensor::ERROR_CRC_SCRATCHPAD, EstadoSensor::CONVERSION_PENDIENTE,
          EstadoSensor::TIMEOUT_CONVERSION, EstadoSensor::FALLO_ESCRITURA_EEPROM,
          EstadoSensor::DESCONECTADO, EstadoSensor::ALIMENTACION_INSUFICIENTE,
          EstadoSensor::REINICIO_ENERGIA_85C, EstadoSensor::CORTOCIRCUITO_VDD,
          EstadoSensor::CORTOCIRCUITO_GND, EstadoSensor::CIRCUITO_ABIERTO
      }).valueDefault(EstadoSensor::OK)
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 10. TELEMETRÍA HISTÓRICA DE TEMPERATURA
    db.createTable("Senso")
      .addAtribute("id_senso").typeInt().pk()
      .addAtribute("id_sensor").typeInt().notNull().FK("Sensor", "id_sensor", CASCADE)
      .addAtribute("temperatura_cruda").typeFloat().notNull()
      .addAtribute("temperatura_corregida").typeFloat().notNull()
      .addAtribute("estado_sensor").typeEnum<EstadoSensor>({
          EstadoSensor::OK, EstadoSensor::ALARMA_SUPERIOR, EstadoSensor::ALARMA_INFERIOR,
          EstadoSensor::ERROR_CRC_SCRATCHPAD, EstadoSensor::DESCONECTADO,
          EstadoSensor::ALIMENTACION_INSUFICIENTE, EstadoSensor::REINICIO_ENERGIA_85C,
          EstadoSensor::CORTOCIRCUITO_VDD, EstadoSensor::CORTOCIRCUITO_GND,
          EstadoSensor::CIRCUITO_ABIERTO
      }).valueDefault(EstadoSensor::OK)
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0");

    // 11. EVENTOS DE TRANSICIÓN Y VIOLACIÓN DE UMBRALES POR SENSOR
    db.createTable("EventoSensor")
      .addAtribute("id_evento_sensor").typeInt().pk()
      .addAtribute("id_sensor").typeInt().notNull().FK("Sensor", "id_sensor", CASCADE)
      .addAtribute("id_senso").typeInt().notNull().FK("Senso", "id_senso", CASCADE)
      .addAtribute("estado_previo_sensor").typeInt().notNull()
      .addAtribute("evento").typeInt().notNull()
      .addAtribute("criticidad").typeEnum<CriticidadEvento>({
          CriticidadEvento::CRITICIDAD_BAJA,
          CriticidadEvento::CRITICIDAD_MEDIA,
          CriticidadEvento::CRITICIDAD_CRITICA
      }).valueDefault(CriticidadEvento::CRITICIDAD_MEDIA)
      .addAtribute("notificado_smtp").typeEnum<EstadoEnvioCorreo>({
          EstadoEnvioCorreo::ENVIO_PENDIENTE,
          EstadoEnvioCorreo::ENVIO_ENVIADO,
          EstadoEnvioCorreo::ENVIO_FALLIDO
      }).valueDefault(EstadoEnvioCorreo::ENVIO_PENDIENTE)
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0")
      .addAtribute("fecha_modificacion").typeInt().valueDefault("0");

    // 12. EVENTOS DEL SISTEMA (BUS 1-WIRE, CAÍDA DE RED, MEMORIA SD LLENA)
    db.createTable("EventoSistema")
      .addAtribute("id_evento_sistema").typeInt().pk()
      .addAtribute("estado_bus").typeEnum<EstadoBus>({
          EstadoBus::OK, EstadoBus::LINEA_EN_CORTO_GND, EstadoBus::SIN_PULSO_PRESENCIA,
          EstadoBus::TOPOLOGIA_MODIFICADA, EstadoBus::FALLO_CONVERSION_GLOBAL,
          EstadoBus::LIMITE_SENSORES_EXCEDIDO
      }).valueDefault(EstadoBus::OK)
      .addAtribute("evento_gestor").typeEnum<EventoGestor>({
          EventoGestor::NINGUNO, EventoGestor::BUS_INICIALIZADO,
          EventoGestor::NUEVO_SENSOR_DETECTADO, EventoGestor::SENSOR_DESCONECTADO_EN_CALIENTE,
          EventoGestor::CONVERSION_COMPLETADA, EventoGestor::ERROR_BUS_ELECTRICO,
          EventoGestor::ALARMA_DETECTADA_EN_BUS
      }).valueDefault(EventoGestor::NINGUNO)
      .addAtribute("criticidad").typeEnum<CriticidadEvento>({
          CriticidadEvento::CRITICIDAD_BAJA,
          CriticidadEvento::CRITICIDAD_MEDIA,
          CriticidadEvento::CRITICIDAD_CRITICA
      }).valueDefault(CriticidadEvento::CRITICIDAD_MEDIA)
      .addAtribute("espacio_sd_libre_kb").typeInt().valueDefault("0")
      .addAtribute("heap_libre_bytes").typeInt().valueDefault("0")
      .addAtribute("detalle").typeSrt().notNull().varChar(96)
      .addAtribute("notificado_smtp").typeEnum<EstadoEnvioCorreo>({
          EstadoEnvioCorreo::ENVIO_PENDIENTE,
          EstadoEnvioCorreo::ENVIO_ENVIADO,
          EstadoEnvioCorreo::ENVIO_FALLIDO
      }).valueDefault(EstadoEnvioCorreo::ENVIO_PENDIENTE)
      .addAtribute("activa").typeInt().valueDefault("1")
      .addAtribute("fecha_creacion").typeInt().valueDefault("0");

    // 13. ÍNDICES DE ACCESO RÁPIDO
    //db.createIndex("idx_sensor_hex").attachTable("Sensor").onAtribute("hexadecimal").orderBy(ASC);
    //db.createIndex("idx_senso_fecha").attachTable("Senso").onAtribute("fecha_creacion").orderBy(DESC);
    //db.createIndex("idx_evento_sensor_smtp").attachTable("EventoSensor").onAtribute("notificado_smtp").orderBy(ASC);
    //db.createIndex("idx_auditoria_fecha").attachTable("Auditoria").onAtribute("fecha_creacion").orderBy(DESC);
    //db.createIndex("idx_ntp_prioridad").attachTable("ServidoresNtp").onAtribute("prioridad").orderBy(ASC);

    // 14. REGLAS DCL (SEGURIDAD DE TABLAS POR ROL)
    // Ningún rol (ni siquiera Operador) puede borrar ni modificar filas de auditoría (Append-Only estricto)
    db.revoke(PERM_UPDATE | PERM_DELETE).onTable("Auditoria").fromRole(ROL_OPERADOR);
    db.revoke(PERM_UPDATE | PERM_DELETE).onTable("Auditoria").fromRole(ROL_LECTOR);

    // Solo el ADMIN puede alterar umbrales de sensores, red, SMTP, calibración, usuarios y correos
    db.revoke(PERM_INSERT | PERM_UPDATE | PERM_DELETE).onTable("Sensor").fromRole(ROL_OPERADOR);
    db.revoke(PERM_INSERT | PERM_UPDATE | PERM_DELETE).onTable("ConfiguracionSmtp").fromRole(ROL_OPERADOR);
    db.revoke(PERM_INSERT | PERM_UPDATE | PERM_DELETE).onTable("ConfiguracionConectividad").fromRole(ROL_OPERADOR);
    db.revoke(PERM_INSERT | PERM_UPDATE | PERM_DELETE).onTable("Usuarios").fromRole(ROL_OPERADOR);
    db.revoke(PERM_INSERT | PERM_UPDATE | PERM_DELETE).onTable("Lts").fromRole(ROL_OPERADOR);
    db.revoke(PERM_INSERT | PERM_UPDATE | PERM_DELETE).onTable("Correos").fromRole(ROL_OPERADOR);
}   