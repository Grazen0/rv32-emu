#ifndef RV32_EMU_ELF_HELPERS
#define RV32_EMU_ELF_HELPERS

#include "stdinc.h"
#include <elf.h>
#include <stddef.h>

typedef enum ElfResult {
    ElfResult_Ok = 0,
    ElfResult_FileTooSmall,
    ElfResult_InvalidMagicNumber,
    ElfResult_UnsupportedBits,
    ElfResult_UnsupportedEndianness,
    ElfResult_InvalidElfVersion,
    ElfResult_UnsupportedElfType,
    ElfResult_UnsupportedMachineType,
    ElfResult_InvalidElfHeaderSize,
    ElfResult_InvalidProgramHeaderSize,
    ElfResult_InvalidSectionHeaderSize,
} ElfResult;

[[nodiscard]] const char *ElfResult_display(ElfResult result);

[[nodiscard]] ElfResult parse_elf(const u8 *elf_data, size_t elf_data_size,
                                  const Elf32_Ehdr **out_ehdr, const Elf32_Phdr **out_phdrs);

#endif
