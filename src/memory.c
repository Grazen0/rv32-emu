#include "memory.h"
#include "cpu.h"
#include "log.h"
#include "macros.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

[[nodiscard]] static const Segment *find_segment(const SegmentedMemory *const mem, const u32 addr)
{
    for (size_t i = 0; i < mem->segments_size; ++i) {
        const Segment *const seg = &mem->segments[i];

        if (addr >= seg->addr && addr < seg->addr + seg->size)
            return seg;
    }

    return nullptr;
}

[[nodiscard]] static u8 SegmentedMemory_read(const Memory *const mem, const u32 addr)
{
    const SegmentedMemory *const segmem = CONTAINER_OF(mem, SegmentedMemory, mem);
    const Segment *const seg = find_segment(segmem, addr);

    if (seg != nullptr && (seg->perms & SegPerms_Read) == 0)
        BAIL("memory read without permission (0x%08X)", addr);

    return segmem->data[addr];
}

[[nodiscard]] static u32 SegmentedMemory_read_instr(const Memory *const mem, const u32 addr)
{
    if ((addr % 4) != 0)
        BAIL("misaligned instruction read (0x%08X)", addr);

    const SegmentedMemory *const segmem = CONTAINER_OF(mem, SegmentedMemory, mem);
    const Segment *const seg = find_segment(segmem, addr);

    if (seg == nullptr || (seg->perms & SegPerms_Execute) == 0)
        BAIL("tried to read from non-executable section (0x%08X)", addr);

    if (seg != nullptr && addr + 3 >= seg->addr + seg->size)
        BAIL("instruction read out of bounds (0x%08X)", addr);

    const u32 a = segmem->data[addr];
    const u32 b = segmem->data[addr + 1];
    const u32 c = segmem->data[addr + 2];
    const u32 d = segmem->data[addr + 3];

    return a | (b << 8) | (c << 16) | (d << 24);
}

static void SegmentedMemory_write(Memory *const mem, const u32 addr, const u8 value)
{
    const SegmentedMemory *const segmem = CONTAINER_OF(mem, SegmentedMemory, mem);

    for (size_t i = 0; i < segmem->segments_size; ++i) {
        const Segment *const seg = &segmem->segments[i];

        if (addr >= seg->addr && addr < seg->addr + seg->size) {
            if ((seg->perms & SegPerms_Write) == 0)
                BAIL("memory write without permission (0x%08X)", addr);
            else
                break;
        }
    }

    segmem->data[addr] = value;
}

u8 Memory_read(const Memory *const mem, const u32 addr)
{
    return mem->read(mem, addr);
}

u32 Memory_read_instr(const Memory *const mem, const u32 addr)
{
    return mem->read_instr(mem, addr);
}

void Memory_write(Memory *mem, u32 addr, u8 value)
{
    mem->write(mem, addr, value);
}

[[nodiscard]] u16 Memory_read_u16_le(const Memory *const memory, const u32 addr)
{
    if ((addr % 2) != 0)
        BAIL("misaligned half-word read (0x%08X)", addr);

    const u16 a = Memory_read(memory, addr);
    const u16 b = Memory_read(memory, addr + 1);

    return a | (b << 8);
}

[[nodiscard]] u32 Memory_read_u32_le(const Memory *const mem, const u32 addr)
{
    if ((addr % 4) != 0)
        BAIL("misaligned word read (0x%08X)", addr);

    const u32 a = Memory_read(mem, addr);
    const u32 b = Memory_read(mem, addr + 1);
    const u32 c = Memory_read(mem, addr + 2);
    const u32 d = Memory_read(mem, addr + 3);

    return a | (b << 8) | (c << 16) | (d << 24);
}

void Memory_write_u16_le(Memory *const mem, const u32 addr, const u16 value)
{
    if ((addr % 2) != 0) {
        BAIL("misaligned half-word write (0x%08X)", addr);
    }

    Memory_write(mem, addr, (u8)value);
    Memory_write(mem, addr + 1, (u8)(value >> 8));
}

void Memory_write_u32_le(Memory *const mem, const u32 addr, const u32 value)
{
    if ((addr % 4) != 0)
        BAIL("misaligned word write (0x%08X)", addr);

    Memory_write(mem, addr, (u8)value);
    Memory_write(mem, addr + 1, (u8)(value >> 8));
    Memory_write(mem, addr + 2, (u8)(value >> 16));
    Memory_write(mem, addr + 3, (u8)(value >> 24));
}

SegmentedMemory SegmentedMemory_new(void)
{
    u8 *const data = malloc(CPU_ADDRESS_SPACE);

    return (SegmentedMemory){
        .mem.read = SegmentedMemory_read,
        .mem.read_instr = SegmentedMemory_read_instr,
        .mem.write = SegmentedMemory_write,
        .data = data,
        .segments = nullptr,
        .segments_size = 0,
    };
}

void SegmentedMemory_add_segment(SegmentedMemory *const mem, const Segment seg)
{
    const size_t new_size = mem->segments_size + 1;
    Segment *const new_segments = realloc(mem->segments, new_size * sizeof(Segment));

    if (new_segments == nullptr)
        BAIL("Could not reallocate memory for segments");

    mem->segments = new_segments;

    mem->segments[mem->segments_size] = seg;
    mem->segments_size = new_size;

    ver_printf("added segment ==================\n");
    ver_printf("addr: %u\n", seg.addr);
    ver_printf("size: %u\n", seg.size);
    ver_printf("perms: %03B\n", seg.perms);
}

void SegmentedMemory_destroy(SegmentedMemory *const mem)
{
    free(mem->data);
    free(mem->segments);

    mem->data = nullptr;
    mem->segments = nullptr;
    mem->segments_size = 0;
}
