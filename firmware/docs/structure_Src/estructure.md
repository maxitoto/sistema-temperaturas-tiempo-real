# Estructuras SRC

## v1
```Printxt
    firmware/
    ├── platformio.ini                # Selector de qué target compilar
    └── src/
        ├── main.cpp                  # INMUTABLE: Arranca el target y mata a loop()
        │
        ├── os/                       # Abstracción de FreeRTOS (Tu API tipo Java)
        │   ├── tarea.hpp             # xTaskCreate, vTaskDelayUntil, notificaciones
        │   ├── cola.hpp              # Queues y Queue Overwrite (longitud 1)
        │   ├── semaforo.hpp          # Binario, Contador y Mutex
        │   ├── grupo_eventos.hpp     # Event Groups (esperar múltiples bits)
        │   └── temporizador.hpp      # Software Timers y esp_timer
        │
        ├── hardware/                 # Capa Física y Drivers puros (Sin lógica)
        │   ├── placa/
        │   │   └── pines_nodemcu32s.hpp
        │   ├── almacenamiento/       # Driver MicroSD
        │   ├── pantalla/             # Driver OLED
        │   └── sensores/             # Driver 1-Wire
        │
        ├── modulos/                  # Lógica de dominio (Tus Tareas e IPC)
        │   ├── telemetria/           # Tarea Sensor + Cola de mediciones
        │   ├── almacenamiento/       # Tarea SGBD consumidora
        │   ├── alarmas/              # Tarea Alarma despertada por notificación
        │   └── comunicaciones/       # Tarea Pila OSI / RPC
        │
        └── targets/                  # PLUG-IN / PLUG-OUT (Solo se compila 1 a la vez)
            ├── contrato_target.hpp   # Declara: void iniciarTarget();
            ├── produccion/
            │   └── target.cpp        # Conecta todos los módulos reales
            ├── examples/
            │   ├── ej_microsd.cpp    # Solo prueba la SD
            │   └── ej_colas.cpp      # Solo prueba productor-consumidor
            └── tests/
                └── test_osi.cpp      # Solo prueba comunicación
```