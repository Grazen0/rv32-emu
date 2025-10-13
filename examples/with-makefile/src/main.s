.text
.global _start

# This program contains a function that converts a string into uppercase.

_start:
    li      sp, 0xFFFFFFF0

    la      a0, str
    call    string_to_uppercase

loop:
    ebreak
    j       loop

# a0 <- The string to convert to uppercase.
string_to_uppercase:
1:
    lb      t0, 0(a0)
    beq     t0, zero, 3f

    li      t1, 'a'
    blt     t0, t1, 2f
    li      t1, 'z'
    bgt     t0, t1, 2f

    addi    t0, t0, 'A' - 'a'
    sb      t0, 0(a0)

2:
    addi    a0, a0, 1
    j       1b

3:
    ret

.data
str: .asciz "Hello, world!"
