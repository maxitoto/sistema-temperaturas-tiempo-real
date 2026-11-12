WiFi/
├── WiFiBase/
│   ├── WiFiInicializador.hpp        // setup(): AP+STA dual, credenciales, IPs
│   └── WiFiRuntime.hpp              // loop(): estado, peer list, keepalive
├── WiFiTcpTls/
│   ├── WiFiTcpTlsInicializador.hpp  // setup(): servidor TLS (AP) + cliente TLS (STA)
│   └── WiFiTcpTlsRuntime.hpp        // loop(): I/O de streams TLS
├── WiFiUdpPlain/
│   ├── WiFiUdpPlainInicializador.hpp // setup(): socket UDP shared
│   └── WiFiUdpPlainRuntime.hpp       // loop(): send/recv datagramas planos
└── utils/certs/                     // PEM reales (generar con openssl, no placeholder)