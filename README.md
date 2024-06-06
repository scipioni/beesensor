# beesensor

DFRobot FireBeetle 2 ESP32 C6
- [dfrobot datasheet](https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6)
- [schematics](https://dfimg.dfrobot.com/5d57611a3416442fa39bffca/wiki/65df25004a7d1e8bc128894c75ce4089.pdf)
- [platformio beetle c3](https://docs.platformio.org/en/latest/boards/espressif32/dfrobot_beetle_esp32c3.html#board-espressif32-dfrobot-beetle-esp32c3)
- [aliexpress](https://it.aliexpress.com/item/1005006449798923.html?spm=a2g0o.order_list.order_list_main.11.61fb3696c7SVgF&gatewayAdapt=glo2ita)

```
[env:dfrobot_beetle_esp32c3]
platform = espressif32
board = dfrobot_beetle_esp32c3

; microcontroller
board_build.mcu = esp32c6

; MCU frequency
board_build.f_cpu = 160000000L
```
