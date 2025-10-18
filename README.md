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
- [ ] F extension.
- [x] Breakpoint support.
- [x] ELF file support.
- [x] GDB support.
- [x] SPIM system calls.
- [ ] Memory access checks.

## Building

If don't disable compiling tests via `-DBUILD_TESTING=Off`, you'll need to have
[Ruby](https://www.ruby-lang.org/) installed on your machine.

You should be able to just clone the repo and run these:

```bash
cmake -S . -B build
cmake --build build
build/rv32-emu <path-to-executable>
```

## Usage

A few usage examples are provided in the [examples] directory.
