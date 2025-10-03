#ifndef RV32_EMU_GDB_H
#define RV32_EMU_GDB_H

#include "stdinc.h"
#include "str.h"

typedef struct Packet {
    String data;
    u8 checksum;
} Packet;

[[nodiscard]] u8 data_checksum(const String *s);

void Packet_destroy(Packet *packet);

#endif
