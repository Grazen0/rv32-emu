#ifndef RV32_EMU_STR_H
#define RV32_EMU_STR_H

#include "stdinc.h"
#include <stddef.h>
#include <stdlib.h>

typedef struct String {
    char *data;
    size_t size;
    size_t capacity;
} String;

[[nodiscard]] String String_new(void);

[[nodiscard]] String String_with_capacity(size_t initial_capacity);

[[nodiscard]] String String_from(const char *data);

void String_push(String *str, char ch);

void String_push_str(String *str, String other);

void String_push_raw(String *str, const char *other);

void String_push_hex(String *str, u8 byte);

void String_clear(String *str);

void String_destroy(String *str);

#endif
