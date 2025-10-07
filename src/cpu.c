#include "cpu.h"
#include "macros.h"
#include "stdinc.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

[[nodiscard]] static u32 read_u32_le(const u8 *const memory, const u32 addr)
{
    if (addr > CPU_MEMORY_SIZE - 4)
        BAIL("memory index out of bounds: 0x%08X\n", addr);

    const u32 a = memory[addr];
    const u32 b = memory[addr + 1];
    const u32 c = memory[addr + 2];
    const u32 d = memory[addr + 3];

    return a | (b << 8) | (c << 16) | (d << 24);
}

[[nodiscard]] static u16 read_u16_le(const u8 *const memory, const u32 addr)
{
    if (addr > CPU_MEMORY_SIZE - 2)
        BAIL("memory index out of bounds: 0x%08X\n", addr);

    const u16 a = memory[addr];
    const u16 b = memory[addr + 1];

    return a | (b << 8);
}

static void write_u32_le(u8 *const memory, const u32 addr, const u32 value)
{
    if (addr > CPU_MEMORY_SIZE - 4)
        BAIL("memory index out of bounds: 0x%08X\n", addr);

    memory[addr] = (u8)value;
    memory[addr + 1] = (u8)(value >> 8);
    memory[addr + 2] = (u8)(value >> 16);
    memory[addr + 3] = (u8)(value >> 24);
}

static void write_u16_le(u8 *const memory, const u32 addr, const u16 value)
{
    if (addr > CPU_MEMORY_SIZE - 2)
        BAIL("memory index out of bounds: 0x%08X\n", addr);

    memory[addr] = (u8)value;
    memory[addr + 1] = (u8)(value >> 8);
}

Cpu Cpu_new(void)
{
    u8 *const memory = malloc(CPU_MEMORY_SIZE * sizeof(*memory));

    return (Cpu){
        .pc = 0x0,
        .registers = {},
        .memory = memory,
    };
}

void Cpu_destroy(Cpu *const cpu)
{
    free(cpu->memory);
}

// NOLINTNEXTLINE
void Cpu_step(Cpu *const cpu)
{
    const u32 instr = read_u32_le(cpu->memory, cpu->pc);
    u32 new_pc = cpu->pc + 4;

    const u8 op = instr & 0b111'1111;
    const u8 funct3 = (instr >> 12) & 0b111;
    const u8 funct7 = (instr >> 25) & 0b111'1111;
    const u8 rd = (instr >> 7) & 0b1'1111;
    const u8 rs1 = (instr >> 15) & 0b1'1111;
    const u8 rs2 = (instr >> 20) & 0b1'1111;

    const i32 imm_i = (i32)instr >> 20;
    const i32 imm_u = (i32)(instr & 0xFFFFF000);

    switch (op) {
    case 0b000'0011: {
        const u32 addr = cpu->registers[rs1] + imm_i;

        switch (funct3) {
        case 0b000:
            cpu->registers[rd] = (u32)(i32)(i8)cpu->memory[addr];
            break;
        case 0b001:
            cpu->registers[rd] = (u32)(i32)(i16)read_u16_le(cpu->memory, addr);
            break;
        case 0b010:
            cpu->registers[rd] = read_u32_le(cpu->memory, addr);
            break;
        case 0b100:
            cpu->registers[rd] = cpu->memory[addr];
            break;
        case 0b101:
            cpu->registers[rd] = read_u16_le(cpu->memory, addr);
            break;
        default:
            BAIL("illegal instruction: 0x%08X\n", instr);
        }
        break;
    }
    case 0b001'0011: {
        const u32 shamt = imm_i & 0x1F;

        switch (funct3) {
        case 0b000:
            cpu->registers[rd] = cpu->registers[rs1] + imm_i;
            break;
        case 0b001:
            cpu->registers[rd] = cpu->registers[rs1] << shamt;
            break;
        case 0b010:
            cpu->registers[rd] = (i32)cpu->registers[rs1] < imm_i;
            break;
        case 0b011:
            cpu->registers[rd] = cpu->registers[rs1] < (u32)imm_i;
            break;
        case 0b100:
            cpu->registers[rd] = cpu->registers[rs1] ^ imm_i;
            break;
        case 0b101:
            if (funct7 == 0b000'0000)
                cpu->registers[rd] = cpu->registers[rs1] >> shamt;
            else if (funct7 == 0b010'0000)
                cpu->registers[rd] = (u32)((i32)cpu->registers[rs1] >> shamt);
            else
                BAIL("illegal instruction: 0x%08X\n", instr);
            break;
        case 0b110:
            cpu->registers[rd] = cpu->registers[rs1] | imm_i;
            break;
        case 0b111:
            cpu->registers[rd] = cpu->registers[rs1] & imm_i;
            break;
        default:
            BAIL("illegal instruction: 0x%08X\n", instr);
        }
        break;
    }
    case 0b001'0111:
        cpu->registers[rd] = cpu->pc + imm_u;
        break;
    case 0b010'0011: {
        const i32 imm_s = (i32)((instr >> 7) & 0x1F) | (((i32)instr >> 25) << 5);
        const u32 addr = cpu->registers[rs1] + imm_s;

        switch (funct3) {
        case 0b000:
            cpu->memory[addr] = (u8)cpu->registers[rs2];
            break;
        case 0b001:
            write_u16_le(cpu->memory, addr, (u16)cpu->registers[rs2]);
            break;
        case 0b010:
            write_u32_le(cpu->memory, addr, cpu->registers[rs2]);
            break;
        default:
            BAIL("illegal instruction: 0x%08X\n", instr);
        }
        break;
    }
    case 0b011'0011: {
        const u32 shamt = (u32)(cpu->registers[rs2] & 0x1F);

        switch (funct3) {
        case 0b000:
            if (funct7 == 0b000'0000)
                cpu->registers[rd] = cpu->registers[rs1] + cpu->registers[rs2];
            else if (funct7 == 0b010'0000)
                cpu->registers[rd] = cpu->registers[rs1] - cpu->registers[rs2];
            else
                BAIL("illegal instruction: 0x%08X\n", instr);
            break;
        case 0b001:
            cpu->registers[rd] = cpu->registers[rs1] << shamt;
            break;
        case 0b010:
            cpu->registers[rd] = (i32)cpu->registers[rs1] < (i32)cpu->registers[rs2];
            break;
        case 0b011:
            cpu->registers[rd] = cpu->registers[rs1] < cpu->registers[rs2];
            break;
        case 0b100:
            cpu->registers[rd] = cpu->registers[rs1] ^ cpu->registers[rs2];
            break;
        case 0b101:
            if (funct7 == 0b000'0000)
                cpu->registers[rd] = cpu->registers[rs1] >> shamt;
            else if (funct7 == 0b010'0000)
                cpu->registers[rd] = (u32)((i32)cpu->registers[rs1] >> shamt);
            else
                BAIL("illegal instruction: 0x%08X\n", instr);
            break;
        case 0b110:
            cpu->registers[rd] = cpu->registers[rs1] | cpu->registers[rs2];
            break;
        case 0b111:
            cpu->registers[rd] = cpu->registers[rs1] & cpu->registers[rs2];
            break;
        default:
            BAIL("illegal instruction: 0x%08X\n", instr);
        }
        break;
    }
    case 0b011'0111:
        cpu->registers[rd] = imm_u;
        break;
    case 0b110'0011: {
        const i32 imm_b = (i32)((((instr >> 8) & 0xF) << 1) | (((instr >> 25) & 0x3F) << 5) |
                                (((instr >> 7) & 0x1) << 11) | (((i32)instr >> 31) << 12));

        switch (funct3) {
        case 0b000:
            if (cpu->registers[rs1] == cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b001:
            if (cpu->registers[rs1] != cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b100:
            if ((i32)cpu->registers[rs1] < (i32)cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b101:
            if ((i32)cpu->registers[rs1] >= (i32)cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b110:
            if (cpu->registers[rs1] < cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b111:
            if (cpu->registers[rs1] >= cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        default:
            BAIL("illegal instruction: 0x%08X\n", instr);
        }
        break;
    }
    case 0b110'0111:
        cpu->registers[rd] = new_pc;

        if (funct3 == 0b000)
            new_pc = (cpu->registers[rs1] + imm_i) & ~1;
        else
            BAIL("illegal instruction: 0x%08X\n", instr);

        break;
    case 0b110'1111: {

        const i32 imm_j = (i32)((((instr >> 21) & 0x3FF) << 1) | (((instr >> 20) & 0x1) << 11) |
                                (((instr >> 12) & 0xFF) << 12) | (((i32)instr >> 31) << 20));

        cpu->registers[rd] = new_pc;
        new_pc = cpu->pc + imm_j;
        break;
    }
    default:
        BAIL("illegal instruction: 0x%08X\n", instr);
    }

    cpu->pc = new_pc;
    cpu->registers[0] = 0;
}
