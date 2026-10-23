# Sistema de Monitoreo de Temperatura para Heladera

Un sistema completo de monitoreo de temperatura para heladera/freezer, construido de forma incremental por un equipo principiante en electrónica y Arduino. Combina firmware embebido (ESP8266) con una aplicación de escritorio moderna (Electron + React) para visualización y control en tiempo real.

---

## Quick path

1. **Solo simulación (sin hardware):** Abre `recursos/monitorizacioon-temperatura-en-tiempo-real/esp8266` en VS Code con PlatformIO, selecciona `env:WokwiSimulator`, haz Build y sigue la [guía de Wokwi](#-entorno-de-simulación-wokwi).
2. **Con hardware físico (ESP8266):** Abre la misma carpeta, selecciona `env:Esp8266`, haz Upload y Monitor. Ver [guía de hardware](#-entorno-con-hardware-físico-esp8266).
3. **App de escritorio (Electron + React):** Entra a `recursos/monitorizacioon-temperatura-en-tiempo-real/desktop`, ejecuta `pnpm install && pnpm run dev`.
4. **Plantilla Electron lista para usar:** `recursos/Template-Electron` — arquitectura moderna con IPC tipado, `contextIsolation`, y auto-registro de servicios.

---

## Estructura del repositorio

```text
sistema-temperaturas-heladera/
├── application/                    # (vacío — reservado para futuros servicios)
├── firmware/                       # (vacío — reservado para builds compilados)
└── recursos/
    ├── monitorizacioon-temperatura-en-tiempo-real/   # Sistema principal
    │   ├── esp8266/                  # Firmware ESP8266 (PlatformIO + Wokwi)
    │   ├── desktop/                  # App Electron + React (pnpm, Vite, TS)
    │   ├── experiments/              # Pruebas aisladas de hardware (LED, DHT11, etc.)
    │   ├── firmware/                 # Arduino main.ino integrado (validado desde experiments)
    │   ├── docs/                     # Anteproyecto, arquitectura, hardware
    │   ├── requirements.txt          # pyserial para sincronía de hora desde PC
    │   └── timestamp.py              # Script de sincronización de timestamp
    │
    └── Template-Electron/            # Plantilla base Electron + React + TS
        ├── src/backend/              # Proceso principal, IPC handles, preload
        ├── src/frontend/             # React 19, componentes, hooks, tabs
        └── src/shared/               # Single Source of Truth: servicios.ts (prod/dev)
```

---

## Componentes principales

| Componente | Tech stack | Propósito |
|------------|------------|-----------|
| **Firmware ESP8266** | C++ / Arduino / PlatformIO | Lectura de sensores (DHT11, etc.), envío por USB/WiFi, simulación Wokwi |
| **App Desktop** | Electron 43, React 19, Vite 8, TypeScript, pnpm | UI en tiempo real, conexión USB/WiFi al ESP, gráficos, alertas |
| **Experiments** | Arduino .ino aislados | Validación de cada sensor/actuador antes de integrar |
| **Template-Electron** | Electron + React + TS (arquitectura de referencia) | Base reutilizable para apps de escritorio seguras y escalables |

---

## Reglas de trabajo del equipo

> **Regla de oro:** Todo hardware nuevo se prueba primero en `experiments/`. Solo el código validado pasa a `firmware/arduino/main.ino`.

- Un experimento = una carpeta en `experiments/` con su `.ino` y opcional `wokwi/`
- No modificar `main.ino` sin haber probado antes en `experiments/`
- Mantener cada experimento aislado y simple
- Documentar pines, conexiones y decisiones en el propio experimento

---

## Entorno de simulación (Wokwi)

**Requisitos:** VS Code + extensiones **Wokwi** y **PlatformIO**.

1. Abre VS Code en `recursos/monitorizacioon-temperatura-en-tiempo-real/esp8266`
2. En la Status bar (barra azul inferior), clic en `📁 env:...` → selecciona **`env:WokwiSimulator`**
3. PlatformIO (icono 👽) → **Build** (genera `firmware.bin`)
4. Descarga **Wokwi Gateway** para tu SO/CPU: <https://github.com/wokwi/wokwigw/releases>
5. Ejecuta el gateway (ej. `sudo ./wokwigw-darwin`) y déjalo corriendo
6. Abre `diagram.json` → botón **Play** o `Ctrl+Shift+P` → `Wokwi: Start Simulator`
7. Puente serial virtual:
   ```bash
   socat -d -d pty,link=/tmp/virtual_serial,raw,echo=0 tcp:localhost:4000
   ```
   Anota el PTY resultante (ej. `/dev/ttys000`)
8. En la app desktop: `apisApp.connection.connectUsb({path: "/dev/ttys000"})`  
   o por WiFi gateway: `apisApp.connection.connectWifi({ip:"127.0.0.1", port:9080})`

---

## Entorno con hardware físico (ESP8266)

**Requisito:** VS Code + extensión **PlatformIO**.

1. Abre VS Code en `recursos/monitorizacioon-temperatura-en-tiempo-real/esp8266`
2. Status bar → `📁 env:...` → **`env:Esp8266`**
3. PlatformIO → **Upload** (compila y flashea) o **Upload and Monitor**
4. ⚠️ **Regla del puerto serie:** El puerto USB no puede ser usado por dos programas a la vez. Cierra el Monitor Serie de VS Code antes de conectar la app desktop.
5. Conexión app desktop:
   - USB automático: `apisApp.connection.connectUsb()`
   - USB forzado: `apisApp.connection.connectUsb({path: "/dev/ttys000"})`
   - WiFi (gateway casero): lee IP en terminal → `apisApp.connection.connectWifi({ip:"192.168.x.x", port:80})`
   - WiFi (AP privado del ESP): conecta PC a SSID `RedPrivadaEsp8266` (pass `12345678`) → `apisApp.connection.connectWifi({ip:"192.168.4.1", port:80})`

---

## App Desktop (Electron + React)

```bash
cd recursos/monitorizacioon-temperatura-en-tiempo-real/desktop
pnpm install
pnpm run dev        # Desarrollo con HMR
pnpm run build      # Empaquetado producción (electron-builder)
```

- **Arquitectura:** `src/backend` (main, handles, preload) + `src/frontend` (React) + `src/shared/services.ts` (APIs tipadas prod/dev)
- **Seguridad:** `contextIsolation: true`, `nodeIntegration: false`, `sandbox: true`
- **IPC:** Auto-registro desde `shared/services.ts` → `handles.ts` → `preload.ts` → `window.electronAPI.prod/dev` con autocompletado TypeScript

---

## Plantilla Electron (Template-Electron)

Base lista para clonar y empezar apps de escritorio modernas:

```bash
cd recursos/Template-Electron
pnpm install
pnpm run dev
pnpm run build
```

Características:
- Separación estricta backend/frontend/shared
- IPC tipado con auto-registro (sin strings mágicos)
- `contextIsolation` + `sandbox` habilitados
- Vite 8 + React 19 + TypeScript strict
- electron-builder configurado

---

## Dependencias de firmware (Arduino IDE)

Si compilas `firmware/arduino/main.ino` desde Arduino IDE clásico, instala desde **Tools → Manage Libraries**:

- `Adafruit SSD1306`
- `Adafruit GFX Library`
- `Adafruit BusIO`
- `DHT sensor library`
- `Adafruit Unified Sensor`
- `SPI` y `SD` (incluidas en Arduino AVR)

Para sincronizar fecha/hora desde PC:
```bash
pip install -r recursos/monitorizacioon-temperatura-en-tiempo-real/requirements.txt
# requiere pyserial
```

---

## Documentación

| Archivo | Contenido |
|---------|-----------|
| `recursos/monitorizacioon-temperatura-en-tiempo-real/docs/anteproyecto.pdf` | Documento inicial del proyecto |
| `docs/arquitectura/diagrama_arquitectonico.pdf` | Diagrama de arquitectura del sistema |
| `docs/hardware/sensores.pdf` | Hojas de datos y guías de conexión de sensores |
| `esp8266/README.md` | Guía completa de entornos (Wokwi, hardware, troubleshooting) |
| `Template-Electron/README.md` | Arquitectura y comandos de la plantilla Electron |

---

## Estado actual

- ✅ Prueba de LED (experimento)
- ✅ Prueba de sensor DHT11 + simulación Wokwi
- ✅ Documentación inicial (anteproyecto, arquitectura, hardware)
- ✅ Firmware ESP8266 con entornos Wokwi / Hardware
- ✅ App Desktop Electron + React funcional
- ✅ Plantilla Electron reutilizable

---

## Próximos pasos (roadmap)

- [ ] Lectura de múltiples sensores de temperatura
- [ ] Medición de humedad integrada
- [ ] Alertas visuales (LED) y sonoras (buzzer)
- [ ] Registro histórico de temperaturas (SD / base de datos local)
- [ ] Umbrales configurables para heladera / freezer
- [ ] Simulación Wokwi completa del sistema integrado
- [ ] Migración a ESP32 (más RAM, Bluetooth, dual core)
- [ ] Notificaciones push / email / MQTT
- [ ] Integración con plataformas IoT (Home Assistant, ThingsBoard, etc.)

---

## Cómo contribuir

1. Crea un experimento en `experiments/nuevo_sensor/` con su `.ino` y `wokwi/`
2. Valida en simulación y/o hardware físico
3. Documenta pines, librerías y decisiones en el README del experimento
4. Abre un PR copiando solo lo validado a `firmware/arduino/main.ino`
5. Si toca la app desktop, trabaja en `desktop/src/` siguiendo la arquitectura backend/frontend/shared

---

## Licencia

MIT — ver `recursos/monitorizacioon-temperatura-en-tiempo-real/LICENSE` y `recursos/Template-Electron/LICENSE`.