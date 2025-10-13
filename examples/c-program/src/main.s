.global _start

_start:
    li      sp, 0xFFFFFFF0
    call    main

loop:
    ebreak
    j       loop
