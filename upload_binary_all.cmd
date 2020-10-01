python -m pip install esptool

esptool --chip esp32 --baud 921600 --before default_reset --after hard_reset^
    write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect^
    0x1000 build/bootloader.bin^
    0x18000 build/partitions.bin^
    0x20000 build/firmware.bin^
    0x300000 build/spiffs.bin