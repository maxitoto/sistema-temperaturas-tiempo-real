#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include <atomic>
#include <type_traits>
#include <string.h>
#include "../hardware/driver/oled/Oled.hpp"
#include "../hardware/driver/oled/icons/icons.hpp"

namespace RTOS {

    // 1. SECCIÓN CRÍTICA DE HARDWARE: portENTER_CRITICAL() / portEXIT_CRITICAL()
    class SpinlockCritico {
    private:
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    public:
        void entrar() { portENTER_CRITICAL(&mux); }
        void salir()  { portEXIT_CRITICAL(&mux); }
    };

    class GuardiaCritico {
    private:
        SpinlockCritico& lock;
    public:
        explicit GuardiaCritico(SpinlockCritico& s) : lock(s) { lock.entrar(); }
        GuardiaCritico(const GuardiaCritico&) = delete;
        GuardiaCritico& operator=(const GuardiaCritico&) = delete;
        ~GuardiaCritico() { lock.salir(); }
    };

    // 2. SEMÁFORO SEGURO CON DETECTOR DE DEADLOCK
    class SemaforoSeguro {
    private:
        SemaphoreHandle_t handle = nullptr;
        const char* nombre;
        TaskHandle_t duenoActual = nullptr;
        uint32_t instanteTomaMs = 0;
        uint32_t maxRetencionMedidaMs = 0;
        uint32_t conteoTimeouts = 0;
        bool deadlockDetectado = false;

        void asegurarInicializado() {
            if (handle == nullptr) {
                handle = xSemaphoreCreateMutex();
            }
        }

    public:
        explicit SemaforoSeguro(const char* nombreSem = "Mutex") : nombre(nombreSem) {}

        bool tomar(int32_t timeoutMs = -1) {
            asegurarInicializado();
            if (!handle) return false;

            TaskHandle_t tareaLlamadora = xTaskGetCurrentTaskHandle();
            if (duenoActual != nullptr && duenoActual == tareaLlamadora) {
                deadlockDetectado = true;
                return false;
            }

            TickType_t ticks = (timeoutMs < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
            if (xSemaphoreTake(handle, ticks) == pdTRUE) {
                duenoActual = tareaLlamadora;
                instanteTomaMs = millis();
                return true;
            }

            conteoTimeouts++;
            return false;
        }

        void soltar() {
            if (!handle) return;
            uint32_t duracion = millis() - instanteTomaMs;
            if (duracion > maxRetencionMedidaMs) {
                maxRetencionMedidaMs = duracion;
            }
            duenoActual = nullptr;
            xSemaphoreGive(handle);
        }

        bool huboDeadlock() const { return deadlockDetectado; }
        void limpiarBanderaDeadlock() { deadlockDetectado = false; }
        uint32_t obtenerTimeouts() const { return conteoTimeouts; }
        uint32_t obtenerMaxRetencionMs() const { return maxRetencionMedidaMs; }
    };

    // 3. COLA TIPADA SEGURA (Solo acepta structs planos POD)
    template <typename T, size_t Capacidad>
    class ColaSegura {
        static_assert(std::is_trivially_copyable<T>::value,
                      "ColaSegura solo acepta structs planos (POD).");
    private:
        QueueHandle_t cola = nullptr;
        std::atomic<uint32_t> enviadosOk{0};
        std::atomic<uint32_t> recibidosOk{0};
        std::atomic<uint32_t> fallosColaLlena{0};

        void asegurarInicializada() {
            if (cola == nullptr) {
                cola = xQueueCreate(Capacidad, sizeof(T));
            }
        }

    public:
        ColaSegura() = default;

        bool enviar(const T& item, int32_t timeoutMs = 0) {
            asegurarInicializada();
            if (!cola) return false;
            TickType_t ticks = (timeoutMs < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
            if (xQueueSend(cola, &item, ticks) == pdPASS) {
                enviadosOk.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            fallosColaLlena.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        bool recibir(T& itemOut, int32_t timeoutMs = 0) {
            asegurarInicializada();
            if (!cola) return false;
            TickType_t ticks = (timeoutMs < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
            if (xQueueReceive(cola, &itemOut, ticks) == pdPASS) {
                recibidosOk.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            return false;
        }

        size_t disponibles() {
            asegurarInicializada();
            return cola ? static_cast<size_t>(uxQueueMessagesWaiting(cola)) : 0;
        }

        void vaciar() {
            asegurarInicializada();
            if (cola) {
                xQueueReset(cola);
                enviadosOk.store(0, std::memory_order_relaxed);
                recibidosOk.store(0, std::memory_order_relaxed);
                fallosColaLlena.store(0, std::memory_order_relaxed);
            }
        }

        uint32_t obtenerEnviados() const { return enviadosOk.load(std::memory_order_relaxed); }
        uint32_t obtenerRecibidos() const { return recibidosOk.load(std::memory_order_relaxed); }
        uint32_t obtenerDesbordes() const { return fallosColaLlena.load(std::memory_order_relaxed); }
    };

    // 4. TAREA SEGURA CON TELEMETRÍA Y RECUPERACIÓN POR BIOS
    class TareaSegura {
    public:
        using FuncionPaso = void (*)(void* ctx);

    private:
        const char* nombre = "Task";
        std::atomic<TaskHandle_t> handle{nullptr};
        FuncionPaso funcionPaso = nullptr;
        void* contexto = nullptr;
        uint32_t periodoMs = 100;
        uint32_t stackReservadoBytes = 4096;
        uint8_t prioridad = 1;
        BaseType_t nucleo = 1;
        std::atomic<bool> seguirEjecutando{false};

        std::atomic<uint32_t> ciclosEjecutados{0};
        std::atomic<uint32_t> maxTiempoEjecucionUs{0};
        std::atomic<uint32_t> minStackLibreBytes{0};
        std::atomic<uint32_t> ultimoLatidoMs{0};
        std::atomic<bool>     enEjecucionPaso{false};
        std::atomic<uint16_t> reiniciosBios{0};

        static void puntoEntradaEstatico(void* param) {
            TareaSegura* self = static_cast<TareaSegura*>(param);
            self->ultimoLatidoMs.store(millis(), std::memory_order_relaxed);

            while (self->seguirEjecutando.load(std::memory_order_relaxed)) {
                self->ultimoLatidoMs.store(millis(), std::memory_order_relaxed);
                self->enEjecucionPaso.store(true, std::memory_order_relaxed);

                int64_t t0 = esp_timer_get_time();
                if (self->funcionPaso) {
                    self->funcionPaso(self->contexto);
                }
                uint32_t dt = static_cast<uint32_t>(esp_timer_get_time() - t0);

                self->enEjecucionPaso.store(false, std::memory_order_relaxed);
                self->ultimoLatidoMs.store(millis(), std::memory_order_relaxed);

                uint32_t prevMax = self->maxTiempoEjecucionUs.load(std::memory_order_relaxed);
                if (dt > prevMax) {
                    self->maxTiempoEjecucionUs.store(dt, std::memory_order_relaxed);
                }
                self->minStackLibreBytes.store(uxTaskGetStackHighWaterMark(nullptr), std::memory_order_relaxed);
                self->ciclosEjecutados.fetch_add(1, std::memory_order_relaxed);

                uint32_t restante = self->periodoMs;
                while (restante > 0 && self->seguirEjecutando.load(std::memory_order_relaxed)) {
                    uint32_t paso = (restante > 50) ? 50 : restante;
                    vTaskDelay(pdMS_TO_TICKS(paso));
                    restante -= paso;
                    self->ultimoLatidoMs.store(millis(), std::memory_order_relaxed);
                }
            }
            self->enEjecucionPaso.store(false, std::memory_order_relaxed);
            self->handle.store(nullptr, std::memory_order_relaxed);
            vTaskDelete(nullptr);
        }

    public:
        TareaSegura() = default;

        bool iniciarEnNucleo(
            const char* nombreTarea,
            FuncionPaso fnPasoCiclico,
            uint32_t intervaloMs,
            uint32_t stackBytes = 4096,
            uint8_t prio = 1,
            BaseType_t coreId = 1,
            void* ctx = nullptr
        ) {
            if (handle.load(std::memory_order_relaxed) != nullptr) return false;
            nombre              = nombreTarea;
            funcionPaso         = fnPasoCiclico;
            contexto            = ctx;
            periodoMs           = (intervaloMs == 0) ? 10 : intervaloMs;
            stackReservadoBytes = stackBytes;
            prioridad           = prio;
            nucleo              = coreId;
            seguirEjecutando.store(true, std::memory_order_relaxed);
            enEjecucionPaso.store(false, std::memory_order_relaxed);
            ultimoLatidoMs.store(millis(), std::memory_order_relaxed);

            TaskHandle_t nuevoHandle = nullptr;
            BaseType_t res = xTaskCreatePinnedToCore(
                &TareaSegura::puntoEntradaEstatico,
                nombre,
                stackReservadoBytes,
                this,
                prioridad,
                &nuevoHandle,
                nucleo
            );
            if (res != pdPASS) {
                seguirEjecutando.store(false, std::memory_order_relaxed);
                return false;
            }
            handle.store(nuevoHandle, std::memory_order_relaxed);
            return true;
        }

        // Usado exclusivamente por TaskBiosRTOS cuando una tarea queda colgada sin retener mutex críticos
        bool reiniciarForzadoPorBios() {
            TaskHandle_t hActual = handle.exchange(nullptr, std::memory_order_relaxed);
            if (hActual != nullptr) {
                vTaskDelete(hActual);
            }
            enEjecucionPaso.store(false, std::memory_order_relaxed);
            seguirEjecutando.store(true, std::memory_order_relaxed);
            ultimoLatidoMs.store(millis(), std::memory_order_relaxed);
            reiniciosBios.fetch_add(1, std::memory_order_relaxed);

            TaskHandle_t nuevoHandle = nullptr;
            BaseType_t res = xTaskCreatePinnedToCore(
                &TareaSegura::puntoEntradaEstatico,
                nombre,
                stackReservadoBytes,
                this,
                prioridad,
                &nuevoHandle,
                nucleo
            );
            if (res != pdPASS) {
                seguirEjecutando.store(false, std::memory_order_relaxed);
                return false;
            }
            handle.store(nuevoHandle, std::memory_order_relaxed);
            return true;
        }

        bool eliminarTarea() {
            if (handle.load(std::memory_order_relaxed) == nullptr) return false;
            seguirEjecutando.store(false, std::memory_order_relaxed);
            for (uint8_t espera = 0; espera < 20 && handle.load(std::memory_order_relaxed) != nullptr; ++espera) {
                vTaskDelay(pdMS_TO_TICKS(15));
            }
            return (handle.load(std::memory_order_relaxed) == nullptr);
        }

        static void dormirMs(uint32_t ms) {
            vTaskDelay(pdMS_TO_TICKS(ms));
        }

        bool estaActiva() const              { return handle.load(std::memory_order_relaxed) != nullptr; }
        TaskHandle_t obtenerHandle() const   { return handle.load(std::memory_order_relaxed); }
        const char* obtenerNombre() const    { return nombre; }
        uint8_t obtenerPrioridad() const     { return prioridad; }
        int8_t obtenerNucleo() const         { return static_cast<int8_t>(nucleo); }
        uint32_t obtenerPeriodoMs() const    { return periodoMs; }
        uint32_t obtenerStackTotal() const   { return stackReservadoBytes; }
        uint32_t obtenerCiclos() const       { return ciclosEjecutados.load(std::memory_order_relaxed); }
        uint32_t obtenerMaxTiempoUs() const  { return maxTiempoEjecucionUs.load(std::memory_order_relaxed); }
        uint32_t obtenerStackLibreMin() const{ return minStackLibreBytes.load(std::memory_order_relaxed); }
        uint32_t obtenerUltimoLatidoMs()const{ return ultimoLatidoMs.load(std::memory_order_relaxed); }
        bool estaEjecutandoPaso() const      { return enEjecucionPaso.load(std::memory_order_relaxed); }
        uint16_t obtenerReiniciosBios() const{ return reiniciosBios.load(std::memory_order_relaxed); }
    };

    // 5. PIZARRA OLED (Buffer en RAM para desacoplar escritores de la única tarea I2C)
    class PizarraOled {
    private:
        SpinlockCritico lockRam;
        char     cabecera[12]          = "";
        char     linea1[22]            = "";
        char     linea2[22]            = "";
        char     linea3[22]            = "";
        char     linea4[22]            = "";
        bool     sucio                 = false;
        bool     mensajeTemporalActivo = false;
        uint32_t finMensajeTemporalMs  = 0;

        char prevCabecera[12] = "";
        char prev1[22]        = "";
        char prev2[22]        = "";
        char prev3[22]        = "";
        char prev4[22]        = "";

        static void copiarSeguro(char* dest, const char* src, size_t cap) {
            if (!src) { dest[0] = '\0'; return; }
            strncpy(dest, src, cap - 1);
            dest[cap - 1] = '\0';
        }

    public:
        static PizarraOled& getInstance() {
            static PizarraOled inst;
            return inst;
        }

        void publicarPantalla(
            const char* cab,
            const char* f1,
            const char* f2,
            const char* f3,
            const char* f4
        ) {
            GuardiaCritico g(lockRam);
            copiarSeguro(cabecera, cab, sizeof(cabecera));
            copiarSeguro(linea1,   f1,  sizeof(linea1));
            copiarSeguro(linea2,   f2,  sizeof(linea2));
            copiarSeguro(linea3,   f3,  sizeof(linea3));
            copiarSeguro(linea4,   f4,  sizeof(linea4));
            sucio = true;
        }

        void publicarMensajeTemporal(
            const char* cab,
            const char* f1,
            const char* f2,
            const char* f3,
            const char* f4,
            uint32_t duracionMs
        ) {
            if (duracionMs < 500)   duracionMs = 500;
            if (duracionMs > 15000) duracionMs = 15000;
            GuardiaCritico g(lockRam);
            copiarSeguro(cabecera, cab, sizeof(cabecera));
            copiarSeguro(linea1,   f1,  sizeof(linea1));
            copiarSeguro(linea2,   f2,  sizeof(linea2));
            copiarSeguro(linea3,   f3,  sizeof(linea3));
            copiarSeguro(linea4,   f4,  sizeof(linea4));
            finMensajeTemporalMs  = millis() + duracionMs;
            mensajeTemporalActivo = true;
            sucio                 = true;
        }

        bool tieneMensajeTemporalActivo() {
            GuardiaCritico g(lockRam);
            if (!mensajeTemporalActivo) return false;
            if (static_cast<int32_t>(millis() - finMensajeTemporalMs) >= 0) {
                mensajeTemporalActivo = false;
                return false;
            }
            return true;
        }

        void cancelarMensajeTemporal() {
            GuardiaCritico g(lockRam);
            mensajeTemporalActivo = false;
        }

        void volcarAlHardwareI2C(bool forzarRedibujo = false) {
            char c[12], l1[22], l2[22], l3[22], l4[22];
            {
                GuardiaCritico g(lockRam);
                if (!sucio && !forzarRedibujo) return;
                memcpy(c,  cabecera, sizeof(c));
                memcpy(l1, linea1,   sizeof(l1));
                memcpy(l2, linea2,   sizeof(l2));
                memcpy(l3, linea3,   sizeof(l3));
                memcpy(l4, linea4,   sizeof(l4));
                sucio = false;
            }

            if (forzarRedibujo) {
                prevCabecera[0] = '\0';
                prev1[0] = '\0';
                prev2[0] = '\0';
                prev3[0] = '\0';
                prev4[0] = '\0';
            }

            if (forzarRedibujo || strcmp(c, prevCabecera) != 0) {
                oled.cabeceraIzq(c);
                strcpy(prevCabecera, c);
            }
            if (forzarRedibujo || strcmp(l1, prev1) != 0) { oled.fila1(l1); strcpy(prev1, l1); }
            if (forzarRedibujo || strcmp(l2, prev2) != 0) { oled.fila2(l2); strcpy(prev2, l2); }
            if (forzarRedibujo || strcmp(l3, prev3) != 0) { oled.fila3(l3); strcpy(prev3, l3); }
            if (forzarRedibujo || strcmp(l4, prev4) != 0) { oled.fila4(l4); strcpy(prev4, l4); }
        }
    };

    inline PizarraOled& pizarraOled = PizarraOled::getInstance();

} // namespace RTOS