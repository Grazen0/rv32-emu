#include "numeric.h"
#include "stdinc.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

size_t sz_max(const size_t a, const size_t b)
{
    return a > b ? a : b;
}

bool u32_is_pow2(const u32 n)
{
    return (n & (n - 1)) == 0;
}
