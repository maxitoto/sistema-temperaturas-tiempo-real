#pragma once
#include "SyncPrimitives.hpp"

namespace Rtos {

namespace Evt {
    constexpr EventBits_t EVT_SENSORES_ACTUALIZADOS = (1 << 0);
    constexpr EventBits_t EVT_CONEXION_CAMBIO       = (1 << 1);
    constexpr EventBits_t EVT_ALARMA_CAMBIO         = (1 << 2);
    constexpr EventBits_t EVT_FORZAR_GUARDADO_DB    = (1 << 3);
}

// Alias para mantener retrocompatibilidad si algún archivo usa namespaceEvt
namespace namespaceEvt = Evt;

class SystemBus {
public:
    RtosMutex dbMutex;             // Protege el acceso SPI a la MicroSD y al ORM
    RtosMutex i2cMutex;            // Protege el bus I2C de la pantalla OLED
    RtosSemaphore semMuteAlarma;   // Semáforo binario para silenciar alarmas (ISR botón / RPC)
    RtosEventGroup eventos;        // Banderas reactivas entre controladores

    static SystemBus& instance() {
        static SystemBus inst;
        return inst;
    }

    // Helpers legibles para desarrolladores no experimentados
    LockGuard lockDb(uint32_t timeoutMs = 3000)  { return LockGuard(dbMutex, timeoutMs); }
    LockGuard lockI2c(uint32_t timeoutMs = 1000) { return LockGuard(i2cMutex, timeoutMs); }

private:
    SystemBus() = default;
};

inline SystemBus& sysBus = SystemBus::instance();

} // namespace Rtos