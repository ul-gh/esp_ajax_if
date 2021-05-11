#!/usr/bin/env python3
# Create ZIP archive containing firmware binary images
import os
from zipfile import ZipFile, ZIP_DEFLATED

project_name = "esp_ajax_if"
project_dir = "."
build_dir = "build"

dest_filename = os.path.join(project_dir, "firmware.zip")

src_files = [
    "bootloader/bootloader.bin",
    "partition_table/partition-table.bin",
    "ota_data_initial.bin",
    f"{project_name}.bin",
    "spiffs.bin",
]

def zip_bin():
    print("\x1b[36;1m  Creating ZIP firmware images in: " + dest_filename)
    with ZipFile(dest_filename, 'w', compression=ZIP_DEFLATED) as zip_f:
        zip_f.write(os.path.join(project_dir, "upload_binary_all.cmd"),
                    "upload_binary_all.cmd")
        zip_f.write(os.path.join(project_dir, "upload_binary_all.sh"),
                    "upload_binary_all.sh")
        for filename in src_files:
            src_path = os.path.join(project_dir, build_dir, filename)
            dest_path = os.path.join(build_dir, filename)
            print(f"Adding to ZIP-File: {src_path}")
            zip_f.write(src_path, dest_path)


if __name__ == "__main__":
    zip_bin()