#ifndef RV32_EMU_CPU_H
#define RV32_EMU_CPU_H

#include "memory.h"
#include "stdinc.h"
#include <stddef.h>
#include <stdio.h>

static constexpr size_t CPU_ADDRESS_SPACE = 0x1'0000'0000;
static constexpr size_t CPU_REGS_SIZE = 32;

typedef struct Cpu {
    u32 pc;
    u32 registers[CPU_REGS_SIZE];
} Cpu;

typedef enum CpuStepResult : u8 {
    CpuStepResult_None,
    CpuStepResult_Break,
    CpuStepResult_IllegalInstruction,
    CpuStepResult_Exit,
} CpuStepResult;

typedef enum Syscall : u32 {
    Syscall_PrintInteger = 1,
    Syscall_PrintString = 4,
    Syscall_ReadInteger = 5,
    Syscall_ReadString = 8,
    Syscall_Sbrk = 9,
    Syscall_Exit = 10,
    Syscall_PrintChar = 11,
    Syscall_ReadChar = 12,
    Syscall_Exit2 = 17,
    Syscall_Time = 30,
    Syscall_Sleep = 32,
    Syscall_PrintHex = 34,
    Syscall_PrintBinary = 35,
    Syscall_PrintUnsigned = 36,
} Syscall;

[[nodiscard]] Cpu Cpu_new(void);

[[nodiscard]] CpuStepResult Cpu_step(Cpu *cpu, Memory *mem);

#endif
