#!/bin/sh
esptool.py -p /dev/ttyUSB0 -b 460800 \
           --before default_reset --after hard_reset \
           --chip esp32 \
           write_flash --flash_mode dio --flash_size detect --flash_freq 40m \
           0x290000 build/spiffs.bin

