| Supported Targets | ESP32-C6 |
| ----------------- | -------- |

# Light Button Example
Whith the press of the button on the board, the led 15 lights up and, with another press, it goes off.


## Hardware Required

* One development board with ESP32-C6 SoC
* A USB cable for power supply and programming


## ESP-IDF


install
```
yay -S esp-idf
```

use ESP-IDF

1. Run /opt/esp-idf/install.sh to install ESP-IDF to ~/.espressif.
   You only have to do this once after installing or upgrading
   the esp-idf package.

2. Run ```source /opt/esp-idf/export.sh``` to add idf.py and idf_tools.py
   to your current PATH. You will have to do this in every terminal
   where you want to use ESP-IDF. Alternatively, you can add
   "source /opt/esp-idf/export.sh" to ~/.bashrc or the equivalent for
   your shell to load ESP-IDF automatically in all terminals.


## Configure the project

Before project configuration and build, make sure to set the correct chip target using `idf.py set-target TARGET` command.

```
idf.py set-target esp32c6
```

## Erase the NVRAM 

Before flash it to the board, it is recommended to erase NVRAM if user doesn't want to keep the previous examples or other projects stored info 
using `idf.py -p /dev/ttyACM2 erase-flash`

## Build and Flash

Build the project, flash it to the board, and start the monitor tool to view the serial output by running 

```bash
idf.py -p /dev/ttyACM2 flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)
