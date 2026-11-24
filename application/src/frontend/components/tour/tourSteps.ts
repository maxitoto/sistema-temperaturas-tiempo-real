import type { Vista } from "../layouts/SidelBar";

export interface PasoTour {
  id: string;
  selector?: string;
  titulo: string;
  subtitulo?: string;
  queEs: string;
  paraQueSirve: string;
  enQueAfecta: string;
  comoSeUsa: string;
  soloAdmin?: boolean;
}

export const NOMBRE_VISTA_TOUR: Record<Vista, string> = {
  dashboard: "Dashboard · Telemetría en Vivo",
  sensores: "Sensores y Censos DS18B20",
  database: "Base de Datos / SQL Embebido",
  auditorias: "Auditorías y Cadena HMAC-64",
  usuarios: "Usuarios y Control RBAC",
  configuracion: "Configuración del Sistema",
  bios: "BIOS / FreeRTOS y Hardware",
};

export const TOUR_POR_VISTA: Record<Vista, PasoTour[]> = {
  dashboard: [
    {
      id: "dash-kpis",
      selector: '[data-tour="dash-kpis"]',
      titulo: "Indicadores Clave del Nodo ESP32",
      subtitulo: "Salud de RAM, MicroSD, Red WiFi y Catálogo",
      queEs:
        "Son 4 tarjetas de estado resumido en vivo con los recursos críticos del microcontrolador.",
      paraQueSirve:
        "Permite detectar de un vistazo falta de memoria Heap, llenado de la tarjeta MicroSD, calidad de señal WiFi (RSSI) y sensores activos.",
      enQueAfecta:
        "Un Heap muy bajo o una MicroSD sin espacio libre comprometen el registro histórico de temperaturas y alertas.",
      comoSeUsa:
        "Cada tarjeta funciona como acceso directo: hacé clic sobre Heap/Uptime para ir a BIOS, sobre MicroSD para ir a Base de Datos, sobre Red para ir a WiFi o sobre Catálogo para abrir Sensores.",
    },
    {
      id: "dash-stream-cards",
      selector: '[data-tour="dash-stream-cards"]',
      titulo: "Tarjetas de Sensores en Tiempo Real",
      subtitulo: "Streaming 1-Wire cada 500 ms (UART-S / UDP)",
      queEs:
        "Lectura instantánea de cada sonda DS18B20 conectada físicamente al bus 1-Wire con su temperatura ya calibrada.",
      paraQueSirve:
        "Monitorea la temperatura real de cada cámara o muestra en tiempo real sin necesidad de consultar la base de datos.",
      enQueAfecta:
        "Refleja en vivo la calibración aplicada a cada sensor; cualquier desvío frente a los umbrales TL/TH dispara la alarma física del equipo.",
      comoSeUsa:
        "Hacé clic sobre la tarjeta de cualquier sensor para saltar directo a su ficha completa, historial y calibración en 'Sensores y Censos'.",
    },
    {
      id: "dash-chart",
      selector: '[data-tour="dash-chart"]',
      titulo: "Curva Multilínea en Vivo",
      subtitulo: "Ventana deslizante de últimas 60 muestras",
      queEs:
        "Gráfico temporal en memoria que superpone la evolución de todos los sensores conectados simultáneamente.",
      paraQueSirve:
        "Ayuda a observar tendencias rápidas, aperturas de puerta, inercia térmica o estabilidad durante ensayos.",
      enQueAfecta:
        "Es solo visualización local en Electron: limpiar la curva no borra ningún dato guardado en la MicroSD del ESP32.",
      comoSeUsa:
        "Pasá el cursor sobre la curva para inspeccionar valores exactos o pulsá 'Limpiar curva' para reiniciar la ventana visual.",
    },
  ],

  sensores: [
    {
      id: "sens-catalogo",
      selector: '[data-tour="sens-catalogo"]',
      titulo: "Catálogo de Sensores DS18B20",
      subtitulo: "Inventario persistido por dirección ROM/MAC",
      queEs:
        "Listado de todas las sondas reconocidas por el ESP32 con su ID interno, dirección MAC de 64 bits, ubicación, umbrales y offset.",
      paraQueSirve:
        "Centraliza la identificación lógica de cada sensor físico aunque cambie su orden en el bus 1-Wire.",
      enQueAfecta:
        "Al seleccionar una fila se cargan abajo su perfil completo, estadísticas históricas, curva temporal y formularios de edición.",
      comoSeUsa:
        "Hacé clic en cualquier fila para inspeccionar ese sensor. Usá 'Solo activos' para ocultar sensores dados de baja y 'Actualizar' para refrescar sin caché.",
    },
    {
      id: "sens-wizard",
      selector: '[data-tour="sens-wizard"]',
      titulo: "Asistente de Calibración (Wizard)",
      subtitulo: "Calibración guiada para 1 sensor o en lote",
      queEs:
        "Un procedimiento paso a paso para calibrar sondas por 2 puntos (hielo + ebullición) o 1 punto (termómetro patrón).",
      paraQueSirve:
        "Permite calibrar correctamente sin conocimientos previos de la fórmula interna, reconstruyendo la lectura cruda real del sensor.",
      enQueAfecta:
        "Actualiza la recta de corrección del sensor sin tocar sus umbrales de alarma TL/TH, y ofrece borrar los censos falsos generados durante los baños térmicos.",
      comoSeUsa:
        "Pulsá el botón amarillo, elegí si calibrás 1 sensor o un lote completo, seguí los pasos de estabilización y confirmá en la pantalla de resumen.",
      soloAdmin: true,
    },
    {
      id: "sens-curva",
      selector: '[data-tour="sens-curva"]',
      titulo: "Curva Histórica y Rango de Fechas",
      subtitulo: "Consultas fragmentadas sobre la MicroSD",
      queEs:
        "Visor gráfico del historial persistido en la tabla Censo para el sensor seleccionado.",
      paraQueSirve:
        "Permite analizar el comportamiento térmico pasado por cantidad de lecturas recientes o entre dos fechas y horas exactas.",
      enQueAfecta:
        "En modo 'Rango de fechas', Electron consulta automáticamente al ESP32 en micro-bloques de 32 registros para reconstruir curvas largas sin saturar la memoria.",
      comoSeUsa:
        "Elegí 'Últimos N' o 'Rango de fechas' (podés usar los atajos 1h, 6h, 24h o 7 días) y pulsá 'Consultar'.",
    },
    {
      id: "sens-umbrales",
      selector: '[data-tour="sens-umbrales"]',
      titulo: "Umbrales de Alarma Térmica (TL / TH)",
      subtitulo: "Protección independiente de la calibración",
      queEs:
        "Formulario exclusivo para definir la temperatura mínima (TL) y máxima (TH) permitida para el sensor seleccionado.",
      paraQueSirve:
        "Controla cuándo el ESP32 activa la sirena acústica, el LED rojo y registra un evento crítico en AuditoriaSensores.",
      enQueAfecta:
        "Se aplica de inmediato en la RAM del ESP32 y en la MicroSD, pero jamás altera los puntos de calibración ni el offset del sensor.",
      comoSeUsa:
        "Ingresá TL y TH en °C (asegurándote de que TL ≤ TH) y pulsá 'Guardar Umbrales de Alarma'.",
      soloAdmin: true,
    },
    {
      id: "sens-calibracion",
      selector: '[data-tour="sens-calibracion"]',
      titulo: "Calibración de Precisión (Manual)",
      subtitulo: "Ajuste fino directo para operadores expertos",
      queEs:
        "Panel de edición directa de los puntos medidos en hielo (E_inf), agua hirviendo (E_sup) y offset lineal.",
      paraQueSirve:
        "Permite cargar valores metrológicos conocidos o corregir un pequeño desvío fijo sin abrir el Wizard.",
      enQueAfecta:
        "Modifica únicamente la fórmula T_corr = m · T_cruda + b del sensor seleccionado; conserva intactos los umbrales de alarma.",
      comoSeUsa:
        "Cargá los valores con coma o punto decimal y pulsá 'Aplicar Calibración al Sensor'. Si tenés dudas, usá el Wizard.",
      soloAdmin: true,
    },
    {
      id: "sens-tabla",
      selector: '[data-tour="sens-tabla"]',
      titulo: "Explorador de Tabla Censo y Picos",
      subtitulo: "Paginación rápida y ranking de extremos",
      queEs:
        "Tabla paginada con los registros crudos vs. corregidos y el estado eléctrico de diagnóstico de cada lectura DS18B20.",
      paraQueSirve:
        "Sirve para auditar mediciones puntuales, detectar fallos CRC/desconexión o encontrar los picos máximos y mínimos.",
      enQueAfecta:
        "La consulta de solo lectura no modifica datos; las herramientas de inserción manual, eliminación y purga (solo ADMIN) escriben directo en el archivo binario Censo.pgb.",
      comoSeUsa:
        "Alterná entre 'Recientes', 'Picos Altos' y 'Picos Bajos', filtrá por sensor con el selector y navegá con las flechas de paginación.",
    },
  ],

  database: [
    {
      id: "db-er",
      selector: '[data-tour="db-er"]',
      titulo: "Diagrama Entidad-Relación (ER)",
      subtitulo: "Esquema físico binario en MicroSD",
      queEs:
        "Mapa visual de las tablas registradas en el catálogo PostgreSQL embebido (/db/pg_catalog.cat) y sus claves foráneas.",
      paraQueSirve:
        "Permite comprender en segundos cómo se relacionan Sensores, Censos, Usuarios, Configuraciones y Auditorías.",
      enQueAfecta:
        "Se sincroniza dinámicamente con el catálogo real del ESP32: si creás o eliminás una tabla por SQL, el diagrama se actualiza.",
      comoSeUsa:
        "Hacé clic en cualquier tabla o etiqueta de clave foránea (FK) para resaltar sus relaciones y abrir sus atributos a la derecha.",
    },
    {
      id: "db-inspector",
      selector: '[data-tour="db-inspector"]',
      titulo: "Inspector de Atributos de Tabla",
      subtitulo: "Tipos PostgreSQL, PK, FK, UNIQUE y NOT NULL",
      queEs:
        "Listado compacto de todas las columnas que conforman la tabla seleccionada en el diagrama ER.",
      paraQueSirve:
        "Muestra los nombres exactos de columna, tipos de datos y restricciones antes de escribir consultas SQL.",
      enQueAfecta:
        "Es información de solo lectura que refleja el layout binario fijo almacenado en el archivo .pgb de la MicroSD.",
      comoSeUsa:
        "Desplazate por la lista para revisar cada campo y copiar sus nombres exactos al editor SQL inferior.",
    },
    {
      id: "db-sql",
      selector: '[data-tour="db-sql"]',
      titulo: "Terminal SQL Interactiva (psql)",
      subtitulo: "Motor DDL / DML / DQL directo sobre el ESP32",
      queEs:
        "Consola SQL administrativa conectada al motor binario PostgreSQL embebido del microcontrolador.",
      paraQueSirve:
        "Permite ejecutar consultas ad-hoc (SELECT, COUNT, SHOW TABLES, \\d tabla) y mantenimiento (INSERT, UPDATE, DELETE, VACUUM, CREATE/ALTER/DROP TABLE).",
      enQueAfecta:
        "Las sentencias DML y DDL modifican permanentemente la base de datos en la MicroSD y devuelven códigos SQLSTATE estándar.",
      comoSeUsa:
        "Elegí uno de los presets rápidos o escribí tu sentencia y pulsá 'Execute' (o el atajo Ctrl + Enter).",
      soloAdmin: true,
    },
  ],

  auditorias: [
    {
      id: "aud-tabs",
      selector: '[data-tour="aud-tabs"]',
      titulo: "Módulos de Trazabilidad y Auditoría",
      subtitulo: "Auth HMAC-64 · Eventos de Sensores · Sistema",
      queEs:
        "Selector que divide el historial de auditoría en 3 bitácoras especializadas del sistema.",
      paraQueSirve:
        "Separa accesos/sesiones de usuarios ('Auth & HMAC-64'), alarmas térmicas ('Eventos de Sensores') y reinicios/salud del firmware ('Sistema & Arranques').",
      enQueAfecta:
        "Cambiar de pestaña consulta únicamente la tabla binaria correspondiente sin saturar la memoria del ESP32.",
      comoSeUsa:
        "Hacé clic en la pestaña que necesites revisar; cada vista cuenta con su propia paginación y filtros.",
    },
    {
      id: "aud-hmac",
      selector: '[data-tour="aud-hmac"]',
      titulo: "Verificación Criptográfica HMAC-64 y Purga",
      subtitulo: "Cadena inmutable tipo ledger",
      queEs:
        "Herramienta que recalcula los hashes encadenados de la bitácora de autenticación para comprobar si algún registro fue alterado.",
      paraQueSirve:
        "Garantiza la integridad forense de logins, bloqueos por fuerza bruta y cambios críticos de configuración.",
      enQueAfecta:
        "La verificación es solo lectura. El botón 'Purgar Auth' borra definitivamente los registros anteriores a la fecha elegida.",
      comoSeUsa:
        "Pulsá 'Verificar Integridad Criptográfica HMAC-64' para auditar la cadena; si todo coincide verás el indicador verde 'CADENA ÍNTEGRA'.",
      soloAdmin: true,
    },
    {
      id: "aud-contenido",
      selector: '[data-tour="aud-contenido"]',
      titulo: "Registro Detallado y Paginación",
      subtitulo: "Inspección de bloques y gestión de reportes",
      queEs:
        "Tabla cronológica inversa con los eventos de la bitácora activa.",
      paraQueSirve:
        "Permite revisar quién ejecutó cada acción, desde qué medio (UART-S o WiFi), qué sensor entró en alarma o el motivo del último reinicio del ESP32.",
      enQueAfecta:
        "En la pestaña Auth (ADMIN), hacer clic en una fila abre el inspector de hashes HMAC previo y actual sin alterar el registro.",
      comoSeUsa:
        "Usá las flechas izquierda/derecha para recorrer páginas históricas o el ícono de ojo para inspeccionar el bloque completo.",
    },
  ],

  usuarios: [
    {
      id: "usr-perfil",
      selector: '[data-tour="usr-perfil"]',
      titulo: "Mi Perfil y Cambio de Contraseña",
      subtitulo: "Sesión autenticada L2",
      queEs:
        "Resumen de tu cuenta actual (usuario, rol RBAC y estado) junto al formulario de cambio de clave.",
      paraQueSirve:
        "Permite a cualquier usuario autenticado actualizar su propia contraseña de forma segura con salt y hash en el ESP32.",
      enQueAfecta:
        "La nueva contraseña se persiste en Usuario.pgb y rige desde el próximo inicio de sesión.",
      comoSeUsa:
        "Ingresá tu clave actual (opcional si sos ADMIN), escribí la nueva clave dos veces y pulsá 'Actualizar Mi Contraseña'.",
    },
    {
      id: "usr-alta",
      selector: '[data-tour="usr-alta"]',
      titulo: "Alta de Nuevos Usuarios",
      subtitulo: "Creación de cuentas con rol RBAC",
      queEs:
        "Formulario administrativo para dar de alta operadores o administradores en la base de datos del ESP32.",
      paraQueSirve:
        "Permite asignar cuentas individuales con trazabilidad propia en la bitácora de auditoría.",
      enQueAfecta:
        "El rol elegido define los permisos en el firmware: NINGUNO (sin acceso), NO_ADMIN (solo lectura operativa) o ADMIN (control total).",
      comoSeUsa:
        "Definí nombre de usuario, contraseña inicial (mín. 4 caracteres) y rol RBAC, luego pulsá 'Crear Usuario'.",
      soloAdmin: true,
    },
    {
      id: "usr-tabla",
      selector: '[data-tour="usr-tabla"]',
      titulo: "Administración de Cuentas y Desbloqueo",
      subtitulo: "Control de roles, estado activo y fuerza bruta",
      queEs:
        "Tabla de control de todas las cuentas registradas con su contador de intentos fallidos y estado de bloqueo.",
      paraQueSirve:
        "Permite promover/degradar roles, activar o suspender cuentas, resetear claves olvidadas y desbloquear usuarios bloqueados tras 3 intentos fallidos.",
      enQueAfecta:
        "Los cambios se aplican al instante en el microcontrolador. La cuenta raíz 'admin' está protegida contra eliminación y degradación.",
      comoSeUsa:
        "Cambiá el rol desde el desplegable, alterná ACTIVO/INACTIVO con un clic o usá los botones de llave y candado a la derecha.",
      soloAdmin: true,
    },
  ],

  configuracion: [
    {
      id: "cfg-tabs",
      selector: '[data-tour="cfg-tabs"]',
      titulo: "Categorías de Configuración Global",
      subtitulo: "Sistema · Ambiente · WiFi · SMTP · NTP",
      queEs:
        "Barra de navegación interna que agrupa todos los parámetros globales del nodo ESP32.",
      paraQueSirve:
        "Organiza por separado las políticas de retención MicroSD, la calibración ambiental global, la red inalámbrica, el correo SMTP y los servidores de hora NTP.",
      enQueAfecta:
        "Permite mantener cada subsistema ordenado; los operadores ven las pestañas permitidas en solo lectura y los administradores pueden editar todo.",
      comoSeUsa:
        "Seleccioná la pestaña del subsistema que querés ajustar; si llegaste desde Dashboard o Sensores se abre automáticamente en la sección correspondiente.",
    },
    {
      id: "cfg-panel",
      selector: '[data-tour="cfg-panel"]',
      titulo: "Panel de Parámetros Activo",
      subtitulo: "Persistencia inmediata en la MicroSD",
      queEs:
        "Formulario de configuración correspondiente a la pestaña seleccionada.",
      paraQueSirve:
        "Configura zona horaria, retención automática, altura/presión para calibración de 2 puntos, credenciales STA/SoftAP, alertas por email o sincronización horaria.",
      enQueAfecta:
        "Al guardar se actualiza la tabla en el ESP32 y el cambio impacta al instante (por ejemplo, cambiar WiFi reconecta la placa; cambiar Ambiente actualiza las referencias de calibración).",
      comoSeUsa:
        "Editá los campos habilitados (los numéricos aceptan coma o punto decimal) y pulsá el botón 'Guardar' correspondiente.",
    },
  ],

  bios: [
    {
      id: "bios-esp",
      selector: '[data-tour="bios-esp"]',
      titulo: "Telemetría Nativa del Silicio ESP32",
      subtitulo: "CPU, Flash, Reset y distribución de RAM Heap",
      queEs:
        "Panel de diagnóstico de bajo nivel leído directamente de los registros hardware del ESP32.",
      paraQueSirve:
        "Muestra el motivo del último reinicio, uptime, memoria Heap libre, bloque contiguo máximo (clave contra fragmentación) e incidentes del supervisor BIOS.",
      enQueAfecta:
        "Permite detectar fugas de memoria o reinicios por brownout/watchdog antes de que afecten la operación en campo.",
      comoSeUsa:
        "Revisá que 'Max Bloque' se mantenga saludable y usá 'Actualizar Todo' en la esquina superior derecha para tomar una muestra fresca.",
    },
    {
      id: "bios-rtos",
      selector: '[data-tour="bios-rtos"]',
      titulo: "Orquestador de Tareas FreeRTOS en Vivo",
      subtitulo: "Núcleos Core 0 / Core 1, Stack y Latidos",
      queEs:
        "Monitor en tiempo real de las 5 tareas FreeRTOS del firmware (BIOS, Comms, AlertasDB, Sensores y OLED).",
      paraQueSirve:
        "Controla el margen libre de stack de cada tarea, su tiempo máximo de ejecución en microsegundos y cuántas veces debió resucitarla la BIOS.",
      enQueAfecta:
        "Si una tarea se queda sin stack o supera su tiempo de latido, TaskBiosOS la reinicia en caliente para evitar que el equipo quede colgado.",
      comoSeUsa:
        "Activá o pausá el checkbox 'Polling cada 2 s' para observar el comportamiento de cada núcleo en vivo.",
    },
    {
      id: "bios-log",
      selector: '[data-tour="bios-log"]',
      titulo: "Caja Negra BIOS (/db/help.log)",
      subtitulo: "Registro de incidentes críticos sobreviviente a resets",
      queEs:
        "Visor de los últimos registros volcados desde la memoria RTC hacia la tarjeta MicroSD ante bloqueos graves.",
      paraQueSirve:
        "Permite diagnosticar qué tarea o mutex originó una intervención del supervisor BIOS incluso después de un reinicio.",
      enQueAfecta:
        "Es una lectura directa del archivo /db/help.log; no modifica ni borra el historial.",
      comoSeUsa:
        "Consultá esta consola ante cualquier reinicio inesperado o pitidos múltiples de la placa.",
    },
    {
      id: "bios-hw",
      selector: '[data-tour="bios-hw"]',
      titulo: "Banco de Pruebas y Control de Periféricos",
      subtitulo: "Diagnóstico físico de LED, Buzzer, OLED y 1-Wire",
      queEs:
        "Panel de comando directo sobre los drivers físicos del nodo ESP32.",
      paraQueSirve:
        "Permite verificar que el LED de alarma, el buzzer PWM, la pantalla OLED I2C y el bus 1-Wire respondan correctamente, además de reiniciar el equipo por software.",
      enQueAfecta:
        "Cada acción actúa físicamente sobre la placa de inmediato. 'Reiniciar ESP32' cerrará la conexión activa por unos segundos.",
      comoSeUsa:
        "Usá los botones de prueba rápida, configurá frecuencia/duración del buzzer o enviá un mensaje de prueba a la pantalla OLED.",
    },
  ],
};