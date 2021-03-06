/* Basic file input/output
 * 
 * License: GPL v.3 
 * U. Lukas 2020-12-24
 */
#include "esp_spiffs.h"
#include <SPIFFS.h>
#include <FS.h>

#include "fs_io.hpp"

static auto TAG = "fs_io.cpp";


bool FSIO::write_to_file_uint8(const char *filename, uint8_t *buf, size_t len) {
    if (!esp_spiffs_mounted(NULL) && !SPIFFS.begin(false)) {
        ESP_LOGE(TAG, "Could not open SPIFFS!");
        return false;
    }
    auto file = SPIFFS.open(filename, "w");
    if (!file) {
        ESP_LOGE(TAG, "Could not open file for writing!: %s", filename);
        return false;
    }
    auto written = file.write(buf, len);
    file.close();
    if (written == len) {
        ESP_LOGI(TAG, "Wrote %d bytes to file: %s", written, filename);
        return true;
    } else {
        ESP_LOGE(TAG, "Error writing to file: %s. File size: %d;  written: %d",
                    filename, len, written);
        return false;
    }
}


size_t FSIO::read_from_file_uint8(const char *filename, uint8_t *buf, size_t max_len) {
    if (!esp_spiffs_mounted(NULL) && !SPIFFS.begin(false)) {
        ESP_LOGE(TAG, "Could not open SPIFFS!");
        return 0;
    } else if (!SPIFFS.exists(filename)) {
        ESP_LOGE(TAG, "File does not exist: %s", filename);
        return 0;
    }
    auto file = SPIFFS.open(filename, "r");
    if (!file) {
        ESP_LOGE(TAG, "Could not open file!: %s", filename);
        return 0;
    }
    auto file_size = file.size();
    if (file_size > max_len) {
        ESP_LOGE(TAG, "Error, file too large: %s. File size: %d;  max_len: %d",
                    filename, file_size, max_len);
        return 0;
    }
    auto read = file.read(buf, file_size);
    file.close();
    if (read == file_size) {
        ESP_LOGI(TAG, "Read %d bytes from file: %s", read, filename);
        return read;
    } else {
        ESP_LOGE(TAG, "Error reading from file: %s. File size: %d;  read: %d",
                    filename, file_size, read);
        return read;
    }
}