// DB Firmware inicio

enum EstadoSensor {
  ESTADO_OK
  ESTADO_DESCONEXION_WIRE
  ESTADO_RUIDO
  ESTADO_SUPERO_INFERIOR
  ESTADO_SUPERO_SUPERIOR
}

enum CriticidadEvento {
  CRITICIDAD_BAJA
  CRITICIDAD_MEDIA
  CRITICIDAD_CRITICA
}

enum TipoRecepcionCorreo {
  RECIBIR_TODOS
  RECIBIR_EVENTOS
  RECIBIR_EVENTOS_CRITICOS
}

enum EstadoEnvioCorreo {
  ENVIO_PENDIENTE
  ENVIO_ENVIADO
  ENVIO_FALLIDO
}

enum RolUsuario {
  ROL_REGULAR
  ROL_ADMIN
}

Table calibracion_fw {
  altura float [default: 0]
}

Table sensor_fw {
  hexadecimal        char(17)  [pk]
  id_sensor          uint16    [unique, increment]
  correcion_superior     float     [default: 0.0]
  correcion_inferior     float     [default: 0.0]
  umbral_superior    float     [default: 126.0]
  umbral_inferior    float     [default: -126.0]
  ubicacion             varchar
  ubicacion_descripcion varchar
  nombre                varchar
  descripcion           varchar
  activa             uint8     [default: 1]
  fecha_creacion     uint32
  fecha_modificacion uint32
}

Table senso_fw {
  id_senso              uint32           [pk, increment]
  id_sensor             uint16           [ref: > sensor_fw.id_sensor]
  temperatura_cruda     float
  temperatura_corregida float
  estado_sensor         EstadoSensor
  activa                uint8            [default: 1]
  fecha_creacion        uint32
  fecha_modificacion    uint32
}

Table evento_sensor_fw {
  id_evento_sensor     uint32           [pk, increment]
  id_sensor            uint16           [ref: > sensor_fw.id_sensor]
  id_senso             uint32           [ref: > senso_fw.id_senso]
  estado_previo_sensor EstadoSensor
  evento               EstadoSensor
  criticidad           CriticidadEvento
  activa               uint8            [default: 1]
  fecha_creacion       uint32
  fecha_modificacion   uint32
}

Table auditoria_fw {
  id_auditoria       uint32   [pk, increment]
  id_usuario         uint16   [ref: > usuarios_fw.id_usuario]
  accion             char(24)
  detalle            char(24)
  activa             uint8    [default: 1]
  fecha_creacion     uint32
  fecha_modificacion uint32
}

Table correos_fw {
  id_correo               uint16              [pk, increment]
  nombre                  char(24)
  apellido                char(24)
  correo_electronico      char(48)
  tipo_recepcion_mensajes TipoRecepcionCorreo [default: 'RECIBIR_TODOS']
  estado_envio            EstadoEnvioCorreo   [default: 'ENVIO_PENDIENTE']
  activa                  uint8               [default: 1]
  fecha_creacion          uint32
  fecha_modificacion      uint32
}

Table usuarios_fw {
  id_usuario         uint16     [pk, increment]
  usuario            char(20)   [unique]
  contrasena         blob(32)
  rol                RolUsuario [default: 'ROL_REGULAR']
  activa             uint8      [default: 1]
  fecha_creacion     uint32
  fecha_modificacion uint32
}

Table configuracion_smtp_fw {
  id_configuracion_smtp uint8   [pk]
  host_smtp             char(40)
  puerto                uint16
  usuario               char(40)
  password              char(48)
  email_remitente       char(48)
  activa                uint8   [default: 1]
  fecha_creacion        uint32
  fecha_modificacion    uint32
}

Table servidores_ntp_fw {
  id_server          uint8    [pk, increment]
  server             char(40)
  activa             uint8    [default: 1]
  fecha_creacion     uint32
  fecha_modificacion uint32
}

Table lts_fw {
  id_lts                uint8    [pk]
  nombre_certificado    char(24)
  certificado           char(64)
  clave_privada         blob(32)
  cadena_certificados   char(64)
  fecha_inicio_vigencia uint32
  fecha_fin_vigencia    uint32
  activa                uint8    [default: 1]
  fecha_creacion        uint32
  fecha_modificacion    uint32
}

Table configuracion_conectividad_fw {
  id_configuracion_conectividad uint8   [pk]
  SSID_STA                      char(33)
  STA_password                  char(65)
  SSID_AP                       char(33)
  AP_password                   char(65)
  puerto_tcp                    uint16  [default: 80]
  puerto_udp                    uint16  [default: 81]
  baudios_usb                   int     [default: 115200]
  activa                        uint8   [default: 1]
  fecha_creacion                uint32
  fecha_modificacion            uint32
}

// DB Firmware FIN