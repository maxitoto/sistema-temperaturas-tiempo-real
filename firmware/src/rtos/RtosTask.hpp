#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>

namespace Rtos {

enum class TaskMode : uint8_t {
    PERIODIC = 0,       // Ejecución fija cada intervalMs
    EVENT_DRIVEN = 1,   // Duerme hasta recibir wakeUp() / notify()
    HYBRID = 2          // Corre cada intervalMs, pero puede adelantarse con wakeUp() / notify()
};

// Configuración opcional con valores seguros por defecto para devs no experimentados
struct TaskConfig {
    const char* name       = "AppTask";
    uint32_t intervalMs    = 1000;
    uint32_t stackBytes    = 4096;
    UBaseType_t priority   = 1;
    BaseType_t coreId      = 1; // Core 1 (App CPU) por defecto para no interferir con WiFi en Core 0
    TaskMode mode          = TaskMode::HYBRID;
};

class RtosTask {
public:
    // Constructor simple: solo pide nombre e intervalo en ms
    explicit RtosTask(const char* name, uint32_t intervalMs = 1000)
        : RtosTask(TaskConfig{name, intervalMs, 4096, 1, 1, TaskMode::HYBRID}) {}

    // Constructor declarativo con struct TaskConfig
    explicit RtosTask(const TaskConfig& cfg)
        : name_(cfg.name), stackBytes_(cfg.stackBytes), priority_(cfg.priority),
          coreId_(cfg.coreId), mode_(cfg.mode), intervalMs_(cfg.intervalMs) {}

    // Constructor clásico completo (retrocompatible)
    RtosTask(const char* name, uint32_t stackBytes, UBaseType_t priority,
             BaseType_t coreId, TaskMode mode, uint32_t intervalMs = 1000)
        : name_(name), stackBytes_(stackBytes), priority_(priority),
          coreId_(coreId), mode_(mode), intervalMs_(intervalMs) {}

    virtual ~RtosTask() { stop(); }

    bool start() {
        if (handle_ != nullptr) return true;
        running_ = true;
        BaseType_t res = xTaskCreatePinnedToCore(
            taskEntry, name_, stackBytes_, this, priority_, &handle_, coreId_
        );
        return res == pdPASS;
    }

    void stop() {
        running_ = false;
        if (handle_ != nullptr) {
            TaskHandle_t h = handle_;
            handle_ = nullptr;
            vTaskDelete(h);
        }
    }

    void suspend() { if (handle_) vTaskSuspend(handle_); }
    void resume()  { if (handle_) vTaskResume(handle_); }

    // Nombres intuitivos + alias retrocompatibles
    void wakeUp()        { notify(); }
    void wakeUpFromISR() { notifyFromISR(); }

    void notify() {
        if (handle_) xTaskNotifyGive(handle_);
    }

    void notifyFromISR() {
        if (!handle_) return;
        BaseType_t higherWoken = pdFALSE;
        vTaskNotifyGiveFromISR(handle_, &higherWoken);
        if (higherWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }

    void setIntervalMs(uint32_t ms) { intervalMs_ = ms; }
    uint32_t getIntervalMs() const  { return intervalMs_; }

    UBaseType_t getFreeStackWords() const {
        return handle_ ? uxTaskGetStackHighWaterMark(handle_) : 0;
    }

protected:
    virtual void onSetup() {}
    // Ahora tiene implementación por defecto para permitir sobreescribir onLoop() sin parámetros
    virtual void onStep(bool /*wokenByNotification*/) { onLoop(); }
    virtual void onLoop() {}

private:
    const char* name_;
    uint32_t stackBytes_;
    UBaseType_t priority_;
    BaseType_t coreId_;
    TaskMode mode_;
    volatile uint32_t intervalMs_;
    TaskHandle_t handle_ = nullptr;
    volatile bool running_ = false;

    static void taskEntry(void* arg) {
        auto* self = static_cast<RtosTask*>(arg);
        self->onSetup();
        TickType_t lastWakeTime = xTaskGetTickCount();

        while (self->running_) {
            if (self->mode_ == TaskMode::PERIODIC) {
                self->onStep(false);
                vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(self->intervalMs_));
            } else if (self->mode_ == TaskMode::EVENT_DRIVEN) {
                uint32_t count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                if (count > 0 && self->running_) {
                    self->onStep(true);
                }
            } else {
                uint32_t count = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(self->intervalMs_));
                if (self->running_) {
                    self->onStep(count > 0);
                }
            }
        }
        vTaskDelete(nullptr);
    }
};

// =========================================================================
// SimpleTask: Permite crear tareas con lambdas/funciones sin usar herencia
// =========================================================================
class SimpleTask : public RtosTask {
public:
    using Callback = std::function<void()>;

    SimpleTask(const char* name, uint32_t intervalMs, Callback loopFn, Callback setupFn = nullptr,
               uint32_t stackBytes = 4096, UBaseType_t priority = 1, BaseType_t coreId = 1)
        : RtosTask(TaskConfig{name, intervalMs, stackBytes, priority, coreId, TaskMode::HYBRID}),
          setupFn_(std::move(setupFn)), loopFn_(std::move(loopFn)) {}

protected:
    void onSetup() override { if (setupFn_) setupFn_(); }
    void onLoop() override  { if (loopFn_)  loopFn_();  }

private:
    Callback setupFn_;
    Callback loopFn_;
};

} // namespace Rtos