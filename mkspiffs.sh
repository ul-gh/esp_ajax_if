#!/bin/sh
#/home/ulrich/.platformio/packages/tool-mkspiffs/mkspiffs_espressif32_espidf \
/home/ulrich/.platformio/packages/tool-mkspiffs/mkspiffs_espressif32_arduino \
    -c src/data -p 256 -b 4096 -s 1507328 .pio/build/debug/spiffs.bin
