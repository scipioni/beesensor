# beesensor

DFRobot FireBeetle 2 ESP32 C6
- [dfrobot datasheet](https://wiki.dfrobot.com/SKU_DFR1075_FireBeetle_2_Board_ESP32_C6)
- [schematics](https://dfimg.dfrobot.com/5d57611a3416442fa39bffca/wiki/65df25004a7d1e8bc128894c75ce4089.pdf)
- [platformio beetle c3](https://docs.platformio.org/en/latest/boards/espressif32/dfrobot_beetle_esp32c3.html#board-espressif32-dfrobot-beetle-esp32c3)
- [aliexpress](https://it.aliexpress.com/item/1005006449798923.html?spm=a2g0o.order_list.order_list_main.11.61fb3696c7SVgF&gatewayAdapt=glo2ita)
- [reduce power consumption](https://diyi0t.com/reduce-the-esp32-power-consumption/)
- [esp32-c6 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)

```
[env:dfrobot_beetle_esp32c3]
platform = espressif32
board = dfrobot_beetle_esp32c3

; microcontroller
board_build.mcu = esp32c6

; MCU frequency
board_build.f_cpu = 160000000L
```


## zigbee2mqtt

install docker
```
yay -S docker docker-buildx docker-compose
sudo usermod -a -G docker $USER
sudo systemctl enable docker
reboot

```

start 
```
cp env.sample .env
touch runtime/devices.yaml
docker compose up
```

frontend zigbee2mqq at http://localhost:8099

## zigbee

- https://github.com/dagrende/HA_on_off_light


## esp-idf

install esp-idf extension and configure with v5.2.2 version

python virtualenv
```bash
python -mvenv --system-site-packages .venv
source .venv/bin/activate

# configure virtualenv (only once)
source $HOME/esp/v5.2.2/esp-idf/install.fish

# activate esp virtualenv
source $HOME/esp/v5.2.2/esp-idf/export.fish

# build environment and ./managed_components (only once)
rm -fR build
idf.py set-target esp32c6

# erasing nvram (only once, )
idf.py -p /dev/ttyACM0 erase-flash

# flash and show logs (CTRL+] to exit)
idf.py -p /dev/ttyACM0 flash monitor



```




## install platformio on archlinux
To install on arch linux:
```
yay codium
```

- Select and install "vscodium-bin-marketplace"

- If inside codium you still don't see platformio on the extensions marketplace then follow this:


- Create this file:
~/.config/VSCodium/product.json

- Put this inside the file:
```
{
  "extensionsGallery": {
    "serviceUrl": "https://marketplace.visualstudio.com/_apis/public/gallery",
    "cacheUrl": "https://vscode.blob.core.windows.net/gallery/index",
    "itemUrl": "https://marketplace.visualstudio.com/items",
    "controlUrl": "",
    "recommendationsUrl": ""
  }
}
```

- Reload codium
- Install platformio from the list of extensions


## platformio setup




udev rules
```
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules | sudo tee /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# for arch users
sudo usermod -a -G uucp $USER
sudo usermod -a -G lock $USER
```

## deepsleep
zigbee2mqtt/FRIENDLY_NAME/set

Publishing messages to this topic allows you to control your Zigbee devices or groups via MQTT. Only accepts JSON messages. An example to control a Philips Hue Go (7146060PH). How to control a specific device can be found in the Exposes section on the device page which can be accessed via "Supported devices".

```
{
  "state": "ON", // Or "OFF", "TOGGLE"
  "brightness": 255, // Value between 0 and 255
  "color": {"x": 0.123, "y": 0.123} // Color in XY
}
```

If FRIENDLY_NAME refers to a group, it will set the state for all devices in that group.

