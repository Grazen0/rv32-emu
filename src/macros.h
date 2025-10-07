#ifndef RV32_EMU_MACROS_H
#define RV32_EMU_MACROS_H

#include <stdio.h>
#include <stdlib.h>

#define BAIL(...)                                                                                  \
    do {                                                                                           \
        fprintf(stderr, "BAIL (%s:%d)\n", __FILE__, __LINE__);                                     \
        __VA_OPT__(fprintf(stderr, __VA_ARGS__);)                                                  \
        __VA_OPT__(fprintf(stderr, "\n");)                                                         \
        exit(1);                                                                                   \
    } while (0)

#define BAIL_IF(cond, ...)                                                                         \
    do {                                                                                           \
        if (cond) {                                                                                \
            fprintf(stderr, "BAIL_IF('%s') (%s:%d)\n", #cond, __FILE__, __LINE__);                 \
            __VA_OPT__(fprintf(stderr, __VA_ARGS__);)                                              \
            __VA_OPT__(fprintf(stderr, "\n");)                                                     \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

#endif
