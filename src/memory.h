#ifndef RV32_EMU_MEMORY_H
#define RV32_EMU_MEMORY_H

#include "stdinc.h"
#include <stddef.h>

typedef enum MemoryResult {
    MemoryResult_Ok,
    MemoryResult_OutOfBounds,
    MemoryResult_ReadFault,
    MemoryResult_WriteFault,
    MemoryResult_ExecuteFault,
} MemoryResult;

typedef struct Memory Memory;

typedef struct Memory {
    u8 (*read)(const Memory *mem, u32 addr);
    u32 (*read_instr)(const Memory *mem, u32 addr);
    void (*write)(Memory *mem, u32 addr, u8 value);
} Memory;

[[nodiscard]] u8 Memory_read(const Memory *mem, u32 addr);

[[nodiscard]] u32 Memory_read_instr(const Memory *mem, u32 addr);

[[nodiscard]] u16 Memory_read_u16_le(const Memory *mem, u32 addr);

[[nodiscard]] u32 Memory_read_u32_le(const Memory *mem, u32 addr);

void Memory_write(Memory *mem, u32 addr, u8 value);

void Memory_write_u16_le(Memory *memory, u32 addr, u16 value);

void Memory_write_u32_le(Memory *memory, u32 addr, u32 value);

typedef enum SegPerms : u8 {
    SegPerms_None = 0,
    SegPerms_Read = 1 << 0,
    SegPerms_Write = 1 << 1,
    SegPerms_Execute = 1 << 2,
} SegPerms;

typedef struct Segment {
    u32 addr;
    u32 size;
    u8 perms;
} Segment;

typedef struct SegmentedMemory {
    Memory mem;
    u8 *data;
    Segment *segments;
    size_t segments_size;
} SegmentedMemory;

[[nodiscard]] SegmentedMemory SegmentedMemory_new(void);

void SegmentedMemory_add_segment(SegmentedMemory *mem, Segment seg);

void SegmentedMemory_destroy(SegmentedMemory *mem);

#endif
