#ifndef RV32_EMU_CPU_H
#define RV32_EMU_CPU_H

#include "stdinc.h"
#include <stddef.h>
#include <stdio.h>

static constexpr size_t CPU_MEMORY_SIZE = 0x1'0000'0000;
static constexpr size_t CPU_REGS_SIZE = 32;

typedef struct Cpu {
    u32 pc;
    u32 registers[CPU_REGS_SIZE];
    u8 *memory;
} Cpu;

typedef enum CpuStepResult {
    CpuStepResult_None,
    CpuStepResult_Break,
    CpuStepResult_IllegalInstruction,
} CpuStepResult;

Cpu Cpu_new(void);

void Cpu_destroy(Cpu *cpu);

CpuStepResult Cpu_step(Cpu *cpu);

#endif
