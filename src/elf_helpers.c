#include "elf_helpers.h"
#include "util.h"
#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

const char *ElfResult_display(const ElfResult result)
{
    switch (result) {
    case ElfResult_Ok:
        return "Ok.";
    case ElfResult_FileTooSmall:
    case ElfResult_InvalidMagicNumber:
        return "Not an ELF binary.";
    case ElfResult_UnsupportedBits:
        return "Sorry, this emulator only supports 32-bit executables.";
    case ElfResult_UnsupportedEndianness:
        return "Sorry, this emulator only supports little-endian.";
    case ElfResult_InvalidElfVersion:
        return "Invalid ELF version.";
    case ElfResult_UnsupportedElfType:
        return "Unsupported ELF type. (Must be ET_EXEC).";
    case ElfResult_UnsupportedMachineType:
        return "Sorry, this emulator only supports RISC-V.";
    case ElfResult_InvalidElfHeaderSize:
        return "Invalid ELF header size.";
    case ElfResult_InvalidProgramHeaderSize:
        return "Invalid program header size.";
    case ElfResult_InvalidSectionHeaderSize:
        return "Invalid section header size.";
    default:
        return "Invalid ElfResult.";
    }
}

ElfResult parse_elf(const u8 *const elf_data, const size_t elf_data_size,
                    const Elf32_Ehdr **const out_ehdr, const Elf32_Phdr **const out_phdrs)
{
    if (elf_data_size < sizeof(Elf32_Ehdr)) {
        printf("Invalid ELF file.\n");
        return 1;
    }

    Elf32_Ehdr *const ehdr = (Elf32_Ehdr *)elf_data;

    if (elf_data_size < sizeof(Elf32_Ehdr))
        return ElfResult_FileTooSmall;

    if (memcmp(&ehdr->e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0)
        return ElfResult_InvalidMagicNumber;

    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32)
        return ElfResult_UnsupportedBits;

    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB)
        return ElfResult_UnsupportedEndianness;

    if (ehdr->e_ehsize != sizeof(Elf32_Ehdr))
        return ElfResult_InvalidElfHeaderSize;

    if (ehdr->e_phentsize != sizeof(Elf32_Phdr))
        return ElfResult_InvalidProgramHeaderSize;

    if (ehdr->e_shentsize != sizeof(Elf32_Shdr))
        return ElfResult_InvalidSectionHeaderSize;

    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT)
        return ElfResult_InvalidElfVersion;

    if (ehdr->e_ident[EI_OSABI] != ELFOSABI_NONE) {
        printf("Warning: Unsupported ELF OSABI (0x%02X), continuing anyway.\n",
               ehdr->e_ident[EI_OSABI]);
    }

    if (ehdr->e_ident[EI_ABIVERSION] != 0x00) {
        printf("Warning: Unsupported ELF ABIVERSION (0x%02X), continuing anyway.\n",
               ehdr->e_ident[EI_ABIVERSION]);
    }

    if (ehdr->e_type != ET_EXEC)
        return ElfResult_UnsupportedElfType;

    if (ehdr->e_machine != EM_RISCV)
        return ElfResult_UnsupportedMachineType;

    if (ehdr->e_version != EV_CURRENT)
        return ElfResult_InvalidElfVersion;

    if (ehdr->e_flags != 0)
        printf("Warning: Ignoring non-zero flags in ELF header.\n");

    ver_printf("\n");

    if (elf_data_size < ehdr->e_phoff + (ehdr->e_phnum * ehdr->e_phentsize))
        return ElfResult_FileTooSmall;

    Elf32_Phdr *const phdrs = (Elf32_Phdr *)(&elf_data[ehdr->e_phoff]);

    *out_ehdr = ehdr;
    *out_phdrs = phdrs;

    return ElfResult_Ok;
}
