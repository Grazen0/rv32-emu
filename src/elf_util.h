#ifndef RV32_EMU_ELF_UTIL_H
#define RV32_EMU_ELF_UTIL_H

#include "memory.h"
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
    ElfResult_UnalignedVAddr,
    ElfResult_ProgramDataFileOutOfBounds,
    ElfResult_ProgramDataVAddrOutOfBounds,
    ElfResult_InvalidMemSize,
} ElfResult;

/**
 * \brief Returns a textual message for an ElfResult.
 *
 * \param result The ElfResult to return as a message.
 *
 * \return Textual message for result.
 */
[[nodiscard]] const char *ElfResult_display(ElfResult result);

/**
 * \brief Parses and validates an ELF file from binary data.
 *
 * Upon returning ElfResult_Ok, the following conditions for the ELF binary are guaranteed:
 *
 * 1. It is a well-formed ELF file.
 * 2. It is a 32-bit little-endian RISC-V executable.
 * 3. It has a valid ELF version.
 *
 * This function will not return an error on unsupported EI_OSABI, EI_ABIVERSION, or flag values,
 * but will warn about them to stdout.
 *
 * This function will not validate program headers, but at least guarantees that out_phdrs points to
 * enough space to fit e_phnum phdrs.
 *
 * \param elf_data ELF binary data to parse.
 * \param elf_data_size Size of elf_data
 * \param out_ehdr The resulting ELF header.
 * \param out_phdrs The resulting program header list.
 *
 * \return The parse and validation result.
 */
[[nodiscard]] ElfResult parse_elf(const u8 *elf_data, size_t elf_data_size,
                                  const Elf32_Ehdr **out_ehdr, const Elf32_Phdr **out_phdrs);

/**
 * \brief Loads an ELF header and constructs its Segment data.
 *
 * \param phdr The program header whose data is to be loaded.
 * \param phdr_n Index of phdr in the phdrs list.
 * \param elf_data Full binary data of the phdr's ELF file.
 * \param elf_data_size Size of elf_data
 * \param dest Address space where to place the segment's data.
 * \param out_seg The constructed Segment.
 *
 * \return The result of the operation.
 *
 * \sa Segment, SegmentedMemory
 */
[[nodiscard]] ElfResult Segment_from_phdr(const Elf32_Phdr *phdr, size_t phdr_n, const u8 *elf_data,
                                          size_t elf_data_size, u8 *dest, Segment *out_seg);

#endif
