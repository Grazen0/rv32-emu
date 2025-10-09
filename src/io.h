#ifndef RV32_EMU_IO_H
#define RV32_EMU_IO_H

#include "stdinc.h"
#include <stddef.h>

/**
 * \brief Loads a file into memory.
 *
 * Returns a dynamic array that must be freed with free().
 *
 * \param filename Path of the file to read.
 * \param file_size Out param for the size of the file read.
 *
 * If an error occurs, NULL will be returned and errno will be set.
 *
 * \return The file data in bytes, or NULL if something went wrong.
 */
[[nodiscard]] u8 *load_file(const char *filename, size_t *file_size);

#endif
