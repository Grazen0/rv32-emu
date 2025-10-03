#include "gdb.h"

u8 data_checksum(const String *const s)
{
    u8 checksum = 0;

    for (size_t i = 0; i < s->size; ++i)
        checksum += (u8)s->data[i];

    return checksum;
}

void Packet_destroy(Packet *const packet)
{
    String_destroy(&packet->data);
    packet->checksum = 0;
}
