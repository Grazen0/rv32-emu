#ifndef RV32_EMU_NUMERIC_H
#define RV32_EMU_NUMERIC_H

#include "stdinc.h"
#include <stddef.h>

/**
 * \brief Returns the greater of two size_t's.
 *
 * \param a A number.
 * \param b Another number.
 *
 * \return a if a > b, b otherwise.
 */
[[nodiscard]] size_t sz_max(size_t a, size_t b);

/**
 * \brief Returns whether a number is a power of 2 or not.
 *
 * \param n The number to check.
 *
 * \return true if n is an integer power of 2, false otherwise.
 */
[[nodiscard]] bool u32_is_pow2(u32 n);

#endif
