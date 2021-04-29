#!/bin/sh
# Upload complete firmware including bootloader, partition table, fimrware and
# SPI file system onto a connected ESP32 module using esptool which must be
# installed.
#
# This script goes into the binary firmware .ZIP file for distribution.
# Ulrich Lukas 2020-11-12
ESPTOOL_BIN="$(which esptool)"
if [ ! -x "$ESPTOOL_BIN" ]; then
    ESPTOOL_BIN="$(which esptool.py)"
fi
if [ ! -x "$ESPTOOL_BIN" ]; then
    echo "\nERROR: Did not find the esptool or esptool.py executable!"
    echo "This must be installed first. Aborting.."
    exit 1
fi
"$ESPTOOL_BIN" --chip esp32 --baud 921600 --before default_reset --after hard_reset \
    write_flash --flash_mode dio --flash_freq 40m --flash_size detect \
    0x1000 bootloader.bin \
    0x8000 partitions.bin \
    0x10000 firmware.bin \
    0x290000 spiffs.bin
