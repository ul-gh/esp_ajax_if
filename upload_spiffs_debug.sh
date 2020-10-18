#!/bin/sh

esptool --chip esp32 --baud 921600 --before default_reset --after hard_reset \
    write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
    0x290000 .pio/build/debug/spiffs.bin
