#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

namespace Rtos {

// Mutex FreeRTOS para proteger recursos compartidos (SPI MicroSD / SGBD e I2C OLED)
class RtosMutex {
public:
    RtosMutex() { handle_ = xSemaphoreCreateMutex(); }
    ~RtosMutex() { if (handle_) vSemaphoreDelete(handle_); }

    RtosMutex(const RtosMutex&) = delete;
    RtosMutex& operator=(const RtosMutex&) = delete;

    bool lock(uint32_t timeoutMs = portMAX_DELAY) {
        if (!handle_) return false;
        TickType_t ticks = (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        return xSemaphoreTake(handle_, ticks) == pdTRUE;
    }

    void unlock() {
        if (handle_) xSemaphoreGive(handle_);
    }

private:
    SemaphoreHandle_t handle_ = nullptr;
};

// Guardián RAII: bloquea al construirse y libera automáticamente al salir del bloque
class LockGuard {
public:
    explicit LockGuard(RtosMutex& mutex, uint32_t timeoutMs = portMAX_DELAY)
        : mutex_(mutex), locked_(mutex_.lock(timeoutMs)) {}

    ~LockGuard() {
        if (locked_) mutex_.unlock();
    }

    bool isLocked() const { return locked_; }

private:
    RtosMutex& mutex_;
    bool locked_ = false;
};

// Semáforo Binario FreeRTOS (ideal para despertar tareas desde ISRs o eventos externos como Mute)
class RtosSemaphore {
public:
    RtosSemaphore() { handle_ = xSemaphoreCreateBinary(); }
    ~RtosSemaphore() { if (handle_) vSemaphoreDelete(handle_); }

    RtosSemaphore(const RtosSemaphore&) = delete;
    RtosSemaphore& operator=(const RtosSemaphore&) = delete;

    void give() {
        if (handle_) xSemaphoreGive(handle_);
    }

    void giveFromISR() {
        if (!handle_) return;
        BaseType_t higherWoken = pdFALSE;
        xSemaphoreGiveFromISR(handle_, &higherWoken);
        if (higherWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }

    bool take(uint32_t timeoutMs = portMAX_DELAY) {
        if (!handle_) return false;
        TickType_t ticks = (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        return xSemaphoreTake(handle_, ticks) == pdTRUE;
    }

private:
    SemaphoreHandle_t handle_ = nullptr;
};

// Grupo de Eventos FreeRTOS (permite esperar múltiples banderas de hardware/red en simultáneo)
class RtosEventGroup {
public:
    RtosEventGroup() { handle_ = xEventGroupCreate(); }
    ~RtosEventGroup() { if (handle_) vEventGroupDelete(handle_); }

    void setBits(EventBits_t bits) {
        if (handle_) xEventGroupSetBits(handle_, bits);
    }

    void setBitsFromISR(EventBits_t bits) {
        if (!handle_) return;
        BaseType_t higherWoken = pdFALSE;
        xEventGroupSetBitsFromISR(handle_, bits, &higherWoken);
        if (higherWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }

    EventBits_t waitBits(EventBits_t bitsToWait, bool clearOnExit = true,
                         bool waitForAll = false, uint32_t timeoutMs = portMAX_DELAY) {
        if (!handle_) return 0;
        TickType_t ticks = (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
        return xEventGroupWaitBits(handle_, bitsToWait,
                                   clearOnExit ? pdTRUE : pdFALSE,
                                   waitForAll ? pdTRUE : pdFALSE,
                                   ticks);
    }

private:
    EventGroupHandle_t handle_ = nullptr;
};

} // namespace Rtos