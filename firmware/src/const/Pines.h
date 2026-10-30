// Archivo: ./src/const/Pines.h
#pragma once
#include <stdint.h>

// =============================================================================
// DIAGRAMA FÍSICO: NODEMCU-32S (Módulo ESP-WROOM-32 - 38 Pines)
// =============================================================================
//
//                               ┌───────────────────┐
//                       ✓  3V3  │ 1              38 │ GND ✓
//                   [RST]  EN   │ 2              37 │ GPIO23 (VSPI_MOSI) -> [SD_MOSI] ✓
//        (ADC1_0 / IN)  GPIO36  │ 3  (VP)        36 │ GPIO22 (I2C_SCL)   -> [OLED_SCL] ✓
//        (ADC1_3 / IN)  GPIO39  │ 4  (VN)        35 │ GPIO1  (UART0_TX)  -> [USB_TX]
//        (ADC1_6 / IN)  GPIO34  │ 5              34 │ GPIO3  (UART0_RX)  -> [USB_RX]
//        (ADC1_7 / IN)  GPIO35  │ 6              33 │ GPIO21 (I2C_SDA)   -> [OLED_SDA] ✓
//        (ADC1_4 / IO)  GPIO32  │ 7              32 │ GND
//        (ADC1_5 / IO)  GPIO33  │ 8              31 │ GPIO19 (VSPI_MISO) -> [SD_MISO] ✓
// ✓[LED] (DAC1   / IO)  GPIO25  │ 9              30 │ GPIO18 (VSPI_SCK)  -> [SD_SCK] ✓
// ✓[BUZ] (DAC2   / IO)  GPIO26  │ 10             29 │ GPIO5  (VSPI_CS0)  -> [SD_CS] ✓
//        (ADC2_7 / IO)  GPIO27  │ 11             28 │ GPIO17 (UART2_TX)
//        (HSPI_CLK)     GPIO14  │ 12             27 │ GPIO16 (UART2_RX)
//        (HSPI_MISO)    GPIO12* │ 13             26 │ GPIO4  (1-WIRE)    -> [SENSOR_TEMP] ✓
//                          GND  │ 14             25 │ GPIO0* (BOOT_BTN)
//        (HSPI_MOSI)    GPIO13  │ 15             24 │ GPIO2* (LED_INTERNO)
//        [FLASH_D2]     GPIO9 ! │ 16             23 │ GPIO15*(HSPI_CS0)
//        [FLASH_D3]     GPIO10! │ 17             22 │ GPIO8 ! [FLASH_D1]
//        [FLASH_CMD]    GPIO11! │ 18             21 │ GPIO7 ! [FLASH_D0]
//                     ✓ VIN 5V  │ 19             20 │ GPIO6 ! [FLASH_CLK]
//                               └─────────┬─────────┘
//                                     [USB-UART]
//
//  Referencias:
//   [!] PROHIBIDOS (GPIO 6..11): Conectados a la memoria SPI Flash interna.
//   [*] STRAPPING PINS (GPIO 0, 2, 5, 12, 15): Afectan el modo de booteo.
//       Importante: GPIO12 (MTDI) NO debe tener pull-up externo al arrancar
//       porque cambia el voltaje de la Flash interna a 1.8V y no bootea.
//   (IN) SOLO ENTRADA (GPIO 34, 35, 36, 39): No tienen pull-up/down ni modo OUTPUT.
// =============================================================================

namespace Pines {

    // --- 1. UART0: PUERTO USB DE COMUNICACIÓN OSI (Medio 0) ---
    static constexpr int PIN_USB_TX          = 1;
    static constexpr int PIN_USB_RX          = 3;

    // --- 2. UART2: EXPANSIÓN SERIAL HARDWARE LIBRE ---
    static constexpr int PIN_UART2_TX        = 17;
    static constexpr int PIN_UART2_RX        = 16;

    // --- 3. BUS VSPI: ALMACENAMIENTO FÍSICO MICROSD (SGBD) ---
    static constexpr int PIN_SD_CS           = 5;
    static constexpr int PIN_SD_SCK          = 18;
    static constexpr int PIN_SD_MISO         = 19;
    static constexpr int PIN_SD_MOSI         = 23;

    // --- 4. BUS I2C: PANTALLA OLED DE DIAGNÓSTICO ---
    static constexpr int PIN_I2C_SDA         = 21;
    static constexpr int PIN_I2C_SCL         = 22;

    // --- 5. SENSORES Y ACTUADORES DEL DOMINIO (HELADERA / LABORATORIO) ---
    static constexpr int PIN_SENSOR_TEMP     = 4;   // Bus 1-Wire (DS18B20)
    static constexpr int PIN_LED_EXTERNO     = 25;  // Indicador visual de estado/alarma
    static constexpr int PIN_BOCINA          = 26;  // Alarma acústica PWM / DAC2
    static constexpr int PIN_LED_PLACA       = 2;   // LED azul integrado en el NodeMCU-32S
    static constexpr int PIN_BOTON_BOOT      = 0;   // Botón físico en placa (Factory Reset / AP)

    // --- 6. ENTRADAS EXCLUSIVAS Y ADC1 (Libres de conflicto con Wi-Fi) ---
    static constexpr int PIN_ADC1_CH4_LIBRE  = 32;
    static constexpr int PIN_ADC1_CH5_LIBRE  = 33;
    static constexpr int PIN_IN_SOLO_34      = 34;
    static constexpr int PIN_IN_SOLO_35      = 35;
    static constexpr int PIN_IN_VP_36        = 36;
    static constexpr int PIN_IN_VN_39        = 39;

    // --- 7. BUS HSPI / GPIO SECUNDARIOS ---
    static constexpr int PIN_GPIO_13_LIBRE   = 13;
    static constexpr int PIN_GPIO_14_LIBRE   = 14;
    static constexpr int PIN_GPIO_27_LIBRE   = 27;

}

// =============================================================================
// ALIAS GLOBALES (Compatibilidad con el código existente del firmware)
// =============================================================================
using Pines::PIN_SENSOR_TEMP;
using Pines::PIN_BOCINA;
using Pines::PIN_I2C_SDA;
using Pines::PIN_I2C_SCL;

// Alias específicos mapeados a la nueva estructura
static constexpr int PIN_LED   = Pines::PIN_LED_EXTERNO;
static constexpr int PIN_CS_SD = Pines::PIN_SD_CS;