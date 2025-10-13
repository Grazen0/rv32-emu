# Example C Program

This example showcases some C code that writes the first 16 Fibonacci numbers to
words in memory starting at address `0x80000000`.

You can compile and run this program with this command:

```bash
make run
```

Then, connect via GDB like this:

```bash
make debug
(gdb) target remote :3333
```
