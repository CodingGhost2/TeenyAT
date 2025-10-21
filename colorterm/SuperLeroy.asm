; TeenyAT Assembly Code for Platformer Control
; Reads keyboard input and writes to movement/animation addresses.

; --- I/O ADDRESSES ---
.const KEY_INPUT       0x4000  ; Read keyboard state (bitmask)
                                ; Bit 0 (1) = Left, Bit 1 (2) = Right, Bit 2 (4) = Jump

.const MOVE_W          0x9014  ; Move West (Left)
.const MOVE_E          0x9010  ; Move East (Right)
.const MOVE_N          0x9016  ; Move North (Jump)

.const Animation_Stand 0x9025  ; Animation state for standing
.const Animation_MoveL 0x9026  ; Animation state for moving left
.const Animation_MoveR 0x9027  ; Animation state for moving right
.const Animation_UP    0x9028  ; Animation state for jumping


; --- MAIN PROGRAM LOOP ---
!main
    set rC, 0               ; Initialize rC to 0 (Used as the data value for STR commands)

    ; Load the keyboard state from the C++ engine into register rA
    lod rA, [KEY_INPUT]

    ; --- Check for Left/Right Movement ---
    ; Check if 'D' key is pressed (Right, bit 1, value 2)
    set rB, rA              ; Copy keyboard state to temp register rB
    and rB, 2               ; Isolate the 'right' bit
    cmp rB, 0               ; Check if the result is non-zero
    jne !move_right         ; If pressed, jump to move_right

    ; Check if 'A' key is pressed (Left, bit 0, value 1)
    set rB, rA              ; Copy keyboard state to temp register rB
    and rB, 1               ; Isolate the 'left' bit
    cmp rB, 0               ; Check if the result is non-zero
    jne !move_left          ; If pressed, jump to move_left

    ; If neither Left nor Right is pressed, the player is standing still
    str [Animation_Stand], rC ; Set animation to 'stand'
    jmp !check_jump         ; Skip movement logic and proceed to jump check

!move_right
    ; Tell the C++ engine to apply rightward acceleration
    str [MOVE_E], rC
    ; Set the animation state for moving right
    str [Animation_MoveR], rC
    jmp !check_jump         ; After moving, proceed to jump check

!move_left
    ; Tell the C++ engine to apply leftward acceleration
    str [MOVE_W], rC
    ; Set the animation state for moving left
    str [Animation_MoveL], rC
    ; Fall through to check_jump

!check_jump
    ; Check if 'W' key is pressed (Jump, bit 2, value 4)
    set rB, rA              ; Copy keyboard state to temp register rB
    and rB, 4               ; Isolate the 'jump' bit
    cmp rB, 0               ; Check if the result is zero (key not pressed)
    je !loop_end            ; If zero, skip the jump logic

    ; If jump key is pressed, tell the C++ engine to jump
    str [MOVE_N], rC
    ; Set the animation state for jumping (overwrites move/stand animation)
    str [Animation_UP], rC

!loop_end
    ; Unconditionally jump back to the beginning of the main loop
    jmp !main
