::Flash upload binary firmware images to ESP32 microcontroller DEV board.
::This uses the esptool python package.
::If none is found, an install is tried first
::2020-10-09 Ulrich Lukas
@echo off
SETLOCAL

set second_run="false"
:check_for_esptool
set esptool_command="esptool"
where %esptool_command% >nul 2>nul || (
    echo No "esptool" executable found. Trying "esptool.py"..
    set esptool_command="esptool.py"
    where %esptool_command% >nul 2>nul || (
	if %second_run% == "true" (
	    echo ERROR: Again, "esptool" is not installed. Giving up!
	    goto end
        )
        echo No "esptool.py" executable found. Trying install..	
	where python >nul 2>nul || (
            echo ERROR: No Python interpreter found. Install Python first!
	    goto end
        )
	set second_run="true"
	python -m pip install esptool
	goto check_for_esptool
    )
)

:: Found the esptool. Continuing.
echo Using %esptool_command% to upload binary firmware to the ESP32...
esptool --chip esp32 --baud 921600 --before default_reset --after hard_reset^
    write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect^
    0x1000 bootloader.bin^
    0x18000 partitions.bin^
    0x20000 firmware.bin^
    0x300000 spiffs.bin

:end
