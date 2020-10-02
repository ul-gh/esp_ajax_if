import os
from zipfile import ZipFile, ZIP_DEFLATED
Import("env")

# General options that are passed to the C and C++ compilers
# env.Append(CCFLAGS=["flag1", "flag2"])

# General options that are passed to the C compiler (C only; not C++).
# env.Append(CFLAGS=["flag1", "flag2"])

# General options that are passed to the C++ compiler.
env.Append(CXXFLAGS=["-std=gnu++17"]) # We use C++ 17


################################################################################
# Ulrich Lukas 2020-09-19: Account for changed bootloader and partition offsets
if "upload" in COMMAND_LINE_TARGETS and "uploadfs" not in COMMAND_LINE_TARGETS:
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


################################################################################
# Create ZIP archive containing firmware binary images for stand-alone deployment
def zip_bin(target, source, env):
    build_folder = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    dest_filename = os.path.join(project_dir, "firmware.zip")
    print(build_folder)
    print("\u001b[36;1mCreating ZIP firmware images in: " + dest_filename)
    with ZipFile(dest_filename, 'w', compression=ZIP_DEFLATED) as zip_f:
        zip_f.write(os.path.join(build_folder, "bootloader.bin"), "bootloader.bin")
        zip_f.write(os.path.join(build_folder, "partitions.bin"), "partitions.bin")
        zip_f.write(os.path.join(build_folder, "firmware.bin"), "firmware.bin")
        zip_f.write(os.path.join(build_folder, "spiffs.bin"), "spiffs.bin")
        zip_f.write(os.path.join(project_dir, "upload_binary_all.cmd"), "upload_binary_all.cmd")
        zip_f.write(os.path.join(project_dir, "upload_binary_all.sh"), "upload_binary_all.sh")

env.AddCustomTarget(
    name="bin_zip",
    dependencies=[
        "buildprog",
        "buildfs", # This does not seem to work
        "$BUILD_DIR/bootloader.bin",
        "$BUILD_DIR/partitions.bin",
        "$BUILD_DIR/firmware.bin",
        "$BUILD_DIR/spiffs.bin",
    ],
    actions=[zip_bin],
    title="Binary Zipfile",
    description="Create ZIP archive containing firmware binary images"
)


################################################################################
# Some debug output
print("BUILD_TARGETS: " + " ".join(map(str, BUILD_TARGETS)))

# Info hook shows how the upload command is invoked
def before_upload(source, target, env):
    print("UL ===> UPLOADCMD: {} {}\n".format(
        env.subst("$UPLOADCMD"), str(source[0])))

env.AddPreAction("upload", before_upload)
env.AddPreAction("uploadfs", before_upload)

# Info hook shows when individual files are built (successfully or not)
def after_build(source, target, env):
    print("BUILD_TARGETS: ", " ".join(map(str, BUILD_TARGETS)))
    print("\u001b[31;1mCreated TARGET: "
          + env.subst("$PROJECT_DIR/{}".format(str(target[0]))
          ))
    #shutil.copy()

env.AddPostAction("$BUILD_DIR/bootloader.bin", after_build)
env.AddPostAction("$BUILD_DIR/partitions.bin", after_build)
env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
env.AddPostAction("$BUILD_DIR/spiffs.bin", after_build)