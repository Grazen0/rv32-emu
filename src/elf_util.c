#include "elf_util.h"
#include "cpu.h"
#include "log.h"
#include "macros.h"
#include "memory.h"
#include "numeric.h"
#include <elf.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
    case ElfResult_UnalignedVAddr:
        return "p_vaddr of not aligned properly";
    case ElfResult_ProgramDataFileOutOfBounds:
        return "File data exceeds ELF file size.";
    case ElfResult_ProgramDataVAddrOutOfBounds:
        return "Virtual address range exceeds target memory bounds.";
    case ElfResult_InvalidMemSize:
        return "p_memsz is smaller than p_filesz";
    default:
        BAIL("Invalid ElfResult.");
    }
}

ElfResult parse_elf(const u8 *const elf_data, const size_t elf_data_size,
                    const Elf32_Ehdr **const out_ehdr, const Elf32_Phdr **const out_phdrs)
{
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

    if (ehdr->e_ident[EI_OSABI] != ELFOSABI_NONE)
        fprintf(stderr, "Warning: Unsupported ELF OSABI (0x%02X).\n", ehdr->e_ident[EI_OSABI]);

    if (ehdr->e_ident[EI_ABIVERSION] != 0x00)
        fprintf(stderr, "Warning: Unsupported ELF ABIVERSION (0x%02X).\n",
                ehdr->e_ident[EI_ABIVERSION]);

    if (ehdr->e_type != ET_EXEC)
        return ElfResult_UnsupportedElfType;

    if (ehdr->e_machine != EM_RISCV)
        return ElfResult_UnsupportedMachineType;

    if (ehdr->e_version != EV_CURRENT)
        return ElfResult_InvalidElfVersion;

    if (ehdr->e_flags != 0)
        fprintf(stderr, "Warning: Ignoring non-zero flags in ELF header.\n");

    if (elf_data_size < ehdr->e_phoff + (ehdr->e_phnum * ehdr->e_phentsize))
        return ElfResult_FileTooSmall;

    Elf32_Phdr *const phdrs = (Elf32_Phdr *)(&elf_data[ehdr->e_phoff]);

    *out_ehdr = ehdr;
    *out_phdrs = phdrs;

    return ElfResult_Ok;
}

ElfResult Segment_from_phdr(const Elf32_Phdr *const phdr, const size_t phdr_n,
                            const u8 *const elf_data, const size_t elf_data_size, u8 *const dest,
                            Segment *const out_seg)
{
    ver_printf("Loading phdr[%zu] into memory.\n", phdr_n);

    if (phdr->p_align > 1) {
        if (!u32_is_pow2(phdr->p_align))
            fprintf(stderr, "Warning(phdrs[%zu]): p_align of is not a power of 2.\n", phdr_n);

        if ((phdr->p_vaddr % phdr->p_align) != (phdr->p_offset % phdr->p_align))
            return ElfResult_UnalignedVAddr;
    }

    if (phdr->p_paddr != phdr->p_vaddr) {
        fprintf(stderr, "Warning (phdrs[%zu]): Ignoring p_addr value of %X different to p_vaddr.\n",
                phdr_n, phdr->p_paddr);
    }

    if (phdr->p_memsz < phdr->p_filesz)
        return ElfResult_InvalidMemSize;

    if (phdr->p_offset + phdr->p_filesz > elf_data_size)
        return ElfResult_ProgramDataFileOutOfBounds;

    if ((size_t)phdr->p_vaddr + phdr->p_memsz > CPU_ADDRESS_SPACE)
        return ElfResult_ProgramDataVAddrOutOfBounds;

    SegPerms perms = SegPerms_None;

    if ((phdr->p_flags & PF_R) != 0)
        perms |= SegPerms_Read;

    if ((phdr->p_flags & PF_W) != 0)
        perms |= SegPerms_Write;

    if ((phdr->p_flags & PF_X) != 0)
        perms |= SegPerms_Read | SegPerms_Execute;

    *out_seg = (Segment){
        .addr = phdr->p_vaddr,
        .size = phdr->p_memsz,
        .perms = perms,
    };

    memcpy(&dest[phdr->p_vaddr], &elf_data[phdr->p_offset], phdr->p_filesz);

    // Zero-out BSS
    if (phdr->p_memsz > phdr->p_filesz)
        memset(&dest[phdr->p_vaddr + phdr->p_filesz], 0, phdr->p_memsz - phdr->p_filesz);

    return ElfResult_Ok;
}
