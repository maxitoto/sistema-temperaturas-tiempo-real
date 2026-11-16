#pragma once
#include "DjangoModel.hpp"

namespace DbNotRuntime::PhysicalModels {

    using namespace DbNotRuntime::Models;

    template <typename TEnum>
    inline bool registerTypedEnum() {
        return PgEnumChoices::registerChoices(
            DbCommon::Dto::PgEnumTraits<TEnum>::typeName(),
            DbCommon::Dto::PgEnumTraits<TEnum>::labels()
        );
    }

    inline bool registerAllPostgresEnums() {
        return registerTypedEnum<RolUsuario>()
            && registerTypedEnum<CriticidadEvento>()
            && registerTypedEnum<TipoEvento>()
            && registerTypedEnum<TipoRecepcionCorreo>()
            && registerTypedEnum<EstadoEnvioCorreo>()
            && registerTypedEnum<MedioConexion>()
            && registerTypedEnum<ResultadoAuditoria>()
            && registerTypedEnum<FuenteTiempo>()
            && registerTypedEnum<EstadoSensor>()
            && registerTypedEnum<EstadoBus>();
    }

    class ConfiguracionRedModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "ConfiguracionRed";
            m.db_tablespace = "tbs_sistema";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            return m;
        }

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

    class ConfiguracionSmtpModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "ConfiguracionSmtp";
            m.db_tablespace = "tbs_sistema";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                IntegerField("id_configuracion_smtp")
                    .primary_key(true)
                    .default_val(1)
                    .check(
                        "id_configuracion_smtp = 1",
                        +[](const String& v) -> bool { return v.toInt() == 1; }
                    ),
                CharField("host_smtp", 40).default_val(""),
                PositiveIntegerField("puerto").default_val(465),
                BooleanField("usar_tls").default_val("TRUE"),
                CharField("usuario", 48).default_val(""),
                CharField("password_cifrado", 160).default_val(""),
                CharField("email_remitente", 48).default_val(""),
                CharField("nombre_remitente", 32).default_val("Laboratorio UNTDF"),
                BooleanField("activa").default_val("TRUE"),
                DateTimeField("fecha_creacion"),
                DateTimeField("fecha_modificacion").auto_now(true)
            };
        }
    };

    class CalibracionModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "Calibracion";
            m.db_tablespace = "tbs_sistema";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                IntegerField("id_calibracion")
                    .primary_key(true)
                    .default_val(1)
                    .check(
                        "id_calibracion = 1",
                        +[](const String& v) -> bool { return v.toInt() == 1; }
                    ),
                FloatField("altura")
                    .default_val(115.0f)
                    .check(
                        "altura >= -500.0 AND altura <= 9000.0",
                        +[](const String& v) -> bool {
                            const float a = v.toFloat();
                            return a >= -500.0f && a <= 9000.0f;
                        }
                    ),
                FloatField("presion_atmosferica")
                    .default_val(1013.25f)
                    .check(
                        "presion_atmosferica >= 300.0 AND presion_atmosferica <= 1100.0",
                        +[](const String& v) -> bool {
                            const float p = v.toFloat();
                            return p >= 300.0f && p <= 1100.0f;
                        }
                    ),
                FloatField("temp_congelacion_ref").default_val(0.0f),
                FloatField("temp_ebullicion_ref").default_val(100.0f),
                BooleanField("activa").default_val("TRUE"),
                BigIntegerField("fecha_creacion").default_val(0),
                BigIntegerField("fecha_modificacion").default_val(0)
            };
        }
    };

    class ServidoresNtpModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "ServidoresNtp";
            m.db_tablespace = "tbs_sistema";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_ntp_activa_prioridad", {"activa", "prioridad"}, false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_server"),
                CharField("server", 40).unique(true),
                SmallIntegerField("prioridad")
                    .default_val(1)
                    .check(
                        "prioridad >= 1 AND prioridad <= 10",
                        +[](const String& v) -> bool {
                            const int p = v.toInt();
                            return p >= 1 && p <= 10;
                        }
                    ),
                BooleanField("activa").default_val("TRUE"),
                BigIntegerField("fecha_creacion").default_val(0),
                BigIntegerField("fecha_modificacion").default_val(0)
            };
        }
    };

    class ConfiguracionSistemaModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "ConfiguracionSistema";
            m.db_tablespace = "tbs_sistema";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                IntegerField("id_configuracion_sistema")
                    .primary_key(true)
                    .default_val(1)
                    .check(
                        "id_configuracion_sistema = 1",
                        +[](const String& v) -> bool { return v.toInt() == 1; }
                    ),
                IntegerField("zona_horaria_min").default_val(-180),
                PositiveIntegerField("retencion_senso_dias").default_val(365),
                PositiveIntegerField("retencion_eventos_dias").default_val(730),
                PositiveIntegerField("retencion_auditoria_dias").default_val(1095),
                PositiveIntegerField("sd_libre_minimo_kb").default_val(51200),
                PositiveIntegerField("correo_cooldown_s").default_val(900),
                BooleanField("activa").default_val("TRUE"),
                BigIntegerField("fecha_modificacion").default_val(0)
            };
        }
    };

    class UsuarioModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "Usuario";
            m.db_tablespace = "tbs_seguridad";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_usuario_activa", {"activa"}, false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_usuario"),
                CharField("usuario", 20).unique(true),
                CharField("salt", 32).default_val("SALT"),
                TextField("contrasena_hash", 64),
                EnumField<RolUsuario>("rol").default_val(RolUsuario::NINGUNO),
                SmallIntegerField("intentos_fallidos")
                    .check(
                        "intentos_fallidos >= 0 AND intentos_fallidos <= 3",
                        +[](const String& v) -> bool {
                            const int n = v.toInt();
                            return n >= 0 && n <= 3;
                        }
                    )
                    .default_val(0),
                BooleanField("bloqueado").default_val("FALSE"),
                BigIntegerField("bloqueado_desde").null(true).default_val(0),
                BigIntegerField("bloqueado_hasta").null(true).default_val(0),
                BigIntegerField("fecha_ultimo_login").null(true).default_val(0),
                BigIntegerField("fecha_cambio_password").null(true).default_val(0),
                BooleanField("activa").default_val("TRUE"),
                DateTimeField("creado_el"),
                DateTimeField("actualizado_el").auto_now(true)
            };
        }
    };

    class SensorModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "Sensor";
            m.db_tablespace = "tbs_sensores";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_sensor_activo",    {"activa"},    false),
                Index("idx_sensor_ubicacion", {"ubicacion"}, false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_sensor"),
                FixedCharField("mac", 16).unique(true),
                TextField("nombre", 32).default_val(""),
                TextField("ubicacion", 32).default_val(""),
                TextField("descripcion", 128).default_val(""),
                FloatField("umbral_sup")
                    .default_val(126.0f)
                    .check(
                        "umbral_sup >= -55.0 AND umbral_sup <= 126.0",
                        +[](const String& v) -> bool {
                            const float t = v.toFloat();
                            return t >= -55.0f && t <= 126.0f;
                        }
                    ),
                FloatField("umbral_inf")
                    .default_val(-55.0f)
                    .check(
                        "umbral_inf >= -55.0 AND umbral_inf <= 126.0",
                        +[](const String& v) -> bool {
                            const float t = v.toFloat();
                            return t >= -55.0f && t <= 126.0f;
                        }
                    ),
                FloatField("error_sup")
                    .default_val(0.0f)
                    .check(
                        "error_sup >= -55.0 AND error_sup <= 126.0",
                        +[](const String& v) -> bool {
                            const float e = v.toFloat();
                            return e >= -55.0f && e <= 126.0f;
                        }
                    ),
                FloatField("error_inf")
                    .default_val(0.0f)
                    .check(
                        "error_inf >= -55.0 AND error_inf <= 126.0",
                        +[](const String& v) -> bool {
                            const float e = v.toFloat();
                            return e >= -55.0f && e <= 126.0f;
                        }
                    ),
                FloatField("offset")
                    .default_val(0.0f)
                    .check(
                        "offset >= -20.0 AND offset <= 20.0",
                        +[](const String& v) -> bool {
                            const float o = v.toFloat();
                            return o >= -20.0f && o <= 20.0f;
                        }
                    ),
                BooleanField("activa").default_val("TRUE"),
                DateTimeField("fecha_creacion"),
                DateTimeField("fecha_modificacion").auto_now(true)
            };
        }
    };

    class SensoModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "Senso";
            m.db_tablespace = "tbs_sensores";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_senso_sensor_fecha", {"id_sensor", "fecha_creacion"}, false),
                Index("idx_senso_fecha",        {"fecha_creacion"},              false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_senso"),
                ForeignKey("id_sensor", "Sensor", "id_sensor", RESTRICT),
                FloatField("temperatura_cruda")
                    .default_val(0.0f)
                    .check(
                        "temperatura_cruda >= -55.0 AND temperatura_cruda <= 126.0",
                        +[](const String& v) -> bool {
                            const float t = v.toFloat();
                            return t >= -55.0f && t <= 126.0f;
                        }
                    ),
                FloatField("temperatura_corregida")
                    .default_val(0.0f)
                    .check(
                        "temperatura_corregida >= -55.0 AND temperatura_corregida <= 126.0",
                        +[](const String& v) -> bool {
                            const float t = v.toFloat();
                            return t >= -55.0f && t <= 126.0f;
                        }
                    ),
                EnumField<EstadoSensor>("estado_sensor").default_val(EstadoSensor::OK),
                BigIntegerField("fecha_creacion").default_val(0)
            };
        }
    };

    class AuditoriaSensoresModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "AuditoriaSensores";
            m.db_tablespace = "tbs_auditoria";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_aud_sensor_fecha",      {"id_sensor", "fecha_creacion"}, false),
                Index("idx_aud_sensor_reporte",    {"estado_reporte"},              false),
                Index("idx_aud_sensor_criticidad", {"criticidad_sensor"},           false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_auditoria_sensor"),
                ForeignKey("id_sensor", "Sensor", "id_sensor", RESTRICT),
                ForeignKey("id_senso", "Senso", "id_senso", CASCADE),
                EnumField<EstadoSensor>("estado_previo_sensor").default_val(EstadoSensor::OK),
                EnumField<EstadoSensor>("estado_nuevo_sensor").default_val(EstadoSensor::ALARMA_SUPERIOR),
                EnumField<EstadoBus>("estado_bus").default_val(EstadoBus::OK),
                EnumField<TipoEvento>("tipo_evento_sensor").default_val(TipoEvento::ADVERTENCIA),
                EnumField<CriticidadEvento>("criticidad_sensor").default_val(CriticidadEvento::ALTA),
                EnumField<EstadoEnvioCorreo>("estado_reporte").default_val(EstadoEnvioCorreo::PENDIENTE),
                BooleanField("activa").default_val("TRUE"),
                DateTimeField("fecha_creacion")
            };
        }
    };

    class AuditoriaSistemaModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "AuditoriaSistema";
            m.db_tablespace = "tbs_auditoria";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_aud_sis_fecha",      {"fecha_creacion"}, false),
                Index("idx_aud_sis_reporte",    {"estado_reporte"}, false),
                Index("idx_aud_sis_criticidad", {"criticidad"},     false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_auditoria_sistema"),
                EnumField<CriticidadEvento>("criticidad").default_val(CriticidadEvento::NULA),
                EnumField<TipoEvento>("tipo_evento_sistema").default_val(TipoEvento::INFORMATIVO),
                EnumField<EstadoBus>("estado_bus").default_val(EstadoBus::OK),
                PositiveIntegerField("espacio_sd_libre_kb").default_val(0),
                PositiveIntegerField("heap_libre_bytes").default_val(0),
                CharField("reset_reason", 24).default_val(""),
                CharField("version_firmware", 16).default_val("1.0.0"),
                CharField("detalle", 96).default_val(""),
                BooleanField("internet").default_val("FALSE"),
                EnumField<EstadoEnvioCorreo>("estado_reporte").default_val(EstadoEnvioCorreo::PENDIENTE),
                PositiveIntegerField("boot_id").default_val(0),
                PositiveIntegerField("uptime_s").default_val(0),
                EnumField<FuenteTiempo>("fuente_tiempo").default_val(FuenteTiempo::SIN_SINCRONIZAR),
                BooleanField("activa").default_val("TRUE"),
                DateTimeField("fecha_creacion")
            };
        }
    };

    class CorreosModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "Correos";
            m.db_tablespace = "tbs_sistema";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_correos_activa", {"activa"}, false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_correo"),
                CharField("nombre", 24),
                CharField("apellido", 24).default_val(""),
                CharField("correo_electronico", 48).unique(true),
                EnumField<TipoRecepcionCorreo>("tipo_recepcion_mensajes").default_val(TipoRecepcionCorreo::RECIBIR_TODOS),
                EnumField<EstadoEnvioCorreo>("ultimo_estado_envio").default_val(EstadoEnvioCorreo::PENDIENTE),
                BooleanField("activa").default_val("TRUE"),
                DateTimeField("fecha_creacion"),
                DateTimeField("fecha_modificacion").auto_now(true)
            };
        }
    };

    class AuditoriaAuthModel final : public Model {
    public:
        MetaOptions meta() const override {
            MetaOptions m;
            m.db_table      = "AuditoriaAuth";
            m.db_tablespace = "tbs_auditoria";
            m.persistence   = Standard::TablePersistence::PERMANENT;
            m.indexes       = {
                Index("idx_aud_auth_fecha",         {"fecha_creacion"},               false),
                Index("idx_aud_auth_usuario_fecha", {"id_usuario", "fecha_creacion"}, false)
            };
            return m;
        }

        std::vector<Field> fields() const override {
            return {
                AutoField("id_auditoria_auth"),
                PositiveBigIntegerField("seq").unique(true),
                ForeignKey("id_usuario", "Usuario", "id_usuario", RESTRICT).null(true),
                CharField("usuario_intentado", 20).default_val(""),
                CharField("accion", 32),
                CharField("tabla_afectada", 32).default_val("NINGUNA"),
                CharField("valor_previo", 64).default_val(""),
                CharField("valor_nuevo", 64).default_val(""),
                CharField("detalle", 120).default_val(""),
                EnumField<MedioConexion>("medio_conexion").default_val(MedioConexion::INTERNO),
                CharField("origen_remoto", 46).default_val("LOCAL"),
                EnumField<ResultadoAuditoria>("resultado").default_val(ResultadoAuditoria::EXITO),
                EnumField<CriticidadEvento>("criticidad").default_val(CriticidadEvento::BAJA),
                PositiveIntegerField("boot_id").default_val(0),
                PositiveIntegerField("uptime_s").default_val(0),
                EnumField<FuenteTiempo>("fuente_tiempo").default_val(FuenteTiempo::SIN_SINCRONIZAR),
                BigIntegerField("fecha_creacion").default_val(0),
                CharField("hash_previo", 64),
                CharField("hash_registro", 64)
            };
        }
    };

    template <typename TModel>
    inline bool migrateSingleModel() {
        TModel modelInstance;
        return modelInstance.migrate();
    }

    inline bool migrateAllPhysicalModels() {
        if (!registerAllPostgresEnums()) return false;

        return migrateSingleModel<ConfiguracionRedModel>()
            && migrateSingleModel<ConfiguracionSmtpModel>()
            && migrateSingleModel<CalibracionModel>()
            && migrateSingleModel<ServidoresNtpModel>()
            && migrateSingleModel<ConfiguracionSistemaModel>()
            && migrateSingleModel<UsuarioModel>()
            && migrateSingleModel<SensorModel>()
            && migrateSingleModel<SensoModel>()
            && migrateSingleModel<AuditoriaSensoresModel>()
            && migrateSingleModel<AuditoriaSistemaModel>()
            && migrateSingleModel<CorreosModel>()
            && migrateSingleModel<AuditoriaAuthModel>();
    }

}