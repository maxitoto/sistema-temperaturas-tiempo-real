#include "SensorController.hpp"
#include "AlarmController.hpp"
#include "../database/AuditService.hpp"

namespace Controllers {

// =============================================================
// SETUP
// =============================================================
void SensorController::onSetup() {
    Audit::asegurarUsuarioSistema();
    Time::timeSrv.inicializarServidoresPorDefecto();
    Time::timeSrv.sincronizarConFailoverNtp();
    db.setSession("sensor_boot", ROL_ADMIN);

    uint8_t asignados = gestorSensores.escanearYAsignarSensores(12, false);

    sincronizarCatalogoDB();
    actualizarSnapshot();

    {
        Rtos::LockGuard lock(Rtos::sysBus.dbMutex, 2000);
        if (lock.isLocked()) {
            guardarEventoSistema(EventoGestor::BUS_INICIALIZADO, "Arranque del sistema");
        }
    }

    Audit::registrar(
        "SYSTEM_BOOT", "Sensor",
        "0", String(asignados),
        String("Arranque completo. Sondas 1-Wire activas: ") + String(asignados),
        CriticidadEvento::CRITICIDAD_BAJA
    );

    ultimoSensoMs_ = millis();
}

// =============================================================
// LOOP
// =============================================================
void SensorController::onStep(bool /*woken*/) {

    // ---------- 1. Hot-plug cada 2 s ----------
    if (gestorSensores.verificarCambiosEnCaliente()) {
        uint8_t previos = gestorSensores.obtenerTotalAsignados();
        EventoGestor ev = gestorSensores.obtenerUltimoEvento();

        uint8_t nuevos = gestorSensores.escanearYAsignarSensores(12, false);
        sincronizarCatalogoDB();

        // Sonido diferenciado según se haya conectado o desconectado un sensor
        if (nuevos > previos || ev == EventoGestor::NUEVO_SENSOR_DETECTADO) {
            alarmController.dispararSonidoEvento(SonidoEvento::SENSOR_CONECTADO);
        } else if (nuevos < previos || ev == EventoGestor::SENSOR_DESCONECTADO_EN_CALIENTE) {
            alarmController.dispararSonidoEvento(SonidoEvento::SENSOR_DESCONECTADO);
        }

        {
            Rtos::LockGuard lock(Rtos::sysBus.dbMutex, 2000);
            if (lock.isLocked()) {
                guardarEventoSistema(ev, "Cambio de topologia en bus 1-Wire");
            }
        }

        Audit::registrar(
            (nuevos >= previos) ? "SENSOR_HOTPLUG_IN" : "SENSOR_HOTPLUG_OUT",
            "Sensor",
            String(previos),
            String(nuevos),
            String("Evento 1-Wire: ") + GestorSensores::nombreEventoGestor(ev),
            (nuevos < previos) ? CriticidadEvento::CRITICIDAD_MEDIA : CriticidadEvento::CRITICIDAD_BAJA
        );

        gestorSensores.consumirEvento();
        notificarTareas();
        actualizarSnapshot();
    }

    // ---------- 2. Refrescar RAM (OLED / alarmas leen de acá) ----------
    gestorSensores.actualizarTodosLosSlots();

    // Avisar al AlarmController que hay una muestra fresca para su filtro de confirmación
    alarmController.notificarEvaluacionSensores();
    Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_SENSORES_ACTUALIZADOS);

    // ---------- 3. Detectar transiciones confirmadas → Senso + EventoSensor ----------
    detectarTransicionesYGuardar();

    // ---------- 4. Persistencia periódica cada 2 min ----------
    uint32_t ahora = millis();
    if (ahora - ultimoSensoMs_ >= SENSO_PERSIST_INTERVAL_MS) {
        guardarSensoPeriodico();
        ultimoSensoMs_ = ahora;
    }
}

// =============================================================
// SYNC CATÁLOGO con DB
// =============================================================
void SensorController::sincronizarCatalogoDB() {
    Rtos::LockGuard lock(Rtos::sysBus.dbMutex, 3000);
    if (!lock.isLocked()) return;

    for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
        Sensor& s = gestorSensores.obtenerSensor(i);
        if (!s.estaActivo()) continue;

        String mac = String(s.obtenerMAC());
        auto filas = db.from("Sensor").select("*").eq("hexadecimal", mac).execute();

        if (!filas.empty()) {
            const auto& f = filas[0];
            float sup = f.at("umbral_superior").toFloat();
            float inf = f.at("umbral_inferior").toFloat();
            float off = f.at("offset_calibracion").toFloat();

            gestorSensores.configurarOffsetSlot(i, off);
            s.setCorrecciones(f.at("correcion_superior").toFloat(),
                              f.at("correcion_inferior").toFloat());
            gestorSensores.configurarAlarmasSlot(i, sup, inf, false);
        } else {
            uint32_t ts = Time::nowUnix();
            db.from("Sensor").insert({
                {"hexadecimal",           mac},
                {"mac2",                  String(s.obtenerMAC2())},
                {"nombre",                "Sensor " + String(i + 1)},
                {"descripcion",           ""},
                {"ubicacion",             "Sin asignar"},
                {"ubicacion_descripcion", ""},
                {"offset_calibracion",    "0.0"},
                {"correcion_superior",    "0.0"},
                {"correcion_inferior",    "0.0"},
                {"umbral_superior",       String(Sensor::UMBRAL_MAX_DEFAULT, 2)},
                {"umbral_inferior",       String(Sensor::UMBRAL_MIN_DEFAULT, 2)},
                {"resolucion_bits",       String(s.obtenerResolucionBits())},
                {"alimentacion_parasita", s.esAlimentacionParasita() ? "1" : "0"},
                {"estado_actual",         String((int)s.obtenerEstado())},
                {"fecha_creacion",        String(ts)},
                {"fecha_modificacion",    String(ts)}
            });
        }
    }
}

// =============================================================
// TRANSICIONES → Senso + EventoSensor (Con filtro de 2 muestras)
// =============================================================
void SensorController::detectarTransicionesYGuardar() {
    struct Cambio {
        uint8_t      idx;
        EstadoSensor antes;
        EstadoSensor ahora;
    };
    Cambio cambios[GestorSensores::MAX_SENSORES];
    int nCambios = 0;

    for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
        const Sensor& s = gestorSensores.obtenerSensor(i);
        bool ahoraActivo = s.estaActivo();

        if (ahoraActivo != snapshot_[i].activo) {
            snapshot_[i].activo = ahoraActivo;
            snapshot_[i].estabilidad = 0;
            if (ahoraActivo) strncpy(snapshot_[i].mac, s.obtenerMAC(), 16);
            continue;
        }
        if (!ahoraActivo) continue;

        EstadoSensor ahoraEstado = s.obtenerEstado();
        if (ahoraEstado == snapshot_[i].estado) {
            snapshot_[i].estabilidad = 0;
            continue;
        }

        // Exigir al menos 2 lecturas consecutivas en el nuevo estado antes de volcar un EventoSensor en SD
        snapshot_[i].estabilidad++;
        if (snapshot_[i].estabilidad < 2) continue;

        cambios[nCambios++] = {i, snapshot_[i].estado, ahoraEstado};
        snapshot_[i].estado = ahoraEstado;
        snapshot_[i].estabilidad = 0;
    }

    if (nCambios == 0) return;

    Rtos::LockGuard lock(Rtos::sysBus.dbMutex, 5000);
    if (!lock.isLocked()) return;

    std::map<String, int> idsPorMac;
    {
        auto todos = db.from("Sensor").select("id_sensor,hexadecimal").execute();
        for (const auto& f : todos)
            idsPorMac[f.at("hexadecimal")] = f.at("id_sensor").toInt();
    }

    uint32_t ts = Time::nowUnix();

    for (int k = 0; k < nCambios; k++) {
        const auto& c = cambios[k];
        const Sensor& s = gestorSensores.obtenerSensor(c.idx);
        String mac = String(s.obtenerMAC());

        if (!idsPorMac.count(mac)) continue;
        int idSensor = idsPorMac[mac];

        float tCorr  = s.obtenerTempCelsius();
        float tCruda = tCorr - s.obtenerOffset();

        bool okSenso = db.from("Senso").insert({
            {"id_sensor",             String(idSensor)},
            {"temperatura_cruda",     String(tCruda, 2)},
            {"temperatura_corregida", String(tCorr, 2)},
            {"estado_sensor",         String(estadoParaSenso(c.ahora))},
            {"activa",                "1"},
            {"fecha_creacion",        String(ts)}
        });
        if (!okSenso) continue;

        auto ultimo = db.from("Senso").select("id_senso")
                        .orderBy("id_senso", SGBD::DESC).limit(1).execute();
        if (ultimo.empty()) continue;
        int idSenso = ultimo[0].at("id_senso").toInt();

        int crit = (c.ahora == EstadoSensor::ALARMA_SUPERIOR ||
                    c.ahora == EstadoSensor::ALARMA_INFERIOR)
                   ? (int)CriticidadEvento::CRITICIDAD_CRITICA
                   : (int)CriticidadEvento::CRITICIDAD_MEDIA;

        db.from("EventoSensor").insert({
            {"id_sensor",            String(idSensor)},
            {"id_senso",             String(idSenso)},
            {"estado_previo_sensor", String((int)c.antes)},
            {"evento",               String((int)c.ahora)},
            {"criticidad",           String(crit)},
            {"notificado_smtp",      String((int)EstadoEnvioCorreo::ENVIO_PENDIENTE)},
            {"activa",               "1"},
            {"fecha_creacion",       String(ts)},
            {"fecha_modificacion",   String(ts)}
        });

        db.from("Sensor").eq("id_sensor", String(idSensor)).update({
            {"estado_actual",      String((int)c.ahora)},
            {"fecha_modificacion", String(ts)}
        });

        notificarTareas();
    }
}

// =============================================================
// SENSO periódico (cada 2 min)
// =============================================================
void SensorController::guardarSensoPeriodico() {
    Rtos::LockGuard lock(Rtos::sysBus.dbMutex, 5000);
    if (!lock.isLocked()) return;

    std::map<String, int> idsPorMac;
    {
        auto todos = db.from("Sensor").select("id_sensor,hexadecimal").execute();
        for (const auto& f : todos)
            idsPorMac[f.at("hexadecimal")] = f.at("id_sensor").toInt();
    }

    uint32_t ts = Time::nowUnix();

    for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
        const Sensor& s = gestorSensores.obtenerSensor(i);
        if (!s.estaActivo()) continue;

        String mac = String(s.obtenerMAC());
        if (!idsPorMac.count(mac)) continue;

        float tCorr  = s.obtenerTempCelsius();
        float tCruda = tCorr - s.obtenerOffset();

        db.from("Senso").insert({
            {"id_sensor",             String(idsPorMac[mac])},
            {"temperatura_cruda",     String(tCruda, 2)},
            {"temperatura_corregida", String(tCorr, 2)},
            {"estado_sensor",         String(estadoParaSenso(s.obtenerEstado()))},
            {"activa",                "1"},
            {"fecha_creacion",        String(ts)}
        });
    }
}

// =============================================================
// EventoSistema (requiere dbMutex tomado)
// =============================================================
void SensorController::guardarEventoSistema(EventoGestor ev, const String& detalle) {
    int crit = (int)CriticidadEvento::CRITICIDAD_MEDIA;
    if (ev == EventoGestor::ERROR_BUS_ELECTRICO)
        crit = (int)CriticidadEvento::CRITICIDAD_CRITICA;
    else if (ev == EventoGestor::NUEVO_SENSOR_DETECTADO ||
             ev == EventoGestor::BUS_INICIALIZADO)
        crit = (int)CriticidadEvento::CRITICIDAD_BAJA;

    uint32_t ts = Time::nowUnix();
    uint32_t sdLibreKb = static_cast<uint32_t>(
        (TarjetaSD::getInstance().espacioTotal() - TarjetaSD::getInstance().espacioUsado()) / 1024ULL
    );

    db.from("EventoSistema").insert({
        {"estado_bus",          String((int)gestorSensores.obtenerEstadoBus())},
        {"evento_gestor",       String((int)ev)},
        {"criticidad",          String(crit)},
        {"espacio_sd_libre_kb", String(sdLibreKb)},
        {"heap_libre_bytes",    String(ESP.getFreeHeap())},
        {"detalle",             detalle},
        {"notificado_smtp",     String((int)EstadoEnvioCorreo::ENVIO_PENDIENTE)},
        {"activa",              "1"},
        {"fecha_creacion",      String(ts)}
    });
}

void SensorController::notificarTareas() {
    Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_SENSORES_ACTUALIZADOS);
    Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_ALARMA_CAMBIO);
    Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_NOTIFICAR_FISICA);
    Rtos::sysBus.eventos.setBits(Rtos::Evt::EVT_NOTIFICAR_CORREOS);
}

void SensorController::actualizarSnapshot() {
    for (uint8_t i = 0; i < GestorSensores::MAX_SENSORES; i++) {
        const Sensor& s = gestorSensores.obtenerSensor(i);
        snapshot_[i].activo = s.estaActivo();
        snapshot_[i].estado = s.obtenerEstado();
        snapshot_[i].estabilidad = 0;
        if (s.estaActivo()) strncpy(snapshot_[i].mac, s.obtenerMAC(), 16);
    }
}

int SensorController::estadoParaSenso(EstadoSensor e) {
    switch (e) {
        case EstadoSensor::OK:
        case EstadoSensor::ALARMA_SUPERIOR:
        case EstadoSensor::ALARMA_INFERIOR:
        case EstadoSensor::ERROR_CRC_SCRATCHPAD:
        case EstadoSensor::DESCONECTADO:
        case EstadoSensor::ALIMENTACION_INSUFICIENTE:
        case EstadoSensor::REINICIO_ENERGIA_85C:
        case EstadoSensor::CORTOCIRCUITO_VDD:
        case EstadoSensor::CORTOCIRCUITO_GND:
        case EstadoSensor::CIRCUITO_ABIERTO:
            return (int)e;
        default:
            return (int)EstadoSensor::DESCONECTADO;
    }
}

} // namespace Controllers