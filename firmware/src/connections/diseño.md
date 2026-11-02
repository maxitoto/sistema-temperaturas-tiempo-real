                       ┌──────────────────────────────────────┐
                       │     TRONCO: CapaFisica (Singleton)   │
                       │   Enruta estrictamente por idMedio   │
                       │    (Sin Broadcast ciego entre ramas) │
                       └──────────────────┬───────────────────┘
                                          │ Interfaz ICanalFisico
        ┌────────────────┬────────────────┼────────────────┬────────────────┐
        ▼                ▼                ▼                ▼                ▼
  [Raíz id=0]      [Raíz id=1..3]   [Raíz id=4]      [Raíz id=5]      [Raíz id=6]
   CanalUSB         CanalSlotUDP    CanalWiFiTCP    CanalBluetooth     CanalMock
 (FIFO + Purga    (WiFi Datagrama   (Passthrough     (BT Classic /   (Inyección Caja
  Ruido + Guard     Multiplexado)    TCP / TLS)       BLE Stream)     Negra/Blanca)
  Half-Duplex)