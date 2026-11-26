#include <esp_system.h>
#include "database/sql/SchemaPostgreSQL.hpp"
#include "database/repositories/repoConfiguracionSistema.hpp"
#include "database/repositories/repoCalibracion.hpp"
#include "database/repositories/repoServidorNtp.hpp"
#include "database/repositories/repoConfiguracionSmtp.hpp"
#include "database/repositories/repoConfiguracionRed.hpp"
#include "database/repositories/repoAuditoriaSistema.hpp"
#include "database/repositories/repoUsuario.hpp"
#include "hardware/driver/tarjetasd/TarjetaSD.hpp"
#include "hardware/driver/led/Led.hpp"
#include "hardware/driver/buzz/Buzz.hpp"
#include "hardware/driver/gestorsensores/GestorSensores.hpp"
#include "rtos/GestorRTOS.hpp"

// Instancias globales de transporte (L0/L1 físico)
UARTSRuntime uartsRuntime(Serial);
WiFiInicializador wifiInicializador;
WiFiRuntime wifiRuntime(wifiInicializador);
WiFiTcpTlsRuntime wifiTcpRuntime;
WiFiRelayClient wifiRelayClient;
WiFiUdpPlainRuntime wifiUdpRuntime;

// Instancias globales de los 3 dominios de middlewares (L3, L2 y L1)
ColaTrabajoRPC colaTrabajoRpc;

MiddlewareMessage middlewareMessageL3;
MiddlewareSesion middlewareSesionL2(middlewareMessageL3);
MiddlewareInterface middlewareInterfaceL1(
    uartsRuntime,
    wifiRuntime,
    wifiTcpRuntime,
    wifiUdpRuntime,
    colaTrabajoRpc,
    middlewareSesionL2,
    middlewareMessageL3
);

void setup() {
    // 1. Vincular y verificar existencia del catálogo físico en MicroSD; si falla tras reintentar, reiniciar limpiamente
    if (!TarjetaSD::getInstance().iniciar() ||
        !DbSql::Schema::bindOrBootstrapDatabase() ||
        DbCommon::Engine::PhysicalSchemaCatalog::getInstance().getAllTables().empty()) {
        led.encenderLed();
        buzz.sonarBuzz(800, 1.5f);
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP.restart();
    }

    // 2. Inicializar configuraciones base y asegurar que el usuario 'admin' exista y no haya quedado corrupto
    try {
        DbRuntime::Repositories::UsuarioDTO adminDto{};
        if (!DbRuntime::Repositories::RepoUsuario::obtenerPorNombreUsuario("admin", adminDto)) {
            DbRuntime::Repositories::RepoUsuario::crearUsuario("admin", "admin", RolUsuario::ADMIN);
        } else if (adminDto.bloqueado) {
            DbRuntime::Repositories::RepoUsuario::desbloquearPorNombre("admin");
        }
    } catch (...) {}

    DbRuntime::Repositories::ConfiguracionSistemaDTO cfgSis{};
    cfgSis.zonaHorariaMin = -180;
    try {
        DbRuntime::Repositories::RepoConfiguracionSistema::obtenerOInicializar(cfgSis);
    } catch (...) {}

    try {
        DbRuntime::Repositories::CalibracionDTO cfgCal{};
        DbRuntime::Repositories::RepoCalibracion::obtenerOInicializar(cfgCal);
    } catch (...) {}

    DbRuntime::Repositories::ConfiguracionRedDTO cfgRed{};
    try {
        DbRuntime::Repositories::RepoConfiguracionRed::obtenerOInicializar(cfgRed);
    } catch (...) {}

    try {
        auto servidoresNtp = DbRuntime::Repositories::RepoServidoresNtp::listarActivosPorPrioridad(3);
        RTOS::RelojSistema::configurarParametrosRed(
            cfgSis.zonaHorariaMin,
            servidoresNtp.size() > 0 ? servidoresNtp[0].server.c_str() : "pool.ntp.org",
            servidoresNtp.size() > 1 ? servidoresNtp[1].server.c_str() : "time.google.com",
            servidoresNtp.size() > 2 ? servidoresNtp[2].server.c_str() : "ar.pool.ntp.org"
        );
    } catch (...) {
        RTOS::RelojSistema::configurarParametrosRed(cfgSis.zonaHorariaMin);
    }

    // 3. Inicializar comunicaciones serie, red WiFi y cliente Reverse TCP Relay
    uartsRuntime.abrirPuertoServidor();
    wifiTcpRuntime.vincularRelay(&wifiRelayClient);
    wifiRelayClient.configurar(
        cfgRed.relay_activa,
        cfgRed.relay_host.c_str(),
        static_cast<uint16_t>(cfgRed.relay_puerto),
        cfgRed.relay_node_id.c_str(),
        cfgRed.relay_secreto.c_str()
    );

    if (wifiInicializador.iniciarRedDesdeBaseDeDatos()) {
        wifiTcpRuntime.iniciarServidorSeguro();
        wifiUdpRuntime.iniciarSocketStreaming();
        RTOS::RelojSistema::sincronizarNtpSiHayWiFi();
    }

    // 4. Escanear bus 1-Wire e hidratar sensores desde la BD antes de lanzar las tareas
    EstadoBus estadoBusBoot = gestorSensores.verificarEstadoElectricoBus();
    if (estadoBusBoot == EstadoBus::OK) {
        gestorSensores.escanearYAsignarSensores();
        estadoBusBoot = gestorSensores.obtenerEstadoBus();
    }

    // 5. Encolar auditoría de arranque de forma asíncrona (sin bloquear setup() con lecturas de FAT32)
    RTOS::RegistroPersistenciaPOD regBoot{};
    regBoot.tipo          = RTOS::TipoRegistroDB::EVENTO_SISTEMA_BUS;
    regBoot.timestampUnix = RTOS::RelojSistema::obtenerTimestampActual();
    regBoot.estadoNuevo   = static_cast<int16_t>(estadoBusBoot);
    strncpy(regBoot.detalle, "BOOT_SISTEMA_INICIALIZADO", sizeof(regBoot.detalle) - 1);
    RTOS::encolarPersistencia(regBoot);

    // 6. Lanzar las tareas definitivas de FreeRTOS (Alertas/DB, Sensores, OLED, Comunicaciones y BIOS)
    colaTrabajoRpc.inicializar();

    RTOS::GestorRTOS::iniciarSistemaCompleto(
        uartsRuntime,
        wifiRuntime,
        colaTrabajoRpc,
        middlewareInterfaceL1,
        middlewareSesionL2,
        middlewareMessageL3
    );
}

void loop() {
    // Todas las tareas operan en FreeRTOS (GestorRTOS). Devuelve 8 KB contiguos al Heap.
    vTaskDelete(nullptr);
}