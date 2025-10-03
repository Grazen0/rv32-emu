# RV32 Emulator

A quick and dirty RISC-V 32 emulator written in C.

[CPUlator](https://cpulator.01xz.net/) is an awesome website we're using in my
computer architecture class to learn RV32I assembly, _but_ I wanted an offline
solution using the GCC RISC-V toolchain and whatnot. What I needed was an
**emulator**, but I wasn't able to find any easy-to-use emulator that mimmicked
CPUlator's insanely simple system specs (all RAM, even the program data).

> [!NOTE]
> As of right now, this emulator only supports raw binary files, not ELFs. That
> means that you must run something like `objcopy -O binary` on your ELF and use
> _that_ on this emulator.

## Features

- [x] RV32I integer instructions.
- [ ] RV64I extra integer instructions.
- [ ] RVF/D floating-point instructions.
- [ ] ELF file support.
- [x] GDB support.

## Building

If don't disable compiling tests via `-DBUILD_TESTING=Off`, you'll need to have
[Unity Test](https://github.com/ThrowTheSwitch/Unity) and [Ruby](https://www.ruby-lang.org/en/) installed on your machine.

You should be able to just clone the repo and run these:

```bash
cmake -S . -B build
cmake --build build
build/rv32-emu <path-to-binary>
```
