# Minimal Example

This example contains a simple program that increments the `s1` register
in a loop.

You can compile and run this program with these commands:

```
riscv32-none-elf-gcc -nostdlib -march=rv32i -mabi=ilp32 -g ./main.s
rv32-emu ./a.out
```

Then, connect via GDB like this:

```
riscv32-none-elf-gdb ./a.out
(gdb) target remote :3333
```

You can step each instruction with `si` and inspect the value of `s1` with
`display $s1` or `layout regs`.
