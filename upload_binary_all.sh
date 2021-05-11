#!/bin/sh
# Upload complete firmware including bootloader, partition table, fimrware and
# SPI file system onto a connected ESP32 module using esptool which must be
# installed.
#
# This script goes into the binary firmware .ZIP file for distribution.
# Ulrich Lukas 2021-05-11
ESPTOOL_BIN="$(which esptool)"
if [ ! -x "$ESPTOOL_BIN" ]; then
    ESPTOOL_BIN="$(which esptool.py)"
fi
if [ ! -x "$ESPTOOL_BIN" ]; then
    echo "\nERROR: Did not find the esptool or esptool.py executable!"
    echo "This must be installed first. Aborting.."
    exit 1
fi
"$ESPTOOL_BIN" \
    --chip esp32 --baud 460800 \
    --before default_reset --after hard_reset \
    write_flash --flash_mode dio --flash_freq 40m --flash_size detect \
    0x1000 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0xe000 build/ota_data_initial.bin \
    0x10000 build/esp_ajax_if.bin \
    0x290000 build/spiffs.bin
