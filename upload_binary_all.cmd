::Flash upload binary firmware images to ESP32 microcontroller DEV board.
::This uses the esptool python package.
::If none is found, an install is tried first
::This script goes into the binary firmware .ZIP file for distribution.
::2021-05-11 Ulrich Lukas
@echo off
SETLOCAL

set "second_run=false"
:check_for_esptool
set "esptool_command=esptool"
where "%esptool_command%" >nul 2>nul || (
    echo No "esptool" executable found. Trying "esptool.py"..
    set "esptool_command=esptool.py"
)
where "%esptool_command%" >nul 2>nul || (
    if %second_run% == true (
        echo ERROR: Again, "esptool" is not installed. Giving up!
        goto end
    )
    echo No "esptool.py" executable found. Trying install..	
    where python >nul 2>nul || (
        echo ERROR: No Python interpreter found. Install Python first!
        goto end
    )
    set "second_run=true"
    python -m pip install esptool
    goto check_for_esptool
)

:: Found the esptool. Continuing.
echo Using %esptool_command% to upload binary firmware to the ESP32...
"%esptool_command%"^
    --chip esp32 --baud 460800^
    --before default_reset --after hard_reset^
    write_flash --flash_mode dio --flash_freq 40m --flash_size detect^
    0x1000 build/bootloader/bootloader.bin^
    0x8000 build/partition_table/partition-table.bin^
    0xe000 build/ota_data_initial.bin^
    0x10000 build/esp_ajax_if.bin^
    0x290000 build/spiffs.bin

:end
