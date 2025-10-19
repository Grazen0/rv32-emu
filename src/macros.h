#ifndef RV32_EMU_MACROS_H
#define RV32_EMU_MACROS_H

#include <stdio.h>
#include <stdlib.h>

// See
// https://www.reddit.com/r/C_Programming/comments/1nou8f8/pros_and_cons_of_this_style_of_vtable_interface/
#define CONTAINER_OF(ptr, Type, member)                                                            \
    ((Type *)((char *)(1 ? (ptr) : &((Type *)0)->member) - offsetof(Type, member)))

#define BAIL(...)                                                                                  \
    do {                                                                                           \
        fprintf(stderr, "BAIL (%s:%d)\n", __FILE__, __LINE__);                                     \
        __VA_OPT__(fprintf(stderr, __VA_ARGS__);)                                                  \
        __VA_OPT__(fprintf(stderr, "\n");)                                                         \
        exit(1);                                                                                   \
    } while (0)

#endif
