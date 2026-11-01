#include <Arduino.h>
#include "./Oled.h"
#include "./icons/icons.h"

void setup() {
    iniciarOled();      // Autoinicia con PIN_I2C_SDA (21) y PIN_I2C_SCL (22)

    oled.cabeceraIzq("UNTDF LAB");

    // Actualiza individualmente cada uno de los 6 sensores enviando solo su celda por I2C
    oled.sensorTemperatura(0, -18.4f);
    oled.sensorTemperatura(1, -17.9f);
    oled.sensorTemperatura(2, -18.1f);
    oled.sensorTemperatura(3, -12.5f);
    oled.sensorTemperatura(4, 0.0f, false); // Muestra "S5: ERR"
    oled.sensorTemperatura(5, -18.0f);

    delay(5000);
    oled.limpiarTodo();
    oled.slotDer1(person_icon16x16);
    oled.slotDer2(wifi1_icon16x16);
    oled.slotDer3(wifi2_icon16x16);
    oled.slotDer4(wifi3_icon16x16);
    oled.dibujarDivisor(Oled::NINGUNA);
    oled.cabeceraIzq("31/10/2026");

    for (size_t i = 0; i < 4; i++)
    {   
        if(i == 0) {oled.slotDer1(wifi4_icon16x16);}
        if(i == 1) {oled.slotDer1(wifi3_icon16x16);}
        if(i == 2) {oled.slotDer1(wifi2_icon16x16);}
        if(i == 3) {oled.slotDer1(wifi1_icon16x16);}
        delay(1000);
    }

	delay(5000);
	oled.dibujarDivisor(Oled::PARCIAL);
	oled.slotDer1(bluetooth_icon16x16);
	oled.slotDer2(person_icon16x16);
	oled.slotDer4(icon_serial_connected_msb);

    delay(5000);
    oled.limpiarTodo();
    oled.fullScreen(icon_untdf, 128, 48);

    delay(5000);
    oled.dibujarDivisor(Oled::PUNTEADA);
    oled.slotDer1(person_icon16x16);
    oled.slotDer2(wifi1_icon16x16);
    oled.slotDer3(wifi2_icon16x16);
    oled.slotDer4(wifi3_icon16x16);
    oled.cabeceraIzq("31/10/2026");
    
    oled.fullScreenConCabecera(icon_untdf, 128, 48);

}

void loop() {}