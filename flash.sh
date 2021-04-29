#!/bin/sh
esptool.py -p /dev/ttyUSB0 -b 460800 \
           --before default_reset --after hard_reset \
           --chip esp32 \
           write_flash --flash_mode dio --flash_size detect --flash_freq 40m \
           0x1000 build/bootloader/bootloader.bin \
           0x8000 build/partition_table/partition-table.bin \
           0xe000 build/ota_data_initial.bin \
           0x10000 build/esp_ajax_if.bin \
           0x290000 spiffs.bin

