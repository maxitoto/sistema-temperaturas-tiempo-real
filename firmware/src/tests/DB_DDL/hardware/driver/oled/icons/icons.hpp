#pragma once
#include <stdint.h>

struct Icono {
    const uint8_t* data;
    uint8_t ancho;
    uint8_t alto;
};

// datos
extern const uint8_t person_icon16x16[];
extern const uint8_t wifi1_icon16x16[];
extern const uint8_t wifi2_icon16x16[];
extern const uint8_t wifi3_icon16x16[];
extern const uint8_t wifi4_icon16x16[];
extern const uint8_t usb_icon16x16[];
extern const uint8_t bluetooth_icon16x16[];
extern const uint8_t icon_serial_connected_msb[];
extern const uint8_t icon_untdf[];

// global
extern const Icono PERSON;
extern const Icono WIFI1;
extern const Icono WIFI2;
extern const Icono WIFI3;
extern const Icono WIFI4;
extern const Icono USB;
extern const Icono BLUETOOTH;
extern const Icono SERIAL_CONNECTED;
extern const Icono LOGO_UNTDF;