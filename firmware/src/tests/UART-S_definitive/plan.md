```Printext
[FLUJO ACTUAL — Independiente del medio y de Capa 7]

 (Cliente: Backend Electron)
 Orden + Registro/ArregloTS ──► EmpaquetadorMarshalling ──► SerializadorBinarioTLV
                                                                     │ (Uint8Array)
                                                                     ▼
                                                           ProtocoloConfiableUARTS
                                                                     │ (Cable 8E1)
 (Servidor: Firmware ESP32)                                          ▼
 Evaluar Orden ◄── DesempaquetadorUnmarshalling ◄── DeserializadorBinarioTLV ◄── GuardiaSeguridadMiddleware ◄── ProtocoloConfiableUARTS

───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────

[FLUJO FUTURO — Multicanal (UART-S / WiFi STA TLS / WiFi AP TLS) + Seguridad en 2 Capas]

 (Cliente: Backend Electron)
 Orden ──► EmpaquetadorMarshalling ──► SerializadorBinarioTLV ──► EnrutadorMedioCliente (1 único medio activo + KeepAlive)
                                                                            │
 (Servidor: Firmware ESP32)                                                 ▼
 EnrutadorMedioServidor (identifica origen: USB / WIFI_STA / WIFI_AP) ◄─────┘
          │
          ▼
 GuardiaSeguridadMiddleware (valida cotas TLV en O(N), Stack HighWaterMark y Heap libre)
          │
          ▼
 DeserializadorBinarioTLV ──► DesempaquetadorUnmarshalling
                                       │
                                       ▼
                        ProxyAutenticacionYControlSesion
                        ├── Dominio único: Ruta PUBLICA (login/logout) vs PRIVADA
                        ├── Control Keep-Alive (3 min sin latido = caída de enlace)
                        └── Control Sesión Ociosa (30 min sin request = expiración)
                                       │
                                       ▼
                        DespachadorCapa7 (RPC / REST / otro)
                        └── Dominio único: Autorización por Rol (ADMIN, OPERADOR, LECTOR, ANONIMO)
                                       │
                                       ▼
                        Ejecución (ModuloDQL / ModuloDML / Hardware 1-Wire)
```