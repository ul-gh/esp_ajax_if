/** @file fs_io.hpp
 *
 * @brief Basic file input/output
 * 
 * License: GPL v.3 
 * U. Lukas 2020-12-24
 */
#ifndef FS_IO_HPP__
#define FS_IO_HPP__

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

namespace FSIO {
    bool write_to_file_uint8(const char *filename, uint8_t *buf, size_t len);

    size_t read_from_file_uint8(const char *filename, uint8_t *buf, size_t max_len);
}

#endif