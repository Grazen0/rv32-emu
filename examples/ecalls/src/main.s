PRINT_INT = 1
PRINT_STR = 4
READ_INT = 5
EXIT = 10
PRINT_CHAR = 11

.text
.global _start

_start:
    la      gp, __global_pointer$
    li      sp, 0

    li      a7, PRINT_STR
    la      a0, str1
    ecall

    li      a7, READ_INT
    ecall

    mv      s0, a0

    slli    s2, s0, 2
    addi    s2, s2, 4
    sub     sp, sp, s2

    li      s1, 0
read_loop:
    beq     s1, s0, end_read_loop

    li      a7, PRINT_STR
    la      a0, str2
    ecall

    li      a7, PRINT_INT
    mv      a0, s1
    ecall

    li      a7, PRINT_STR
    la      a0, str3
    ecall

    li      a7, READ_INT
    ecall

    slli    s3, s1, 2
    add     s3, s3, sp
    sw      a0, 0(s3)

    addi    s1, s1, 1
    j       read_loop
end_read_loop:

    mv      a0, sp
    mv      a1, s0
    call    selection_sort

    li      a7, PRINT_STR
    la      a0, str4
    ecall

    li      t0, 0
print_loop:
    beq     t0, s0, end_print_loop

    slli    t1, t0, 2
    add     t1, t1, sp
    lw      a0, 0(t1)

    li      a7, PRINT_INT
    ecall

    li      a7, PRINT_CHAR
    li      a0, ' '
    ecall

    addi    t0, t0, 1
    j       print_loop
end_print_loop:

    li      a7, PRINT_CHAR
    li      a0, '\n'
    ecall

    add     sp, sp, s2

    la      a7, EXIT
    ecall

# a0: int *array
# a1: int size
selection_sort:
    li      t0, 2
    blt     a1, t0, end_outer

    # t1 = size - 1
    addi    t1, a1, -1

    li      t0, 0               # t0 <- i
loop_outer:
    beq     t0, t1, end_outer

    # t3 <- min_value
    # t3 = arr[i]
    slli    t3, t0, 2
    add     t3, t3, a0
    lw      t3, 0(t3)

    # t4 <- index of min_value
    # t4 = i
    mv      t4, t0

    # t2 <- j
    # t2 = i + 1
    addi    t2, t0, 1
loop_inner:
    beq     t2, a1, end_inner

    # t5 = arr[j]
    slli    t5, t2, 2
    add     t5, t5, a0
    lw      t5, 0(t5)

    bge     t5, t3, skip

    mv      t3, t5
    mv      t4, t2
skip:

    addi    t2, t2, 1
    j       loop_inner
end_inner:

    # t5 = arr[i]
    slli    t2, t0, 2
    add     t2, t2, a0
    lw      t5, 0(t2)

    # arr[i] = min_value
    sw      t3, 0(t2)

    slli    t4, t4, 2
    add     t4, t4, a0
    sw      t5, 0(t4)

    addi    t0, t0, 1
    j       loop_outer

end_outer:
    ret

.data

str1: .asciz "Length of the array: "
str2: .asciz "arr["
str3: .asciz "]: "
str4: .asciz "Sorted array: "
