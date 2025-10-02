#include "macros.h"
#include "stdinc.h"
#include <argparse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static const char *const usages[] = {
    "rv32-emu [options] [--] <filename>",
    nullptr,
};

static constexpr size_t MEMORY_SIZE = 0x1'0000'0000;

typedef struct Cpu {
    u32 pc;
    i32 registers[32];
    u8 *memory;
} Cpu;

Cpu Cpu_new(void)
{
    u8 *const memory = malloc(MEMORY_SIZE * sizeof(*memory));

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

u32 Cpu_read_u32(const u8 *const memory, const u32 addr)
{
    const u32 a = memory[addr];
    const u32 b = memory[addr + 1];
    const u32 c = memory[addr + 2];
    const u32 d = memory[addr + 3];

    return a | (b << 8) | (c << 16) | (d << 24);
}

u16 Cpu_read_u16(const u8 *const memory, const u32 addr)
{
    const u16 a = memory[addr];
    const u16 b = memory[addr + 1];

    return a | (b << 8);
}

void Cpu_write_u32(u8 *const memory, const u32 addr, const u32 value)
{
    memory[addr] = value;
    memory[addr + 1] = value >> 8;
    memory[addr + 2] = value >> 16;
    memory[addr + 3] = value >> 24;
}

void Cpu_write_u16(u8 *const memory, const u32 addr, const u16 value)
{
    memory[addr] = value;
    memory[addr + 1] = value >> 8;
}

static inline i32 sign_ext_u8(const u8 num)
{
    return (i8)num;
}

static inline i32 sign_ext_u16(const u16 num)
{
    return (i16)num;
}

void Cpu_step(Cpu *const cpu)
{
    const u32 instr = Cpu_read_u32(cpu->memory, cpu->pc);
    cpu->pc += 4;

    const u8 op = instr & 0b111'1111;
    const u8 funct3 = (instr >> 12) & 0b111;
    const u8 funct7 = (instr >> 25) & 0b111'1111;
    const u8 rd = (instr >> 7) & 0b1'1111;
    const u8 rs1 = (instr >> 15) & 0b1'1111;
    const u8 rs2 = (instr >> 20) & 0b1'1111;

    const i32 imm_i = (i32)instr >> 20;
    const i32 imm_s = (i32)((instr >> 7) & 0x1F) | (((i32)instr >> 25) << 5);
    const i32 imm_b =
        (i32)((((instr >> 8) & 0xF) << 1) | (((instr >> 25) & 0x3F) << 5) |
              (((instr >> 7) & 0x1) << 11) | (((i32)instr >> 31) << 12));
    const i32 imm_u = (i32)(instr & 0xFFFFF000);
    const i32 imm_j =
        (i32)((((instr >> 21) & 0x3FF) << 1) | (((instr >> 20) & 0x1) << 11) |
              (((instr >> 12) & 0xFF) << 12) | (((i32)instr >> 31) << 20));

    printf("instruction: %08X\n", instr);
    printf("op: %02X\n", op);

    switch (op) {
    case 0b000'0011: {
        const u32 addr = cpu->registers[rs1] + imm_i;

        switch (funct3) {
        case 0b000:
            cpu->registers[rd] = sign_ext_u8(cpu->memory[addr]);
            break;
        case 0b001:
            cpu->registers[rd] = sign_ext_u16(Cpu_read_u16(cpu->memory, addr));
            break;
        case 0b010:
            cpu->registers[rd] = (i32)Cpu_read_u32(cpu->memory, addr);
            break;
        case 0b100:
            cpu->registers[rd] = cpu->memory[addr];
            break;
        case 0b101:
            cpu->registers[rd] = Cpu_read_u16(cpu->memory, addr);
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
            cpu->registers[rd] = cpu->registers[rs1] < imm_i;
            break;
        case 0b011:
            cpu->registers[rd] = (u32)cpu->registers[rs1] < (u32)imm_i;
            break;
        case 0b100:
            cpu->registers[rd] = cpu->registers[rs1] ^ imm_i;
            break;
        case 0b101:
            if (funct7 == 0b000'0000) // srli rd, rs1, imm
                cpu->registers[rd] = (i32)((u32)cpu->registers[rs1] >> shamt);
            else if (funct7 == 0b010'0000) // srai rd, rs1, imm
                cpu->registers[rd] = cpu->registers[rs1] >> shamt;
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
    case 0b001'0111: // auipc rd, upimm
        cpu->registers[rd] = (i32)(cpu->pc + imm_u);
        break;
    case 0b010'0011: {
        const u32 addr = cpu->registers[rs1] + imm_s;

        switch (funct3) {
        case 0b000:
            cpu->memory[addr] = (u8)cpu->registers[rs2];
            break;
        case 0b001:
            Cpu_write_u16(cpu->memory, addr, (u16)cpu->registers[rs2]);
            break;
        case 0b010:
            Cpu_write_u32(cpu->memory, addr, (u32)cpu->registers[rs2]);
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
            cpu->registers[rd] = cpu->registers[rs1] < cpu->registers[rs2];
            break;
        case 0b011:
            cpu->registers[rd] =
                (u32)cpu->registers[rs1] < (u32)cpu->registers[rs2];
            break;
        case 0b100:
            cpu->registers[rd] = cpu->registers[rs1] ^ cpu->registers[rs2];
            break;
        case 0b101:
            if (funct7 == 0b000'0000)
                cpu->registers[rd] = (i32)((u32)cpu->registers[rs1] >> shamt);
            else if (funct7 == 0b010'0000)
                cpu->registers[rd] = cpu->registers[rs1] >> shamt;
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
    case 0b110'0011:
        switch (funct3) {
        case 0b000:
            if (cpu->registers[rs1] == cpu->registers[rs2])
                cpu->pc += imm_b;
            break;
        case 0b001:
            if (cpu->registers[rs1] != cpu->registers[rs2])
                cpu->pc += imm_b;
            break;
        case 0b100:
            if (cpu->registers[rs1] < cpu->registers[rs2])
                cpu->pc += imm_b;
            break;
        case 0b101:
            if (cpu->registers[rs1] >= cpu->registers[rs2])
                cpu->pc += imm_b;
            break;
        case 0b110:
            if ((u32)cpu->registers[rs1] < (u32)cpu->registers[rs2])
                cpu->pc += imm_b;
            break;
        case 0b111:
            if ((u32)cpu->registers[rs1] >= (u32)cpu->registers[rs2])
                cpu->pc += imm_b;
            break;
        default:
            BAIL("illegal instruction: 0x%08X\n", instr);
        }
        break;
    case 0b110'0111:
        cpu->registers[rd] = (i32)cpu->pc;

        if (funct3 == 0b000)
            cpu->pc = (cpu->registers[rs1] + imm_i) & ~1;
        else
            BAIL("illegal instruction: 0x%08X\n", instr);

        break;
    case 0b110'1111:
        cpu->registers[rd] = (i32)cpu->pc;
        cpu->pc += imm_j;
        break;
    default:
        BAIL("illegal instruction: 0x%08X\n", instr);
    }

    cpu->registers[0] = 0;
}

int main(int argc, const char *argv[])
{
    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(
        &argparse, "\nA quick and dirty RISC-V 32 CPU emulator written in C.",
        nullptr);

    argc = argparse_parse(&argparse, argc, argv);

    if (argc < 1) {
        argparse_usage(&argparse);
        return EXIT_FAILURE;
    }

    const char *const filename = argv[0];
    printf("Opening %s\n", filename);

    FILE *const file = fopen(filename, "r");

    if (file == nullptr) {
        perror("Could not open file");
        return EXIT_FAILURE;
    }

    fseek(file, 0L, SEEK_END);
    const size_t file_size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    if (file_size > MEMORY_SIZE) {
        fprintf(
            stderr,
            "Program is too large (must be up to %zu bytes, was %zu bytes)\n",
            MEMORY_SIZE, file_size);
        return EXIT_FAILURE;
    }

    Cpu cpu = Cpu_new();
    fread(cpu.memory, sizeof(cpu.memory[0]), file_size, file);

    fclose(file);

    for (int i = 0; i < 100; ++i)
        Cpu_step(&cpu);

    Cpu_destroy(&cpu);
    return EXIT_SUCCESS;
}
