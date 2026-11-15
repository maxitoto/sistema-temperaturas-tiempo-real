#pragma once
#include "DjangoModel.hpp"

namespace DbNotRuntime::PhysicalModels {

    using namespace DbNotRuntime::Models;

    // Registra en PostgreSQL todos los tipos ENUM nativos requeridos por el modelo físico antes de migrar las tablas.
    inline bool registerAllPostgresEnums() {
        return PgEnumChoices::registerChoices("enum_rol_usuario", {"ADMIN","NO_ADMIN","NINGUNO"})
            && PgEnumChoices::registerChoices("criticidad_evento_enum", {"BAJA", "MEDIA", "CRITICA"})
            && PgEnumChoices::registerChoices("tipo_recepcion_correo_enum", {"TODOS", "EVENTOS", "CRITICOS"})
            && PgEnumChoices::registerChoices("estado_envio_correo_enum", {"PENDIENTE", "ENVIADO", "FALLIDO"})
            && PgEnumChoices::registerChoices("medio_conexion_enum", {"INTERNO", "USB", "WIFI_STA", "WIFI_AP"})
            && PgEnumChoices::registerChoices("resultado_auditoria_enum", {
                   "EXITO", "DENEGADO_ROL", "CREDENCIAL_INVALIDA", "INTRUSION_TERMINAL", "ERROR_VALIDACION"
               });
    }

    class ConfiguracionRedModel final : public Model {
        public: 
            MetaOptions meta() const override {return { "ConfiguracionRed" }; }
            std::vector<Field> fields() const override {
                return {
                    AutoField("id_configuracion_red"),
                    CharField("ssid_sta", 35).default_val(""),
                    CharField("password_sta", 64).default_val(""),
                    CharField("ssid_ap", 35).default_val("ESP32-SecureAP"),
                    CharField("password_ap", 64).default_val("ESP32secure1234@"),
                    DateTimeField("creado_el"),
                    DateTimeField("actualizado_el").auto_now(true)
                };
            }
    };

    class UsuarioModel final : public Model {
        public: 
            MetaOptions meta() const override {return { "Usuario" }; }
            std::vector<Field> fields() const override {
                return {
                    AutoField("id_usuario"),
                    CharField("usuario", 16).unique(true),
                    CharField("salt", 8).default_val("SALT"),
                    TextField("contrasena_hash"),
                    EnumField("rol", "enum_rol_usuario").default_val("NINGUNO"),
                    SmallIntegerField("intentos_fallidos")
                        .check(
                            "intentos_fallidos >= 0 AND intentos_fallidos <= 3",
                            +[](const String& v) -> bool { int n = v.toInt(); return n >= 0 && n <= 3; }
                        )
                        .default_val(0),
                    BooleanField("bloqueado").default_val("FALSE"),
                    BooleanField("activa").default_val("TRUE"),
                    DateTimeField("creado_el"),
                    DateTimeField("actualizado_el").auto_now(true)
                };
            }
    };

    // 1. Modelo físico de la tabla Calibracion.
    // class CalibracionModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "Calibracion" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_calibracion"),
    //             FloatField("altura").default_val(0.0f),
    //             FloatField("presion_atmosferica").default_val(1013.25f),
    //             FloatField("temp_congelacion_ref").default_val(0.0f),
    //             FloatField("temp_ebullicion_ref").default_val(100.0f),
    //             BigIntegerField("fecha_modificacion").default_val(0)
    //         };
    //     }
    // };

    // // 2. Modelo físico de la tabla Lts.
    // class LtsModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "Lts" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_lts"),
    //             CharField("version_firmware", 32).default_val("1.0.0"),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0)
    //         };
    //     }
    // };

    // // 3. Modelo físico de la tabla ConfiguracionConectividad.
    // class ConfiguracionConectividadModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "ConfiguracionConectividad" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_configuracion_conectividad"),
    //             CharField("ssid_sta", 64).default_val(""),
    //             CharField("sta_password_aes", 128).default_val(""),
    //             CharField("ssid_ap", 64).default_val("ESP32-SecureAP"),
    //             CharField("ap_password_aes", 128).default_val(""),
    //             PositiveIntegerField("puerto_tcp").default_val(8443),
    //             PositiveIntegerField("puerto_udp").default_val(8444),
    //             PositiveIntegerField("baudios_usb").default_val(115200),
    //             BigIntegerField("fecha_modificacion").default_val(0)
    //         };
    //     }
    // };

    // // 4. Modelo físico de la tabla ServidoresNtp.
    // class ServidoresNtpModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "ServidoresNtp" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_server"),
    //             CharField("server", 96).unique(),
    //             SmallIntegerField("prioridad").default_val(1).check("prioridad >= 1 AND prioridad <= 10"),
    //             PositiveIntegerField("timeout_ms").default_val(2500),
    //             PositiveIntegerField("fallos_consecutivos").default_val(0),
    //             BigIntegerField("ultimo_sync_exitoso").default_val(0),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0),
    //             BigIntegerField("fecha_modificacion").default_val(0)
    //         };
    //     }
    // };

    // // 5. Modelo físico de la tabla ConfiguracionSmtp.
    // class ConfiguracionSmtpModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "ConfiguracionSmtp" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_configuracion_smtp"),
    //             CharField("host_smtp", 96).default_val(""),
    //             PositiveIntegerField("puerto").default_val(465),
    //             BooleanField("usar_tls").default_val("TRUE"),
    //             CharField("usuario", 96).default_val(""),
    //             CharField("password_aes", 128).default_val(""),
    //             CharField("email_remitente", 96).default_val(""),
    //             CharField("nombre_remitente", 64).default_val(""),
    //             BigIntegerField("fecha_modificacion").default_val(0)
    //         };
    //     }
    // };

    // // 6. Modelo físico de la tabla Usuarios (unicidad garantizada por .unique() en columna usuario).
    // class UsuariosModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "Usuarios" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_usuario"),
    //             CharField("usuario", 48).unique(),
    //             CharField("salt", 32),
    //             CharField("contrasena_hash", 64),
    //             EnumField("rol", "rol_usuario_enum").default_val("OPERADOR"),
    //             PositiveIntegerField("intentos_fallidos").default_val(0),
    //             BooleanField("bloqueado").default_val("FALSE"),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0),
    //             BigIntegerField("fecha_modificacion").default_val(0)
    //         };
    //     }
    // };

    // // 7. Modelo físico de la tabla Correos.
    // class CorreosModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "Correos" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_correo"),
    //             CharField("nombre", 48),
    //             CharField("apellido", 48),
    //             CharField("correo_electronico", 96).unique(),
    //             EnumField("tipo_recepcion_mensajes", "tipo_recepcion_correo_enum").default_val("TODOS"),
    //             EnumField("ultimo_estado_envio", "estado_envio_correo_enum").default_val("PENDIENTE"),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0),
    //             BigIntegerField("fecha_modificacion").default_val(0)
    //         };
    //     }
    // };

    // // 8. Modelo físico de la tabla Auditoria (con FK hacia Usuarios).
    // class AuditoriaModel final : public Model {
    // public:
    //     MetaOptions meta() const override {
    //         MetaOptions m;
    //         m.db_table = "Auditoria";
    //         m.indexes  = { Index("idx_auditoria_fecha", {"fecha_creacion"}) };
    //         return m;
    //     }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_auditoria"),
    //             ForeignKey("id_usuario", "Usuarios", "id_usuario", PROTECT),
    //             CharField("accion", 48),
    //             CharField("tabla_afectada", 48),
    //             CharField("valor_previo", 64).default_val(""),
    //             CharField("valor_nuevo", 64).default_val(""),
    //             CharField("detalle", 120).default_val(""),
    //             EnumField("medio_conexion", "medio_conexion_enum").default_val("INTERNO"),
    //             CharField("origen_remoto", 48).default_val("LOCAL"),
    //             EnumField("resultado", "resultado_auditoria_enum").default_val("EXITO"),
    //             EnumField("criticidad", "criticidad_evento_enum").default_val("BAJA"),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0)
    //         };
    //     }
    // };

    // // 9. Modelo físico de la tabla Sensor (unicidad garantizada por .unique() en hexadecimal).
    // class SensorModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "Sensor" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_sensor"),
    //             CharField("hexadecimal", 24).unique(),
    //             CharField("mac2", 24),
    //             CharField("nombre", 48).default_val("Sensor"),
    //             TextField("descripcion").default_val(""),
    //             CharField("ubicacion", 64).default_val("Sin asignar"),
    //             TextField("ubicacion_descripcion").default_val(""),
    //             FloatField("offset_calibracion").default_val(0.0f),
    //             FloatField("correcion_superior").default_val(0.0f),
    //             FloatField("correcion_inferior").default_val(0.0f),
    //             FloatField("umbral_superior").default_val(125.0f),
    //             FloatField("umbral_inferior").default_val(-55.0f),
    //             SmallIntegerField("resolucion_bits").default_val(12).check("resolucion_bits >= 9 AND resolucion_bits <= 12"),
    //             BooleanField("alimentacion_parasita").default_val("FALSE"),
    //             SmallIntegerField("estado_actual").default_val(0),
    //             DateTimeField("fecha_creacion").default_val(0),
    //             DateTimeField("fecha_modificacion").default_val(0),
    //         };
    //     }
    // };

    // // 10. Modelo físico de la tabla Senso (con FK hacia Sensor).
    // class SensoModel final : public Model {
    // public:
    //     MetaOptions meta() const override {
    //         MetaOptions m;
    //         m.db_table = "Senso";
    //         m.indexes  = { Index("idx_senso_sensor_fecha", {"id_sensor", "fecha_creacion"}) };
    //         return m;
    //     }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_senso"),
    //             ForeignKey("id_sensor", "Sensor", "id_sensor", CASCADE),
    //             FloatField("temperatura_cruda"),
    //             FloatField("temperatura_corregida"),
    //             SmallIntegerField("estado_sensor").default_val(0),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0)
    //         };
    //     }
    // };

    // // 11. Modelo físico de la tabla EventoSensor (con FKs hacia Sensor y Senso).
    // class EventoSensorModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "EventoSensor" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_evento_sensor"),
    //             ForeignKey("id_sensor", "Sensor", "id_sensor", CASCADE),
    //             ForeignKey("id_senso", "Senso", "id_senso", CASCADE),
    //             SmallIntegerField("estado_previo_sensor").default_val(0),
    //             SmallIntegerField("evento").default_val(0),
    //             EnumField("criticidad", "criticidad_evento_enum").default_val("BAJA"),
    //             EnumField("notificado_smtp", "estado_envio_correo_enum").default_val("PENDIENTE"),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0),
    //             BigIntegerField("fecha_modificacion").default_val(0)
    //         };
    //     }
    // };

    // // 12. Modelo físico de la tabla EventoSistema.
    // class EventoSistemaModel final : public Model {
    // public:
    //     MetaOptions meta() const override { return { "EventoSistema" }; }
    //     std::vector<Field> fields() const override {
    //         return {
    //             AutoField("id_evento_sistema"),
    //             SmallIntegerField("estado_bus").default_val(0),
    //             SmallIntegerField("evento_gestor").default_val(0),
    //             EnumField("criticidad", "criticidad_evento_enum").default_val("BAJA"),
    //             PositiveIntegerField("espacio_sd_libre_kb").default_val(0),
    //             PositiveIntegerField("heap_libre_bytes").default_val(0),
    //             CharField("detalle", 96).default_val(""),
    //             EnumField("notificado_smtp", "estado_envio_correo_enum").default_val("PENDIENTE"),
    //             BooleanField("activa").default_val("TRUE"),
    //             BigIntegerField("fecha_creacion").default_val(0)
    //         };
    //     }
    // };

    // Ejecuta la migración de un modelo individual en su propio frame de pila y reporta el margen libre.
    template <typename TModel>
    inline bool migrateSingleModel() {
        TModel modelInstance;
        return modelInstance.migrate();
    };

    // Ejecuta la migración determinista en orden topológico de dependencias liberando la pila entre cada modelo.
    inline bool migrateAllPhysicalModels() {
        if (!registerAllPostgresEnums()) return false;

        return migrateSingleModel<ConfiguracionRedModel>()
            && migrateSingleModel<UsuarioModel>();
    };

} // namespace DbNotRuntime::PhysicalModels