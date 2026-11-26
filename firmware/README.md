## 📘 Guía de Entornos de Desarrollo

El proyecto utiliza un sistema de entornos múltiples. Cambiar de entorno ajusta automáticamente las variables globales (como los pines) para que coincidan con el hardware físico o virtual, manteniendo exactamente las mismas librerías sobre el framework de Arduino.

### 💻 1. Para el equipo SIN hardware (Simulador Wokwi)

* **Requisito 1:** Instalar la extensión de Wokwi en VS Code.
* **Requisito 2:** Instalar la extensión de PlatformIO en VS Code.

* Una vez instalado PlatformIO, debes abrir VS Code en la carpeta `esp8266`, no en la carpeta raíz del repositorio. Esto permite a la extensión leer el archivo `platformio.ini` e inyectar opciones en la barra inferior de VS Code (Status bar).

* **Selección de entorno:** En la barra inferior azul de VS Code (Status bar), haz clic en `📁 env:...` (también aparece como Default(esp8266)) y selecciona **`env:WokwiSimulator`**.
* **Construcción (Build):** Abre el menú de PlatformIO (el ícono del alien 👽 en la barra lateral izquierda). Haz clic en el botón **`Build`**. Esto instalará las librerías y creará el archivo ejecutable (`firmware.bin`) sin intentar conectarse a una placa física.
* **Wokwi-Gateway:** Deben descargar de aquí el Gateway correcto para su sistema operativo (SO) y CPU: `https://github.com/wokwi/wokwigw/releases`. Ejecútenlo con doble clic o, por ejemplo, en mi caso con `./wokwigw-darwin --forward udp:9081:10.13.37.2:81`, y déjenlo corriendo.
* **Inicio del simulador:** Abre el archivo `diagram.json`. Puedes hacer clic en el botón de "Play" que aparece, o presionar `Command+Shift+P` (o `Ctrl+Shift+P`), escribir `> Wokwi: Start Simulator` y presionar Enter. Un mensaje emergente los guiará para generar un certificado. Una vez obtenido el certificado, vuelve a ejecutar `> Wokwi: Start Simulator`.
* **Conexión con la App por USB:** El simulador necesita crear un puente de comunicación. En la terminal, ejecuta `socat -d -d pty,link=/tmp/virtual_serial,raw,echo=0 tcp:localhost:4000`. Socat te devolverá un puerto virtual, por ejemplo: `N PTY is /dev/ttys000` o `/tmp/virtual_serial`. La aplicación de escritorio se conectará automáticamente mediante `apisApp.connection.connectUsb()`, o pueden forzarlo con `apisApp.connection.connectUsb({path: "/dev/ttys000"})`.
* **Conexión con la App por WIFI (Gateway):** Wokwi-Gateway está por defecto y los puertos del ESP también. En la app se conecta así: `apisApp.connection.connectWifi({ip:"127.0.0.1", port:9080})`.
* **Conexión con la App por WIFI (Privada):** No se puede, por ahora.

---

### 🔌 2. Para el equipo CON hardware (ESP8266 Físico)

* **Requisito único:** Instalar la extensión de PlatformIO en VS Code.
* **Selección de entorno:** En la barra inferior azul de VS Code, haz clic en `📁 env:...` y selecciona **`env:Esp8266`**.
* **Carga del código:** Abre el menú de PlatformIO (ícono del alien 👽). Utiliza el comando **`Upload`** para compilar y flashear el código en la placa.
* **Monitoreo:** Si deseas ver los mensajes del dispositivo, utiliza el comando **`Upload and Monitor`** (o ejecuta `Monitor` después del upload).
* **⚠️ REGLA DE ORO DEL PUERTO SERIE:** El puerto USB no puede ser utilizado por dos programas al mismo tiempo. Si tienes abierto el "Monitor Serie" en VS Code, la aplicación de escritorio NO podrá conectarse al ESP8266 y las peticiones (requests) fallarán. Cierra el monitor serie antes de probar la app de escritorio.
* **Conexión con la App por USB:** La aplicación de escritorio se conectará automáticamente mediante `apisApp.connection.connectUsb()`, o pueden forzarlo con `apisApp.connection.connectUsb({path: "/dev/ttys000"})`. Para descubrir los puertos fácilmente, conecta el ESP8266 y en la barra de acceso rápido 👽 selecciona `Devices`; esto muestra los puertos conectados.
* **Conexión con la App por WIFI (Gateway):** El ESP recibe una IP por medio del protocolo DHCP del gateway de tu casa/oficina/universidad. No hay forma fácil de saber la IP sin ver la terminal, así que por el momento lee la terminal y utiliza esa IP. Por ejemplo: `apisApp.connection.connectWifi({ip:"192.168.56.17", port:80})`.
* **Conexión con la App por WIFI (Privada):** El ESP actúa como gateway y host de su propia red, y su IP es `192.168.4.1:80`. Primero tienes que conectar tu PC a su red (SSID: `RedPrivadaEsp8266` y Pass: `12345678`). Luego, tienes que hacer que la app hable con el ESP: `apisApp.connection.connectWifi({ip:"192.168.4.1", port:80})`. Tiene la IP `x.x.4.1` porque es el primer host de la red.

---

### 🚑 3. Resolución de Problemas (Troubleshooting)

* **Paso 1:** Ante cualquier error de compilación extraño o librerías que "desaparecen", elimina manualmente la carpeta oculta **`.pio`** de tu proyecto.
* **Paso 2:** Presiona el ícono del bote de basura (`PlatformIO: Clean`) en la barra inferior.
* **Paso 3:** Reinicia la ventana de VS Code. Presiona `Command+Shift+P` (o `Ctrl+Shift+P`), escribe `> Developer: Reload Window` y presiona Enter. Esto fuerza a PlatformIO a reconstruir todo el entorno desde cero.