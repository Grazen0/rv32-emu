# RV32 Emulator

A quick and dirty RISC-V 32 emulator written in C.

![Screenshot](https://github.com/Grazen0/rv32-emu/blob/main/.github/screenshot.png?raw=true)

[CPUlator](https://cpulator.01xz.net/) is an awesome website we're using in my
computer architecture class to learn RV32I assembly, _but_ I wanted an offline
solution using the GCC RISC-V toolchain and whatnot. What I needed was an
**emulator**, but I wasn't able to find any easy-to-use emulator that mimmicked
CPUlator's insanely simple system specs (all RAM, even the program data).

You can check out an example project using this emulator
[here](https://github.com/Grazen0/hello-rv32).

## Features

- [x] RV32I integer instructions.
- [x] ~~RV64I extra integer instructions.~~ I'm probably sticking to 32 bits.
- [ ] RVF/D floating-point instructions.
- [x] Breakpoint support.
- [x] ELF file support.
- [x] GDB support.

## Building

If don't disable compiling tests via `-DBUILD_TESTING=Off`, you'll need to have
[Ruby] installed on your machine.

You should be able to just clone the repo and run these:

```bash
cmake -S . -B build
cmake --build build
build/rv32-emu <path-to-executable>
```

## Usage

```bash
# Compile your code with the RISC-V GNU toolchain
riscv32-unknown-none-elf-gcc -nostdlib -march=rv32i -mabi=ilp32 -g -o ./hello.elf ./hello.s

# Run the emulator
rv32-emu ./hello.elf

# Now, in *another* terminal...
riscv32-unknown-none-elf-gdb ./hello.elf
(gdb) target remote :3333
```

[pkg-config]: https://www.freedesktop.org/wiki/Software/pkg-config/
[unity test]: https://github.com/ThrowTheSwitch/Unity
[ruby]: https://www.ruby-lang.org/en/
