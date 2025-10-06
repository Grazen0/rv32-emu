#include "str.h"
#include "macros.h"
#include "util.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void String_reallocate(String *const str, const size_t new_capacity)
{
    char *const new_data = realloc(str->data, (new_capacity + 1) * sizeof(*new_data));

    if (new_data == nullptr)
        BAIL("Could not reallocate string");

    str->data = new_data;
    str->capacity = new_capacity;
}

String String_new(void)
{
    return String_with_capacity(0);
}

String String_with_capacity(const size_t initial_capacity)
{
    String s = {
        .data = malloc((initial_capacity + 1) * sizeof(*s.data)),
        .size = 0,
        .capacity = initial_capacity,
    };

    s.data[0] = '\0';
    return s;
}

String String_from(const char *const data)
{
    if (data == nullptr)
        return String_new();

    const size_t size = strlen(data);

    String s = {
        .data = malloc((size + 1) * sizeof(*data)),
        .size = size,
        .capacity = size,
    };

    memcpy(s.data, data, (size + 1) * sizeof(*data));
    return s;
}

void String_push(String *const str, const char ch)
{
    if (str->size == str->capacity)
        String_reallocate(str, str->capacity == 0 ? 1 : 2 * str->capacity);

    str->data[str->size] = ch;
    ++str->size;
    str->data[str->size] = '\0';
}

void String_push_str(String *const str, const String other)
{
    if (other.size == 0)
        return;

    if (str->size + other.size >= str->capacity) {
        const size_t new_capacity = sz_max(2 * str->capacity, str->size + other.size);
        String_reallocate(str, new_capacity);
    }

    memcpy(str->data + str->size, other.data, other.size * sizeof(*other.data));

    str->size += other.size;
    str->data[str->size] = '\0';
}

void String_push_raw(String *const str, const char *const other)
{
    const size_t other_size = strlen(other);

    if (other_size == 0)
        return;

    if (str->size + other_size >= str->capacity) {
        const size_t new_capacity = sz_max(2 * str->capacity, str->size + other_size);
        String_reallocate(str, new_capacity);
    }

    memcpy(str->data + str->size, other, other_size * sizeof(*other));

    str->size += other_size;
    str->data[str->size] = '\0';
}

void String_push_hex(String *const str, const u8 byte)
{
    char buf[3] = {};
    sprintf(buf, "%02x", byte);
    String_push_raw(str, buf);
}

void String_clear(String *const str)
{
    free(str->data);

    str->data = malloc(1);
    str->data[0] = '\0';
    str->size = 0;
    str->capacity = 0;
}

void String_destroy(String *const str)
{
    free(str->data);
    str->data = nullptr;
    str->capacity = 0;
    str->size = 0;
}
