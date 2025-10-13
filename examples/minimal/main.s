.global _start

_start:
    li      s1, 0

loop:
    addi    s1, s1, 1
    j       loop
