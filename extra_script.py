import os
Import("env")

# General options that are passed to the C and C++ compilers
# env.Append(CCFLAGS=["flag1", "flag2"])

# General options that are passed to the C compiler (C only; not C++).
# env.Append(CFLAGS=["flag1", "flag2"])

# General options that are passed to the C++ compiler
env.Append(CXXFLAGS=["-std=gnu++17"])

print("UL ===> COMMAND_LINE_TARGETS: ", COMMAND_LINE_TARGETS)
####################################################################################
# Ulrich Lukas 2020-09-19: Account for changed bootloader and partition offsets
if False and "upload" in COMMAND_LINE_TARGETS and "uploadfs" not in COMMAND_LINE_TARGETS:
    print("UL ===> Using custom upload command for non-standard partitioning scheme...")
    # From .platformio/platforms/espressif32/builder/frameworks/espidf.py
    env.Replace(FLASH_EXTRA_IMAGES=[
                ("0x1000", os.path.join("$BUILD_DIR", "bootloader.bin")),
                ("0x18000", os.path.join("$BUILD_DIR", "partitions.bin")),
                ])
    # From .platformio/platforms/espressif32/builder/main.py
    env.Replace(UPLOADERFLAGS=[
                "--chip", "esp32",
                "--port", '"$UPLOAD_PORT"',
                "--baud", "$UPLOAD_SPEED",
                "--before", "default_reset",
                "--after", "hard_reset",
                "write_flash", "-z",
                "--flash_mode", "${__get_board_flash_mode(__env__)}",
                "--flash_freq", "${__get_board_f_flash(__env__)}",
                "--flash_size", "detect",
                ])
    # From .platformio/platforms/espressif32/builder/main.py
    for image in env.get("FLASH_EXTRA_IMAGES", []):
        env.Append(UPLOADERFLAGS=[image[0], env.subst(image[1])])
        env.Replace(UPLOADCMD='"$PYTHONEXE" "$UPLOADER" $UPLOADERFLAGS 0x20000 $SOURCE')
    print("UL ===> UPLOADCMD: ", env.subst("$UPLOADCMD"))
####################################################################################
