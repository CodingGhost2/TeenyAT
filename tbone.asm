.const PORT_A 0x8002

set rA, rZ

!main
    str [PORT_A], rA
    inv rA
    jmp !main