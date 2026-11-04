[TRANSPORT:UART_S] -> PHY_LINKED (motivo=NONE)
[TRANSPORT:UART_S] -> HANDSHAKING (motivo=NONE)
[TRANSPORT:UART_S] -> AUTHENTICATED (motivo=NONE)
┌────────────────┬─────────────────────┐
│ (index)        │ Values              │
├────────────────┼─────────────────────┤
│ staConectado   │ true                │
│ staSsid        │ 'Zhone_96F8'        │
│ ipDhcpAsignada │ '192.168.1.14'      │
│ gatewayDhcp    │ '192.168.1.1'       │
│ mascaraSubred  │ '255.255.255.0'     │
│ rssiDbm        │ -61                 │
│ apActivo       │ true                │
│ apSsid         │ 'ESP32-Secure'      │
│ ipApPropia     │ '192.168.4.1'       │
│ macAddress     │ '30:AE:A4:FE:ED:30' │
│ bootId         │ 4                   │
│ uptimeMs       │ 444062              │
│ resetReason    │ 1                   │
│ channel        │ 8                   │
└────────────────┴─────────────────────┘
[TRANSPORT:WIFI] -> PHY_LINKED (motivo=NONE)
[TRANSPORT:WIFI] -> HANDSHAKING (motivo=NONE)
[TRANSPORT:WIFI] -> AUTHENTICATED (motivo=NONE)
┌────────────────┬─────────────────────┐
│ (index)        │ Values              │
├────────────────┼─────────────────────┤
│ staConectado   │ true                │
│ staSsid        │ 'Zhone_96F8'        │
│ ipDhcpAsignada │ '192.168.1.14'      │
│ gatewayDhcp    │ '192.168.1.1'       │
│ mascaraSubred  │ '255.255.255.0'     │
│ rssiDbm        │ -61                 │
│ apActivo       │ true                │
│ apSsid         │ 'ESP32-Secure'      │
│ ipApPropia     │ '192.168.4.1'       │
│ macAddress     │ '30:AE:A4:FE:ED:30' │
│ bootId         │ 4                   │
│ uptimeMs       │ 523903              │
│ resetReason    │ 1                   │
│ channel        │ 8                   │
└────────────────┴─────────────────────┘
┌────────────────┬─────────────────────┐
│ (index)        │ Values              │
├────────────────┼─────────────────────┤
│ staConectado   │ true                │
│ staSsid        │ 'Zhone_96F8'        │
│ ipDhcpAsignada │ '192.168.1.14'      │
│ gatewayDhcp    │ '192.168.1.1'       │
│ mascaraSubred  │ '255.255.255.0'     │
│ rssiDbm        │ -62                 │
│ apActivo       │ true                │
│ apSsid         │ 'ESP32-Secure'      │
│ ipApPropia     │ '192.168.4.1'       │
│ macAddress     │ '30:AE:A4:FE:ED:30' │
│ bootId         │ 4                   │
│ uptimeMs       │ 532877              │
│ resetReason    │ 1                   │
│ channel        │ 8                   │
└────────────────┴─────────────────────┘
┌────────────────┬─────────────────────┐
│ (index)        │ Values              │
├────────────────┼─────────────────────┤
│ staConectado   │ true                │
│ staSsid        │ 'Zhone_96F8'        │
│ ipDhcpAsignada │ '192.168.1.14'      │
│ gatewayDhcp    │ '192.168.1.1'       │
│ mascaraSubred  │ '255.255.255.0'     │
│ rssiDbm        │ -62                 │
│ apActivo       │ true                │
│ apSsid         │ 'ESP32-Secure'      │
│ ipApPropia     │ '192.168.4.1'       │
│ macAddress     │ '30:AE:A4:FE:ED:30' │
│ bootId         │ 4                   │
│ uptimeMs       │ 533040              │
│ resetReason    │ 1                   │
│ channel        │ 8                   │
└────────────────┴─────────────────────┘
[TRANSPORT:WIFI] -> DISCONNECTED (motivo=LINK_TIMEOUT)
[TRANSPORT:WIFI] -> PHY_LINKED (motivo=LINK_TIMEOUT)
[TRANSPORT:WIFI] -> HANDSHAKING (motivo=LINK_TIMEOUT)
[TRANSPORT:WIFI] -> DISCONNECTED (motivo=HANDSHAKE_TIMEOUT)
[TRANSPORT:WIFI] -> PHY_LINKED (motivo=HANDSHAKE_TIMEOUT)
[TRANSPORT:WIFI] -> HANDSHAKING (motivo=HANDSHAKE_TIMEOUT)
[TRANSPORT:WIFI] -> AUTHENTICATED (motivo=HANDSHAKE_TIMEOUT)
┌────────────────┬─────────────────────┐
│ (index)        │ Values              │
├────────────────┼─────────────────────┤
│ staConectado   │ true                │
│ staSsid        │ 'Zhone_96F8'        │
│ ipDhcpAsignada │ '192.168.1.14'      │
│ gatewayDhcp    │ '192.168.1.1'       │
│ mascaraSubred  │ '255.255.255.0'     │
│ rssiDbm        │ -59                 │
│ apActivo       │ true                │
│ apSsid         │ 'ESP32-Secure'      │
│ ipApPropia     │ '192.168.4.1'       │
│ macAddress     │ '30:AE:A4:FE:ED:30' │
│ bootId         │ 4                   │
│ uptimeMs       │ 738435              │
│ resetReason    │ 1                   │
│ channel        │ 8                   │
└────────────────┴─────────────────────┘
[TRANSPORT:UART_S] -> CLOSED_IDLE (motivo=IDLE_CLOSE)
[TRANSPORT:WIFI] -> DISCONNECTED (motivo=USER_CLOSE)

---

await window.service.client.conectarSerialDetallado();
{ok: true, medio: 'UART-S', clientId: 1, sessionId: 880900482, bootId: 4, …}bootId: 4clientId: 1medio: "UART-S"motivo: "NONE"ok: truerebootDetected: falseresetReason: 1sessionId: 880900482[[Prototype]]: Object
await window.service.client.conectarSerialDetallado("/dev/cu.usbserial-0001");
{ok: true, medio: 'UART-S', clientId: 1, sessionId: 880900482, bootId: 4, …}bootId: 4clientId: 1medio: "UART-S"motivo: "NONE"ok: truerebootDetected: falseresetReason: 1sessionId: 880900482[[Prototype]]: Object
await window.service.client.conectarSerialDetallado();
{ok: true, medio: 'UART-S', clientId: 1, sessionId: 880900482, bootId: 4, …}bootId: 4clientId: 1medio: "UART-S"motivo: "NONE"ok: truerebootDetected: falseresetReason: 1sessionId: 880900482[[Prototype]]: Object
await window.service.client.testUartS();
{medio: 'UART-S', autenticado: true, sessionId: 880900482, sumaRpc: 5, echoRecibido: 'hola desde UART-S', …}autenticado: trueechoRecibido: "hola desde UART-S"estadoRed: {staConectado: true, staSsid: 'Zhone_96F8', ipDhcpAsignada: '192.168.1.14', gatewayDhcp: '192.168.1.1', mascaraSubred: '255.255.255.0', …}latenciaMs: 450medio: "UART-S"muestrasStreamRecibidas: (5) [{…}, {…}, {…}, {…}, {…}]sessionId: 880900482sumaRpc: 5[[Prototype]]: Object
await window.service.client.obtenerDiagnosticoConexion();
{uart: {…}, wifi: {…}}uart: {clientId: 1, estado: 'AUTHENTICATED', autenticado: true, puertoConfigurado: '/dev/cu.usbserial-0001', sessionId: 880900482, …}wifi: {clientId: 2, estado: 'DISCONNECTED', autenticado: false, ipDestino: '192.168.1.14', sessionId: 0, …}[[Prototype]]: Object
await window.service.client.conectarWiFiDetallado();
{ok: true, medio: 'WIFI', clientId: 2, sessionId: 1840052998, bootId: 4, …}bootId: 4clientId: 2medio: "WIFI"motivo: "NONE"ok: truerebootDetected: falseresetReason: 1sessionId: 1840052998[[Prototype]]: Object
await window.service.client.testUartS();
{medio: 'UART-S', autenticado: true, sessionId: 880900482, sumaRpc: 5, echoRecibido: 'hola desde UART-S', …}autenticado: trueechoRecibido: "hola desde UART-S"estadoRed: apActivo: trueapSsid: "ESP32-Secure"bootId: 4channel: 8gatewayDhcp: "192.168.1.1"ipApPropia: "192.168.4.1"ipDhcpAsignada: "192.168.1.14"macAddress: "30:AE:A4:FE:ED:30"mascaraSubred: "255.255.255.0"resetReason: 1rssiDbm: -61staConectado: truestaSsid: "Zhone_96F8"uptimeMs: 523903[[Prototype]]: ObjectlatenciaMs: 359medio: "UART-S"muestrasStreamRecibidas: Array(5)0: {sensorId: 1, celsius: 22.5, timestampMs: 524008}1: {sensorId: 1, celsius: 22.9, timestampMs: 524011}2: {sensorId: 1, celsius: 23.3, timestampMs: 524014}3: {sensorId: 1, celsius: 23.7, timestampMs: 524016}4: {sensorId: 1, celsius: 24.1, timestampMs: 524019}length: 5[[Prototype]]: Array(0)sessionId: 880900482sumaRpc: 5[[Prototype]]: Object
await window.service.client.testWiFiSta();
{medio: 'WIFI-STA', autenticado: true, sessionId: 1840052998, sumaRpc: 42, echoRecibido: 'hola desde WiFi-STA', …}autenticado: trueechoRecibido: "hola desde WiFi-STA"estadoRed: apActivo: trueapSsid: "ESP32-Secure"bootId: 4channel: 8gatewayDhcp: "192.168.1.1"ipApPropia: "192.168.4.1"ipDhcpAsignada: "192.168.1.14"macAddress: "30:AE:A4:FE:ED:30"mascaraSubred: "255.255.255.0"resetReason: 1rssiDbm: -62staConectado: truestaSsid: "Zhone_96F8"uptimeMs: 533040[[Prototype]]: ObjectlatenciaMs: 303medio: "WIFI-STA"muestrasStreamRecibidas: Array(5)0: {sensorId: 1, celsius: 22.5, timestampMs: 533093}1: {sensorId: 1, celsius: 22.9, timestampMs: 533097}2: {sensorId: 1, celsius: 23.3, timestampMs: 533099}3: {sensorId: 1, celsius: 23.7, timestampMs: 533101}4: {sensorId: 1, celsius: 24.1, timestampMs: 533104}length: 5[[Prototype]]: Array(0)sessionId: 1840052998sumaRpc: 42[[Prototype]]: Object
await window.service.client.obtenerDiagnosticoConexion();
{uart: {…}, wifi: {…}}uart: autenticado: trueclientId: 1estado: "AUTHENTICATED"idleNegociadoMs: 600000peerBootId: 4peerResetReason: 1puertoConfigurado: "/dev/cu.usbserial-0001"rebootDetected: falsesessionId: 880900482ultimoMotivoCierre: "NONE"[[Prototype]]: Objectwifi: autenticado: trueclientId: 2estado: "AUTHENTICATED"idleNegociadoMs: 600000ipDestino: "192.168.1.14"peerBootId: 4peerResetReason: 1rebootDetected: falsesessionId: 1840052998ultimoMotivoCierre: "NONE"[[Prototype]]: Object[[Prototype]]: Object
await window.service.client.obtenerDiagnosticoConexion();
{uart: {…}, wifi: {…}}uart: {clientId: 1, estado: 'AUTHENTICATED', autenticado: true, puertoConfigurado: '/dev/cu.usbserial-0001', sessionId: 880900482, …}wifi: autenticado: falseclientId: 2estado: "DISCONNECTED"idleNegociadoMs: 600000ipDestino: "192.168.1.14"peerBootId: 4peerResetReason: 1rebootDetected: falsesessionId: 1840052998ultimoMotivoCierre: "LINK_TIMEOUT"[[Prototype]]: Object[[Prototype]]: Object
await window.service.client.conectarWiFiDetallado();
{ok: false, medio: 'WIFI', clientId: 2, sessionId: 1895991372, bootId: 4, …}bootId: 4clientId: 2medio: "WIFI"motivo: "HANDSHAKE_TIMEOUT"ok: falserebootDetected: falseresetReason: 1sessionId: 1895991372[[Prototype]]: Object
await window.service.client.obtenerDiagnosticoConexion();
{uart: {…}, wifi: {…}}uart: {clientId: 1, estado: 'AUTHENTICATED', autenticado: true, puertoConfigurado: '/dev/cu.usbserial-0001', sessionId: 880900482, …}wifi: autenticado: falseclientId: 2estado: "DISCONNECTED"idleNegociadoMs: 600000ipDestino: "192.168.1.14"peerBootId: 4peerResetReason: 1rebootDetected: falsesessionId: 1840052998ultimoMotivoCierre: "LINK_TIMEOUT"[[Prototype]]: Object[[Prototype]]: Objectconstructor: ƒ Object()hasOwnProperty: ƒ hasOwnProperty()isPrototypeOf: ƒ isPrototypeOf()propertyIsEnumerable: ƒ propertyIsEnumerable()toLocaleString: ƒ toLocaleString()toString: ƒ toString()valueOf: ƒ valueOf()__defineGetter__: ƒ __defineGetter__()__defineSetter__: ƒ __defineSetter__()__lookupGetter__: ƒ __lookupGetter__()__lookupSetter__: ƒ __lookupSetter__()__proto__: (...)get __proto__: ƒ __proto__()set __proto__: ƒ __proto__()
await window.service.client.conectarWiFiDetallado("192.168.4.1");
{ok: true, medio: 'WIFI', clientId: 2, sessionId: 1598385895, bootId: 4, …}bootId: 4clientId: 2medio: "WIFI"motivo: "NONE"ok: truerebootDetected: falseresetReason: 1sessionId: 1598385895[[Prototype]]: Object
await window.service.client.conectarWiFiDetallado();
{ok: true, medio: 'WIFI', clientId: 2, sessionId: 1598385895, bootId: 4, …}
await window.service.client.conectarWiFiDetallado();
{ok: true, medio: 'WIFI', clientId: 2, sessionId: 1598385895, bootId: 4, …}
await window.service.client.testWiFiAp();
{medio: 'WIFI-AP', autenticado: true, sessionId: 1598385895, sumaRpc: 15, echoRecibido: 'hola desde WiFi-AP', …}autenticado: trueechoRecibido: "hola desde WiFi-AP"estadoRed: apActivo: trueapSsid: "ESP32-Secure"bootId: 4channel: 8gatewayDhcp: "192.168.1.1"ipApPropia: "192.168.4.1"ipDhcpAsignada: "192.168.1.14"macAddress: "30:AE:A4:FE:ED:30"mascaraSubred: "255.255.255.0"resetReason: 1rssiDbm: -59staConectado: truestaSsid: "Zhone_96F8"uptimeMs: 738435[[Prototype]]: ObjectlatenciaMs: 204medio: "WIFI-AP"muestrasStreamRecibidas: Array(5)0: {sensorId: 1, celsius: 22.5, timestampMs: 738489}1: {sensorId: 1, celsius: 22.9, timestampMs: 738492}2: {sensorId: 1, celsius: 23.3, timestampMs: 738495}3: {sensorId: 1, celsius: 23.7, timestampMs: 738497}4: {sensorId: 1, celsius: 24.1, timestampMs: 738500}length: 5[[Prototype]]: Array(0)sessionId: 1598385895sumaRpc: 15[[Prototype]]: Object
await window.service.client.estaAutenticado(); 
true
await window.service.client.decirHolaAlFirmware("Hola!!!!!!");
'Hola!!!!!!'
await window.service.client.estaAutenticado(); 
true
await window.service.client.desconectarWiFi();
undefined
await window.service.client.estaAutenticado();
false

---

