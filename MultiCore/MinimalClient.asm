; MinimalClient.asm
; Week 1 Milestone version. Moves randomly.

.const  CLIENT_X          0x9002
.const  MOVE_REQUEST      0x9100
.const  CLIENT_YIELD      0x9200

!main
!loop
    ; --- WANDER LOGIC ---
    ; Generate a pseudo-random direction from 0 to 7.
    ; We use the client's X coordinate as a cheap source of randomness.
    lod rA, [CLIENT_X]
    add rA, 1           ; Increment to change the value each turn
    mod rA, 8           ; Modulo 8 to get a value in the range 0-7

    ; --- MOVE & YIELD ---
    str [MOVE_REQUEST], rA
    
    set rB, 1
    str [CLIENT_YIELD], rB
    jmp !loop
