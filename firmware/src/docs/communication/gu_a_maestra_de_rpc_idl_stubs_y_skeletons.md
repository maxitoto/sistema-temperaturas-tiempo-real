# Guía Maestra 2: Arquitectura RPC Real (IDL, Stubs, Skeletons y Servicios)

Este manual vincula directamente los conceptos formales de llamadas a procedimientos remotos (**Capítulo 16: Remote Method Invocation / Object Request Broker**) con tu implementación en Electron y ESP32, y corrige el acoplamiento que hacía difícil propagar cambios desde la base de datos hasta el Frontend.

---

## 1. Mapa Conceptual: Del Libro (`Kap16.md`) a tu Código Real

En la arquitectura clásica de RPC y ORB (Secciones 16.1.1 a 16.2.8 de tu libro), el cliente y el servidor nunca manipulan sockets ni bytes directamente; se comunican a través de representantes locales (**Client Stub** y **Server Skeleton**) definidos por una interfaz común (**IDL**).

Así se mapea exactamente cada componente teórico en tu proyecto:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        PROCESO CLIENTE (Electron: Frontend + Backend)                  │
│                                                                                        │
│  [Componente React] ──(IPC)──► [FirmwareStub (clientExample.ts)]                       │
│   (Client App)                   │  1. Recibe el DTO tipado de TypeScript              │
│                                  │  2. Cifra campos sensibles (si aplica)              │
│                                  │  3. Empaqueta (Marshalling) hacia el ORB            │
│                                  ▼                                                     │
│                        [Pila OSI Backend: CapaAplicacion -> CapaFisica]                │
│                         (RPC Runtime / Client ORB: MsgPack + AES-256-GCM + TCP/UDP)    │
└──────────────────────────────────────────┬─────────────────────────────────────────────┘
                                           │ Cable USB (HDLC + ARQ)
                                           │ o Wi-Fi (TCP RFC 5531 / UDP Datagram)
┌──────────────────────────────────────────▼─────────────────────────────────────────────┐
│                             PROCESO SERVIDOR (ESP32 Firmware)                          │
│                                                                                        │
│                        [Pila OSI Firmware: CapaFisica -> CapaAplicacion]               │
│                         (RPC Runtime / Server ORB: Desencripta GCM + Unmarshalling)    │
│                                  │                                                     │
│                                  ▼                                                     │
│                        [Middleware Política + registrarSkeletonFirmware]               │
│                         (Server Skeleton: ServerSkeleton.ipp)                          │
│                                  │  1. Deserializa CredencialRpcDTO y el InputDTO      │
│                                  │  2. Invoca al método virtual de IFirmware           │
│                                  │  3. Serializa el OutputDTO en EscritorMsgPack       │
│                                  ▼                                                     │
│                        [FirmwareImpl (FirmwareService.cpp)]                            │
│                         (Server Implementation / Business Object)                      │
│                                  │  Ejecuta lógica de negocio y llama al ORM/SGBD      │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

| Concepto del Libro (`Kap16.md`) | Archivo en Backend (TypeScript) | Archivo en Firmware (C++) |
| :--- | :--- | :--- |
| **IDL (Interface Definition)** | `src/backend/stubs/contratos.ts` (`IFirmware`, `RPC`, DTOs) | `src/stubs/ContratosRPC.hpp` (`IFirmware`, `RPC_*`, DTOs) |
| **Client-Side Stub** | `src/backend/stubs/clientExample.ts` (`FirmwareStub`) | `src/stubs/ClienteBackendStub.hpp` (`BackendStub`) |
| **Server-Side Stub (Skeleton)** | `src/backend/stubs/serverExample.ts` (`registrarSkeletonBackend`) | `src/stubs/ServerSkeleton.ipp` (`registrarSkeletonFirmware`) |
| **Server Implementation** | `src/backend/stubs/serverExample.ts` (`BackendImpl`) | `src/stubs/FirmwareService.cpp` (`FirmwareImpl`) |
| **RPC Runtime / ORB** | `CapaAplicacion.ts` + `CapaPresentacion.ts` + `CapaSesion.ts` | `CapaAplicacion.h` + `CapaPresentacion.h` + `CapaSesion.h` |

---

## 2. Diagnóstico: Las 3 "Trampas" que hacían imposible modificar una Tabla o RPC

Cuando intentaste modificar la tabla `sensor` o `configuracion_conectividad` y llevar ese cambio hasta el endpoint RPC, chocaste con tres problemas de diseño que vamos a rectificar:

### Trampa 1: Argumentos "Aplanados" y Conteo Mágico (`cantArgs < 9`)
*   **Cómo estaba antes:** El Stub enviaba `[emisor, firma, user, pin, hex, errSup, errInf, umbSup, umbInf]` como 9 elementos sueltos en el array raíz de MessagePack. Y el Skeleton en C++ tenía escrito `if (cantArgs < 9) return false;`. Si agregabas 6 columnas al sensor, tenías que acordarte de cambiar el `9` por `15` en el Skeleton, cambiar la firma del repositorio y cambiar el orden de lectura.
*   **Cómo se soluciona (Estándar RPC/IDL):** Todo procedimiento RPC autenticado envía **siempre 2 bloques estructurados** (o 1 si es solo consulta sin filtros):
    1. `CredencialRpcDTO` (4 campos fijos).
    2. `InputDTO` encapsulado como un sub-array MessagePack propio (`w.iniciarArray(N)`).
    *Beneficio:* En el nivel raíz del RPC, `cantArgs` es constante. El Skeleton simplemente delega en `in.deserializar(args)` y nunca más rechaza llamadas por olvidar actualizar un `if (cantArgs < X)`.

### Trampa 2: Doble Deserialización en el Middleware de Política
*   **Cómo estaba antes:** `evaluarPoliticaProcesoUsuario` deserializaba el DTO completo del sensor solo para validar si `umbralInferior < -40.0f`, y luego el Skeleton lo volvía a deserializar. Si cambiabas un campo en el DTO y olvidabas tocar el middleware, la política rechazaba el paquete con `FORMATO_SENSOR_INVALIDO`.
*   **Cómo se soluciona:** El middleware de política (`evaluarPoliticaProcesoUsuario`) **solo debe autenticar y autorizar** (validar Certificado TLS, Usuario/PIN contra `usuarios.bin` y Rol DCL `ROL_ADMIN` vs `ROL_REGULAR`). Las validaciones de negocio del DTO pertenecen a `InputDTO::deserializar()` o a `FirmwareImpl`.

### Trampa 3: Tipos Duplicados en el Frontend
*   **Cómo estaba antes:** `DatabaseExplorer.tsx` y `ConfiguracionRedPanel.tsx` volvían a escribir `interface EstadoRed` o `interface SensorEnriquecido` a mano.
*   **Cómo se soluciona:** Como `bridge.types.ts` ya infiere automáticamente los tipos de retorno de `clientServices.ts` a través de `window.service.client.<metodo>`, en el Frontend puedes obtener el tipo exacto con una sola línea:
    ```ts
    type SensorDTO = Awaited<ReturnType<typeof window.service.client.listarSensoresFirmware>>[number];
    ```
    Así, al cambiar `contratos.ts`, el Frontend se entera instantáneamente con autocompletado de TypeScript.

---

## 3. Recetario Paso a Paso: Cómo Crear, Modificar o Eliminar un Servicio RPC

### Receta 1: Cómo CREAR un nuevo Servicio RPC de punta a punta (5 Pasos)

Supongamos que quieres crear un servicio `calibrarPresionAtmosferica` (Transaccional por TCP) o un stream UDP:

#### Paso 1: Definir el Contrato IDL en C++ (`src/stubs/ContratosRPC.hpp`)
1. Agrega la constante del comando:
   ```cpp
   static const char* const RPC_DB_CALIBRAR_PRESION = "RPC_DB_CALIBRAR_PRESION";
   ```
2. Define el DTO de entrada/salida con sus métodos `deserializar` y `serializar`:
   ```cpp
   struct CalibrarPresionInputDTO {
       uint16_t idSensor;
       float    alturaMsnm;
       float    presionHpa;

       bool deserializar(LectorMsgPack& r) {
           uint32_t idTmp = 0;
           if (!r.leerUInt32(idTmp) || !r.leerFloat(alturaMsnm) || !r.leerFloat(presionHpa)) {
               return false;
           }
           idSensor = (uint16_t)idTmp;
           return true;
       }
   };
   ```
3. Agrega el método virtual puro en la interfaz `IFirmware`:
   ```cpp
   virtual bool calibrarPresion(const UsuarioCredencialDTO& cred,
                                const CalibrarPresionInputDTO& in,
                                FilaSensor& outSensor) = 0;
   ```

#### Paso 2: Implementar la Lógica de Negocio (`src/stubs/FirmwareService.cpp`)
En la clase `FirmwareImpl`, programas la operación usando el repositorio:
```cpp
bool FirmwareImpl::calibrarPresion(const UsuarioCredencialDTO& cred,
                                   const CalibrarPresionInputDTO& in,
                                   FilaSensor& outSensor) {
    ContextoSesionSGBD sesion = construirSesionDesdeCredencial(cred);
    if (!sesion.autenticado) return false;

    if (!repositorios().sensores.buscarPorId(in.idSensor, outSensor, sesion)) return false;
    outSensor.altura              = in.alturaMsnm;
    outSensor.presion_atmosferica = in.presionHpa;

    return repositorios().sensores.guardarSensorCompleto(
        outSensor, gEstadoSistema.ultimoUnixTimestamp, outSensor, sesion
    );
}
```

#### Paso 3: Registrar el Skeleton en el Servidor (`src/stubs/ServerSkeleton.ipp`)
En `registrarSkeletonFirmware` agregas el enlace:
*   Usa `appRpc.onRequest(...)` si quieres que viaje por **TCP Fiable**.
*   Usa `appRpc.onStream(...)` si quieres que viaje por **UDP Tiempo Real**.
*   Si requiere rol `ROL_ADMIN`, añade el comando a `requiereRolAdmin(comando)`.

```cpp
appRpc.onRequest(RPC_DB_CALIBRAR_PRESION, [](size_t, LectorMsgPack& args,
                                             EscritorMsgPack& resp, const MetaCall&) -> bool {
    CredencialRpcDTO auth;
    CalibrarPresionInputDTO in;
    if (!auth.deserializar(args) || !in.deserializar(args)) return false;

    FilaSensor actualizado = {};
    if (!servicioFirmware()->calibrarPresion(auth.usuario, in, actualizado)) return false;
    return resp.iniciarArray(2) &&
           resp.escribirTexto("OK") &&
           serializarFilaSensor(resp, actualizado);
});
```

#### Paso 4: Definir el Contrato e Implementar el Stub en Electron (`contratos.ts` y `clientExample.ts`)
1. En `src/backend/stubs/contratos.ts`:
   * Añade `DB_CALIBRAR_PRESION: "RPC_DB_CALIBRAR_PRESION"` al objeto `RPC`.
   * Define la interfaz `CalibrarPresionInput` y agrega la firma en `IFirmware`.
2. En `src/backend/stubs/clientExample.ts` (`FirmwareStub`):
   ```ts
   public readonly calibrarPresionFirmware = async (
     input: CalibrarPresionInput,
     opts?: OpcionesLlamada
   ): Promise<SensorFirmwareDTO> => {
     const [tupla] = await pila.appRpc.request<[TuplaSensor]>(
       RPC.DB_CALIBRAR_PRESION,
       [
         ...serializarCredencial(input.credencial),
         Number(input.idSensor),
         Number(input.alturaMsnm),
         Number(input.presionHpa),
       ],
       opts
     );
     return mapearTuplaSensor(tupla);
   };
   ```

#### Paso 5: Invocar desde el Frontend (`.tsx`)
**No necesitas tocar `clientServices.ts`, `handles.ts` ni `preload.ts`**. Gracias al registro dinámico (`...firmware` en `clientServices.ts` y `get-api-layout` en `handles.ts`), el método ya existe automáticamente en `window.service.client`:
```ts
const sensorActualizado = await window.service.client.calibrarPresionFirmware({
  credencial,
  idSensor: 1,
  alturaMsnm: 25.0,
  presionHpa: 1005.2,
});
```

---

### Receta 2: Cómo MODIFICAR un Servicio RPC Existente (Lista de Verificación de 4 Puntos)

Cada vez que agregues o cambies un campo en un servicio existente, sigue esta lista en orden:
1. **`ContratosRPC.hpp` (Firmware):**
   * Si es un dato de entrada, agrégalo al `struct ...InputDTO` y a su método `deserializar(LectorMsgPack& r)`.
   * Si es un dato de salida, agrégalo a `serializarFila...(EscritorMsgPack& w, ...)` e **incrementa en $+1$ el número dentro de `w.iniciarArray(N)`**.
2. **`FirmwareService.cpp` / `ServerSkeleton.ipp` (Firmware):**
   * Asigna el campo en la lógica de negocio.
3. **`contratos.ts` (Backend):**
   * Agrega la propiedad a la `interface ...DTO` correspondiente.
4. **`clientExample.ts` (Backend):**
   * Agrega el elemento en la tupla de retorno (`mapearTupla...`) o en el array de parámetros enviados a `pila.appRpc.request(...)`.

---

## 4. Física Aplicada: Corrección de $0\text{ }^\circ\text{C}$ y $100\text{ }^\circ\text{C}$ por Altura y Presión Atmosférica

Dado que agregaste `altura`, `presion_atmosferica`, `correcion_inferior` y `correcion_superior` en `sensor_fw`, aquí tienes las fórmulas físicas exactas para tu módulo de calibración en el ESP32:

1. **Estimación de Presión Atmosférica Barométrica a partir de la Altura ($h$ en $\text{msnm}$):**
   Si el usuario ingresa solo la altura $h$ (en metros) y no tiene un barómetro a mano, la presión estándar $P$ (en $\text{hPa}$) se calcula con la fórmula barométrica internacional:
   $$P(h) = 1013.25 \cdot \left(1 - 2.25577 \times 10^{-5} \cdot h\right)^{5.25588}$$

2. **Alteración del Punto de Ebullición del Agua ($T_{100}$ cerca de $100\text{ }^\circ\text{C}$) según la Presión $P$:**
   A partir de la ecuación de Antoine / Clausius-Clapeyron para el agua (donde $P_0 = 1013.25\text{ hPa}$ equivale a $760\text{ mmHg}$), la temperatura real de ebullición $T_{\text{ebullicion}}$ en grados Celsius es:
   $$T_{\text{ebullicion}}(P) \approx 100.0 + 0.02799 \cdot (P - 1013.25) - 1.36 \times 10^{-5} \cdot (P - 1013.25)^2$$
   *(Por ejemplo, en Ushuaia o en altitudes con baja presión de $980\text{ hPa}$, el agua hierve a $\approx 99.05\text{ }^\circ\text{C}$, no a $100.0\text{ }^\circ\text{C}$).*

3. **Corrección Lineal de 2 Puntos sobre la Temperatura Cruda ($T_{\text{cruda}}$):**
   Conociendo la desviación en hielo ($\Delta T_0 = \text{correcion\_inferior}$ en $T_0 = 0.0\text{ }^\circ\text{C}$) y la desviación en ebullición ($\Delta T_{100} = \text{correcion\_superior}$ en $T_{\text{ebullicion}}(P)$), la temperatura corregida se obtiene por interpolación lineal:
   $$T_{\text{corregida}} = T_{\text{cruda}} + \Delta T_0 + \left(\frac{\Delta T_{100} - \Delta T_0}{T_{\text{ebullicion}}(P) - 0.0}\right) \cdot T_{\text{cruda}}$$