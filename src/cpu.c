#include "cpu.h"
#include "macros.h"
#include "memory.h"
#include "stdinc.h"
#include "unistd.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

Cpu Cpu_new(void)
{
    return (Cpu){
        .pc = 0x0,
        .registers = {},
    };
}

// NOLINTNEXTLINE
CpuStepResult Cpu_step(Cpu *const cpu, Memory *const mem)
{
    const u32 instr = Memory_read_instr(mem, cpu->pc);
    u32 new_pc = cpu->pc + 4;

    const u8 op = instr & 0b111'1111;
    const u8 funct3 = (instr >> 12) & 0b111;
    const u8 funct7 = (instr >> 25) & 0b111'1111;
    const u8 rd = (instr >> 7) & 0b1'1111;
    const u8 rs1 = (instr >> 15) & 0b1'1111;
    const u8 rs2 = (instr >> 20) & 0b1'1111;

    const i32 imm_i = (i32)instr >> 20;
    const i32 imm_u = (i32)(instr & 0xFFFFF000);

    cpu->registers[0] = 0;

    switch (op) {
    case 0b000'0011: {
        const u32 addr = cpu->registers[rs1] + imm_i;

        switch (funct3) {
        case 0b000: // lb    rd,  imm(rs1)
            cpu->registers[rd] = (i32)(i8)Memory_read(mem, addr);
            break;
        case 0b001: // lh    rd,  imm(rs1)
            cpu->registers[rd] = (i32)(i16)Memory_read_u16_le(mem, addr);
            break;
        case 0b010: // lw    rd,  imm(rs1)
            cpu->registers[rd] = Memory_read_u32_le(mem, addr);
            break;
        case 0b100: // lbu    rd,  imm(rs1)
            cpu->registers[rd] = Memory_read(mem, addr);
            break;
        case 0b101: // lhu    rd,  imm(rs1)
            cpu->registers[rd] = Memory_read_u16_le(mem, addr);
            break;
        default:
            return CpuStepResult_IllegalInstruction;
        }
        break;
    }
    case 0b001'0011: {
        const u32 shamt = imm_i & 0x1F;

        switch (funct3) {
        case 0b000: // addi    rd, rs1, imm
            cpu->registers[rd] = cpu->registers[rs1] + imm_i;
            break;
        case 0b001: // slli    rd, rs1, uimm
            cpu->registers[rd] = cpu->registers[rs1] << shamt;
            break;
        case 0b010: // slti    rd, rs1, imm
            cpu->registers[rd] = (i32)cpu->registers[rs1] < imm_i;
            break;
        case 0b011: // sltiu    rd, rs1, imm
            cpu->registers[rd] = cpu->registers[rs1] < (u32)imm_i;
            break;
        case 0b100: // xori    rd, rs1, imm
            cpu->registers[rd] = cpu->registers[rs1] ^ imm_i;
            break;
        case 0b101:
            if (funct7 == 0b000'0000) // srli    rd, rs1, uimm
                cpu->registers[rd] = cpu->registers[rs1] >> shamt;
            else if (funct7 == 0b010'0000) // srai    rd, rs1, uimm
                cpu->registers[rd] = (i32)cpu->registers[rs1] >> shamt;
            else
                return CpuStepResult_IllegalInstruction;
            break;
        case 0b110: // ori    rd, rs1, imm
            cpu->registers[rd] = cpu->registers[rs1] | imm_i;
            break;
        case 0b111: // andi    rd, rs1, imm
            cpu->registers[rd] = cpu->registers[rs1] & imm_i;
            break;
        default:
            return CpuStepResult_IllegalInstruction;
        }
        break;
    }
    case 0b001'0111: // auipc    rd, upimm
        cpu->registers[rd] = cpu->pc + imm_u;
        break;
    case 0b010'0011: {
        const i32 imm_s = (i32)((instr >> 7) & 0x1F) | (((i32)instr >> 25) << 5);
        const u32 addr = cpu->registers[rs1] + imm_s;

        switch (funct3) {
        case 0b000: // sb    rs2, imm(rs1)
            Memory_write(mem, addr, cpu->registers[rs2] & 0xFF);
            break;
        case 0b001: // sh    rs2, imm(rs1)
            Memory_write_u16_le(mem, addr, cpu->registers[rs2] & 0xFFFF);
            break;
        case 0b010: // sw    rs2, imm(rs1)
            Memory_write_u32_le(mem, addr, cpu->registers[rs2]);
            break;
        default:
            return CpuStepResult_IllegalInstruction;
        }
        break;
    }
    case 0b011'0011: {
        const u32 shamt = (u32)(cpu->registers[rs2] & 0x1F);

        switch (funct3) {
        case 0b000:
            if (funct7 == 0b000'0000) // add    rd, rs1, rs2
                cpu->registers[rd] = cpu->registers[rs1] + cpu->registers[rs2];
            else if (funct7 == 0b010'0000) // sub    rd, rs1, rs2
                cpu->registers[rd] = cpu->registers[rs1] - cpu->registers[rs2];
            else
                return CpuStepResult_IllegalInstruction;
            break;
        case 0b001: // sll    rd, rs1, rs2
            cpu->registers[rd] = cpu->registers[rs1] << shamt;
            break;
        case 0b010: // slt    rd, rs1, rs2
            cpu->registers[rd] = (i32)cpu->registers[rs1] < (i32)cpu->registers[rs2];
            break;
        case 0b011: // sltu    rd, rs1, rs2
            cpu->registers[rd] = cpu->registers[rs1] < cpu->registers[rs2];
            break;
        case 0b100: // xor    rd, rs1, rs2
            cpu->registers[rd] = cpu->registers[rs1] ^ cpu->registers[rs2];
            break;
        case 0b101:
            if (funct7 == 0b000'0000) // srl    rd, rs1, rs2
                cpu->registers[rd] = cpu->registers[rs1] >> shamt;
            else if (funct7 == 0b010'0000) // sra    rd, rs1, rs2
                cpu->registers[rd] = (u32)((i32)cpu->registers[rs1] >> shamt);
            else
                return CpuStepResult_IllegalInstruction;
            break;
        case 0b110: // or    rd, rs1, rs2
            cpu->registers[rd] = cpu->registers[rs1] | cpu->registers[rs2];
            break;
        case 0b111: // and    rd, rs1, rs2
            cpu->registers[rd] = cpu->registers[rs1] & cpu->registers[rs2];
            break;
        default:
            return CpuStepResult_IllegalInstruction;
        }
        break;
    }
    case 0b011'0111: // lui    rd, rs1, rs2
        cpu->registers[rd] = imm_u;
        break;
    case 0b110'0011: {
        const i32 imm_b = (i32)((((instr >> 8) & 0xF) << 1) | (((instr >> 25) & 0x3F) << 5) |
                                (((instr >> 7) & 0x1) << 11) | (((i32)instr >> 31) << 12));

        switch (funct3) {
        case 0b000: // beq    rs1, rs2, label
            if (cpu->registers[rs1] == cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b001: // bne    rs1, rs2, label
            if (cpu->registers[rs1] != cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b100: // blt    rs1, rs2, label
            if ((i32)cpu->registers[rs1] < (i32)cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b101: // bge    rs1, rs2, label
            if ((i32)cpu->registers[rs1] >= (i32)cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b110: // bltu    rs1, rs2, label
            if (cpu->registers[rs1] < cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        case 0b111: // bgeu    rs1, rs2, label
            if (cpu->registers[rs1] >= cpu->registers[rs2])
                new_pc = cpu->pc + imm_b;
            break;
        default:
            return CpuStepResult_IllegalInstruction;
        }
        break;
    }
    case 0b110'0111:
        cpu->registers[rd] = new_pc;

        if (funct3 == 0b000) // jalr    rd, rs1, imm
            new_pc = (cpu->registers[rs1] + imm_i) & ~1;
        else
            return CpuStepResult_IllegalInstruction;

        break;
    case 0b110'1111: { // jal    rd, label
        const i32 imm_j = (i32)((((instr >> 21) & 0x3FF) << 1) | (((instr >> 20) & 0x1) << 11) |
                                (((instr >> 12) & 0xFF) << 12) | (((i32)instr >> 31) << 20));

        cpu->registers[rd] = new_pc;
        new_pc = cpu->pc + imm_j;
        break;
    }
    case 0b111'0011:
        if (funct3 != 0)
            return CpuStepResult_IllegalInstruction;

        if (imm_i == 0) { // ecall
            const u32 a7 = cpu->registers[17]; // a7
            const u32 a0 = cpu->registers[10]; // a0
            const u32 a1 = cpu->registers[11]; // a1

            switch (a7) {
            case Syscall_PrintInteger:
                printf("%i", (i32)a0);
                fflush(stdout);
                break;

            case Syscall_PrintString:
                u32 addr = a0;

                while (true) {
                    const char ch = (char)Memory_read(mem, addr);

                    if (ch == '\0')
                        break;

                    fputc(ch, stdout);
                    ++addr;
                }

                fflush(stdout);
                break;

            case Syscall_ReadInteger:
                int n = 0;

                if (scanf("%d", &n) == 1)
                    cpu->registers[10] = n;

                break;

            case Syscall_ReadString: {
                char *buf = malloc(a1);

                if (fgets(buf, (int)a1, stdin) != nullptr) {
                    const size_t len = strlen(buf);

                    if (len != 0 && buf[len - 1] == '\n')
                        buf[len - 1] = '\0';

                    for (size_t i = 0; buf[i] != '\0'; ++i)
                        Memory_write(mem, a0 + i, buf[i]);
                }

                free(buf);
                buf = nullptr;
                break;
            }

            case Syscall_Exit:
                return CpuStepResult_Exit;

            case Syscall_PrintChar:
                fputc((char)a0, stdout);
                fflush(stdout);
                break;

            case Syscall_ReadChar:
                char ch = '\0';

                if (scanf(" %c", &ch) == 1)
                    cpu->registers[10] = (u32)ch;

                break;

            case Syscall_Time:
                struct timeval time = {};
                gettimeofday(&time, nullptr);

                const u64 ms = (time.tv_sec * 1000ULL) + (time.tv_usec / 1000ULL);

                cpu->registers[10] = ms & 0xFFFF'FFFF;
                cpu->registers[11] = (ms >> 32) & 0xFFFF'FFFF;
                break;

            case Syscall_Sleep:
                usleep(1000ULL * a0);
                break;

            case Syscall_PrintHex:
                printf("%08X", a0);
                fflush(stdout);
                break;

            case Syscall_PrintBinary:
                printf("%032B", a0);
                fflush(stdout);
                break;

            case Syscall_PrintUnsigned:
                printf("%u", a0);
                fflush(stdout);
                break;

            default:
                BAIL("Illegal ecall number (%u)", a7);
            }
        } else if (imm_i == 1) { // ebreak
            return CpuStepResult_Break;
        } else {
            return CpuStepResult_IllegalInstruction;
        }
        break;
    default:
        return CpuStepResult_IllegalInstruction;
    }

    cpu->pc = new_pc;
    cpu->registers[0] = 0;

    return CpuStepResult_None;
}
