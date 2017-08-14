** work-in-progrss

# esp-stm8-flasher
ESP8266 application for flashing STM8S microcontrollers via [stm8-bootloader](https://github.com/lujji/stm8-bootloader).

## Features
* **file storage** - store multiple firmware images in flash
* **web GUI** - manage remote file storage and perform drag-and-drop uploads directly from browser
* **remote terminal** - UART<->WiFi bridge

## Build instructions

``` bash
git clone --recursive https://github.com/lujji/esp-stm8-flasher
```

Create `esp-open-rtos/include/private_ssid_config.h` with two macro definitions:
``` C
#define WIFI_SSID "mywifissid"
#define WIFI_PASS "my secret password"
```

Build and flash the firmware
``` bash
make html && make -j4 flash
```

## Configuration

## Connection diagram

## Uploading the firmware
``` bash
python esfutil.py -p firmware.bin 192.168.100.6
```
This performs three operations: upload file to remote storage, flash the firmware, delete file from storage. Utility supports other commands for manipulating remote file system, see `--help` for more details.

## Remote terminal
This project provides a buffered UART<->WiFi bridge. Default port is 23.
``` bash
telnet 192.168.100.6 23
```
