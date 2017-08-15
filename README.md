# esp-stm8-flasher
ESP8266 application for flashing STM8S microcontrollers via [stm8-bootloader](https://github.com/lujji/stm8-bootloader).

## Features
* **Remote terminal** - UART<->WiFi bridge
* **File storage** - store multiple firmware images in flash
* **Web GUI** - manage remote file storage and perform drag-and-drop uploads directly from browser

<p align="center"><img src="https://github.com/lujji/blog/blob/gh-pages/flashing-stm8-with-esp8266/web_gui.png"/></p>

## Build instructions
Precompiled binaries for ESP8266 and STM8S003 are located inside `esp-stm8-flasher/firmware`.

``` bash
git clone --recursive https://github.com/lujji/esp-stm8-flasher
```

Create `./esp-stm8-flasher/esp-open-rtos/include/private_ssid_config.h` with two macro definitions:
``` C
#define WIFI_SSID "mywifissid"
#define WIFI_PASS "my secret password"
```

Build and flash the firmware
``` bash
make html && make -j4 flash
```

## Configuration
You may want to change a few things inside `config.h`:

* `BLOCK_SIZE` - STM8 flash block size. This should match bootloader configuration
* `TELNET_PORT` - remote terminal port
* `RST_PIN` - GPIO pin for resetting STM8

`FLASH_SIZE` and `SPIFFS_SIZE` are set in Makefile according to the flash size of the ESP8266 module.

STM8 bootloader should be compiled with `BOOT_PIN` set to `PD6` - this way RX pin will be used to detect entry condition.

## Wiring
| ESP8266    | STM8     |
|------------|----------|
| GPIO1 (TX) | PD6 (RX) |
| GPIO3 (RX) | PD5 (TX) |
| GPIO5      | nRST     |

## Uploading the firmware
Uploading and flashing the firmware can be done either from Web GUI or the command-line utility:

``` bash
python esfutil.py -p firmware.bin 192.168.100.6
```
This performs three operations: upload file to remote storage, flash the firmware, delete file from storage. Utility supports other commands for manipulating remote file system, see `--help` for more details. This utility requires `websocket` module, which can be installed via pip.

## Remote terminal
This project provides a buffered serial bridge. Default port is 23.
``` bash
telnet 192.168.100.6 23
```
