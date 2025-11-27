; DebugClient.asm - Minimal client with debug output
.const MOVE_REQUEST     0x4100
.const CLIENT_YIELD     0x4200
.const DEBUG_PRINT      0x5000

!main
    set rA, 0 ; Initialize direction register
    
!loop
    ; Simple pseudo-random movement
    add rA, 1
    mod rA, 8
    
    ; Write the direction to the debug address
    str [DEBUG_PRINT], rA
    
    ; Request the move
    str [MOVE_REQUEST], rA
    
    ; Yield CPU back to server
    set rB, 1
    str [CLIENT_YIELD], rB
    
    jmp !loop
