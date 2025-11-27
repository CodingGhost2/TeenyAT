; MinimalClient.asm - Reads ID and moves randomly
.const CLIENT_ID        0x4000
.const CLIENT_TEAM      0x4001
.const CLIENT_X         0x4002
.const CLIENT_Y         0x4003
.const MOVE_REQUEST     0x4100
.const CLIENT_YIELD     0x4200

; Using a register to hold a value that will be changed
; to simulate randomness for movement direction.

!main
    set rA, 0 ; Initialize direction register
    
!loop
    ; Simple pseudo-random movement
    add rA, 1
    mod rA, 8
    str [MOVE_REQUEST], rA
    
    ; Yield CPU back to server
    set rB, 1
    str [CLIENT_YIELD], rB
    
    jmp !loop