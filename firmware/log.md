Processing esp32s_hardware (platform: espressif32; board: esp32dev; framework: arduino)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/esp32dev.html
PLATFORM: Espressif 32 (7.1.3) > Espressif ESP32 Dev Module
HARDWARE: ESP32 240MHz, 320KB RAM, 4MB Flash
DEBUG: Current (cmsis-dap) External (cmsis-dap, esp-bridge, esp-prog, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa)
PACKAGES: 
 - framework-arduinoespressif32 @ 4.20017.260907+sha.dcc1105b 
 - tool-esptoolpy @ 2.41100.260830 (4.11.0) 
 - tool-mkfatfs @ 2.0.1 
 - tool-mklittlefs @ 1.203.210628 (2.3) 
 - tool-mkspiffs @ 2.230.0 (2.30) 
 - toolchain-xtensa-esp32 @ 8.4.0+2021r2-patch5
Converting main.ino
LDF: Library Dependency Finder -> https://bit.ly/configure-pio-ldf
LDF Modes: Finder ~ chain, Compatibility ~ soft
Found 37 compatible libraries
Scanning dependencies...
Dependency Graph
|-- Adafruit SSD1306 @ 2.5.17
|-- Adafruit GFX Library @ 1.13.0
|-- Adafruit BusIO @ 1.17.4
|-- WiFi @ 2.0.0
|-- SD @ 2.0.0
|-- SPI @ 2.0.0
|-- Wire @ 2.0.0
Building in release mode
Compiling .pio/build/esp32s_hardware/src/main.ino.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFi.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiAP.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiClient.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiGeneric.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiMulti.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiSTA.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiScan.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiServer.cpp.o
Compiling .pio/build/esp32s_hardware/lib5ea/WiFi/WiFiUdp.cpp.o
Archiving .pio/build/esp32s_hardware/lib5ea/libWiFi.a
Indexing .pio/build/esp32s_hardware/lib5ea/libWiFi.a
Linking .pio/build/esp32s_hardware/firmware.elf
Retrieving maximum program size .pio/build/esp32s_hardware/firmware.elf
Checking size .pio/build/esp32s_hardware/firmware.elf
Advanced Memory Usage is available via "PlatformIO Home > Project Inspect"
RAM:   [==        ]  18.4% (used 60400 bytes from 327680 bytes)
Flash: [=======   ]  67.5% (used 885041 bytes from 1310720 bytes)
Building .pio/build/esp32s_hardware/firmware.bin
esptool.py v4.11.0
Creating esp32 image...
Merged 27 ELF sections
Successfully created esp32 image.
Configuring upload protocol...
AVAILABLE: cmsis-dap, esp-bridge, esp-prog, espota, esptool, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa
CURRENT: upload_protocol = esptool
Looking for upload port...
Using manually specified: /dev/cu.usbserial-0001
Uploading .pio/build/esp32s_hardware/firmware.bin
esptool.py v4.11.0
Serial port /dev/cu.usbserial-0001
Connecting....
Chip is ESP32-D0WDQ6 (revision v1.0)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz
MAC: 30:ae:a4:fe:ed:30
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 921600
Changed.
Configuring flash size...
Flash will be erased from 0x00001000 to 0x00005fff...
Flash will be erased from 0x00008000 to 0x00008fff...
Flash will be erased from 0x0000e000 to 0x0000ffff...
Flash will be erased from 0x00010000 to 0x000e9fff...
SHA digest in image updated
Compressed 17536 bytes to 12202...
Writing at 0x00001000... (100 %)
Wrote 17536 bytes (12202 compressed) at 0x00001000 in 0.4 seconds (effective 344.1 kbit/s)...
Hash of data verified.
Compressed 3072 bytes to 146...
Writing at 0x00008000... (100 %)
Wrote 3072 bytes (146 compressed) at 0x00008000 in 0.1 seconds (effective 306.5 kbit/s)...
Hash of data verified.
Compressed 8192 bytes to 47...
Writing at 0x0000e000... (100 %)
Wrote 8192 bytes (47 compressed) at 0x0000e000 in 0.1 seconds (effective 457.2 kbit/s)...
Hash of data verified.
Compressed 891616 bytes to 579354...
Writing at 0x00010000... (2 %)
Writing at 0x0001b114... (5 %)
Writing at 0x00025708... (8 %)
Writing at 0x0003121d... (11 %)
Writing at 0x00036ba5... (13 %)
Writing at 0x0003bd1c... (16 %)
Writing at 0x0004146c... (19 %)
Writing at 0x00046f12... (22 %)
Writing at 0x0004c4df... (25 %)
Writing at 0x00051ca1... (27 %)
Writing at 0x00056eea... (30 %)
Writing at 0x0005c2c2... (33 %)
Writing at 0x000615b6... (36 %)
Writing at 0x0006690a... (38 %)
Writing at 0x0006bc38... (41 %)
Writing at 0x00070dd6... (44 %)
Writing at 0x00076123... (47 %)
Writing at 0x0007b3e9... (50 %)
Writing at 0x00080e6c... (52 %)
Writing at 0x000865bf... (55 %)
Writing at 0x0008bb20... (58 %)
Writing at 0x00090d5e... (61 %)
Writing at 0x0009619a... (63 %)
Writing at 0x0009b698... (66 %)
Writing at 0x000a0d4a... (69 %)
Writing at 0x000a6536... (72 %)
Writing at 0x000ac447... (75 %)
Writing at 0x000b1da4... (77 %)
Writing at 0x000b8381... (80 %)
Writing at 0x000c0df9... (83 %)
Writing at 0x000c8832... (86 %)
Writing at 0x000cda4a... (88 %)
Writing at 0x000d7550... (91 %)
Writing at 0x000dcde0... (94 %)
Writing at 0x000e2058... (97 %)
Writing at 0x000e797a... (100 %)
Wrote 891616 bytes (579354 compressed) at 0x00010000 in 8.8 seconds (effective 814.7 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
--- Terminal on /dev/cu.usbserial-0001 | 115200 8-N-1
--- Available filters and text transformations: debug, default, direct, esp32_exception_decoder, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
d:0x4007800232
load:0x40080400,len:3028
entry 0x400805e4
[    93][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer
[   310][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer

assert failed: tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid mbox)


Backtrace: 0x40083b4d:0x3ffb1e20 0x4008d6d1:0x3ffb1e40 0x40092d6d:0x3ffb1e60 0x400fae05:0x3ffb1f90 0x4010b3a5:0x3ffb1fc0 0x4010b405:0x3ffb1fe0 0x400fa5c5:0x3ffb2030 0x400dec1a:0x3ffb2050 0x400dec94:0x3ffb20a0 0x400d7f4d:0x3ffb20c0 0x400d9a63:0x3ffb20e0 0x400d9d7d:0x3ffb2140 0x400e437a:0x3ffb2290




ELF file SHA256: f55f80f1dceb6309

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
[    93][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer
[   310][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer

assert failed: tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid mbox)


Backtrace: 0x40083b4d:0x3ffb1e20 0x4008d6d1:0x3ffb1e40 0x40092d6d:0x3ffb1e60 0x400fae05:0x3ffb1f90 0x4010b3a5:0x3ffb1fc0 0x4010b405:0x3ffb1fe0 0x400fa5c5:0x3ffb2030 0x400dec1a:0x3ffb2050 0x400dec94:0x3ffb20a0 0x400d7f4d:0x3ffb20c0 0x400d9a63:0x3ffb20e0 0x400d9d7d:0x3ffb2140 0x400e437a:0x3ffb2290




ELF file SHA256: f55f80f1dceb6309

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
[    93][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer
[   310][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer

assert failed: tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid mbox)


Backtrace: 0x40083b4d:0x3ffb1e20 0x4008d6d1:0x3ffb1e40 0x40092d6d:0x3ffb1e60 0x400fae05:0x3ffb1f90 0x4010b3a5:0x3ffb1fc0 0x4010b405:0x3ffb1fe0 0x400fa5c5:0x3ffb2030 0x400dec1a:0x3ffb2050 0x400dec94:0x3ffb20a0 0x400d7f4d:0x3ffb20c0 0x400d9a63:0x3ffb20e0 0x400d9d7d:0x3ffb2140 0x400e437a:0x3ffb2290




ELF file SHA256: f55f80f1dceb6309

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
[    93][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer
[   310][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer

assert failed: tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid mbox)


Backtrace: 0x40083b4d:0x3ffb1e20 0x4008d6d1:0x3ffb1e40 0x40092d6d:0x3ffb1e60 0x400fae05:0x3ffb1f90 0x4010b3a5:0x3ffb1fc0 0x4010b405:0x3ffb1fe0 0x400fa5c5:0x3ffb2030 0x400dec1a:0x3ffb2050 0x400dec94:0x3ffb20a0 0x400d7f4d:0x3ffb20c0 0x400d9a63:0x3ffb20e0 0x400d9d7d:0x3ffb2140 0x400e437a:0x3ffb2290




ELF file SHA256: f55f80f1dceb6309

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
[    93][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer
[   310][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer

assert failed: tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid mbox)


Backtrace: 0x40083b4d:0x3ffb1e20 0x4008d6d1:0x3ffb1e40 0x40092d6d:0x3ffb1e60 0x400fae05:0x3ffb1f90 0x4010b3a5:0x3ffb1fc0 0x4010b405:0x3ffb1fe0 0x400fa5c5:0x3ffb2030 0x400dec1a:0x3ffb2050 0x400dec94:0x3ffb20a0 0x400d7f4d:0x3ffb20c0 0x400d9a63:0x3ffb20e0 0x400d9d7d:0x3ffb2140 0x400e437a:0x3ffb2290




ELF file SHA256: f55f80f1dceb6309

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
[    93][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer
[   310][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer

assert failed: tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid mbox)


Backtrace: 0x40083b4d:0x3ffb1e20 0x4008d6d1:0x3ffb1e40 0x40092d6d:0x3ffb1e60 0x400fae05:0x3ffb1f90 0x4010b3a5:0x3ffb1fc0 0x4010b405:0x3ffb1fe0 0x400fa5c5:0x3ffb2030 0x400dec1a:0x3ffb2050 0x400dec94:0x3ffb20a0 0x400d7f4d:0x3ffb20c0 0x400d9a63:0x3ffb20e0 0x400d9d7d:0x3ffb2140 0x400e437a:0x3ffb2290




ELF file SHA256: f55f80f1dceb6309

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4
[    93][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer
[   310][E][sd_diskio.cpp:806] sdcard_mount(): f_mount failed: (1) A hard error occurred in the low level disk I/O layer

assert failed: tcpip_send_msg_wait_sem IDF/components/lwip/lwip/src/api/tcpip.c:455 (Invalid mbox)


Backtrace: 0x40083b4d:0x3ffb1e20 0x4008d6d1:0x3ffb1e40 0x40092d6d:0x3ffb1e60 0x400fae05:0x3ffb1f90 0x4010b3a5:0x3ffb1fc0 0x4010b405:0x3ffb1fe0 0x400fa5c5:0x3ffb2030 0x400dec1a:0x3ffb2050 0x400dec94:0x3ffb20a0 0x400d7f4d:0x3ffb20c0 0x400d9a63:0x3ffb20e0 0x400d9d7d:0x3ffb2140 0x400e437a:0x3ffb2290




ELF file SHA256: f55f80f1dceb6309

Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:1184
load:0x40078000,len:13232
load:0x40080400,len:3028
entry 0x400805e4