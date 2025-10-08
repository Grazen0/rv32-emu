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
 * \brief Loads a program section into dest.
 *
 * Will load the data pointed to by the program header at phdr into the address space starting at
 * dest.
 *
 * \param dest The address space to load data into.
 * \param dest_size Size of dest.
 * \param phdr The program header whose data is to be loaded.
 * \param phdr_n Index of phdr in the phdrs list.
 * \param elf_data Full binary data of the phdr's ELF file.
 * \param elf_data_size Size of elf_data
 *
 * \return The result of the operation.
 */
[[nodiscard]] ElfResult load_phdr(u8 *dest, size_t dest_size, const Elf32_Phdr *phdr, size_t phdr_n,
                                  const u8 *elf_data, size_t elf_data_size);

/**
 * \brief Verbose-prints debug information about an ELF program header.
 *
 * \param phdr The header to print.
 * \param phdr_n Index of phdr in the list of program headers.
 * \param elf_data The full ELF binary data.
 * \param elf_data_size Size of elf_data.
 *
 * \sa set_verbose, ver_printf
 */
void print_phdr_debug(const Elf32_Phdr *phdr, size_t phdr_n);

#endif
