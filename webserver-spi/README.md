
### Features

* esp32-c6
* dev board
* web server
* spi controller
* web client
* connect to SPI device
* read / write registers

### Development

dmesg -w

```
[ 3775.004942] usb 1-9.3.3: new full-speed USB device number 12 using xhci_hcd
[ 3775.111062] usb 1-9.3.3: New USB device found, idVendor=303a, idProduct=1001, bcdDevice= 1.02
[ 3775.111080] usb 1-9.3.3: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[ 3775.111085] usb 1-9.3.3: Product: USB JTAG/serial debug unit
[ 3775.111089] usb 1-9.3.3: Manufacturer: Espressif
[ 3775.111092] usb 1-9.3.3: SerialNumber: 40:4C:CA:46:15:D4
[ 3775.803166] cdc_acm 1-9.3.3:1.0: ttyACM0: USB ACM device
[ 3775.803199] usbcore: registered new interface driver cdc_acm
[ 3775.803202] cdc_acm: USB Abstract Control Model driver for USB modems and ISDN adapters
```

esp-idf v5.1 (master)

Modify `src/secrets.h` for WiFi Auth

```bash
cd ~/work/github/espressif/esp-idf
./install.sh esp32c6

. ~/work/github/espressif/esp-idf/export.sh

idf.py set-target esp32c6

idf.py build # just build

idf.py -p /dev/ttyACM0 flash # build & flash

idf.py -p /dev/ttyACM0 flash monitor # build & flash & monitor -> exit Ctrl+]
```

https://github.com/wuxx/nanoESP32-C6/blob/master/README_en.md

### SPI

https://github.com/espressif/esp-idf/blob/master/examples/peripherals/spi_master/lcd/main/spi_master_example_main.c
