;-------------------
; TeenyAT Demo Program - Shows movement and animation capabilities
; Controls a sprite using memory-mapped I/O for movement and display
;-------------------

; I/O Address Constants
.const KEY_INPUT       0x4000  ; Keyboard input bitmask
.const MOVE_E          0x9010  ; Move East (Right)
.const MOVE_W          0x9014  ; Move West (Left)
.const MOVE_N          0x9016  ; Move North (Jump)
.const Animation_Stand 0x9025  ; Standing animation state
.const Animation_MoveL 0x9026  ; Moving left animation
.const Animation_MoveR 0x9027  ; Moving right animation
.const Animation_UP    0x9028  ; Jumping animation
.const SET_FG_COLOR    0x9000  ; Set foreground color
.const GFX_PRESENT     0x9105  ; Present graphics frame
; Terminal/printing
.const PRINT_CHAR      0x9004  ; Print character and advance cursor

; Demo Configuration
.const SELECT_DEMO     1       ; Animation demo selected

;-------------------
; Program Entry Point
;-------------------
!main
    set rC, 0          ; Initialize control register
    set rD, SELECT_DEMO ; Load demo selector
    cmp rD, 0          ; Check which demo to run
    je !run_move       ; If 0, run movement demo
    jmp !run_anim      ; Otherwise run animation demo
    
;-------------------
; Movement Demo - Auto-moves right and jumps periodically
;-------------------
!run_move
    set rD, 0          ; Step counter (using rD)
    set rE, 0          ; Delay counter (using rE)

!move_loop
    str [MOVE_E], rC   ; Move right
    inc rD             ; Increment step count
    cmp rD, 60         ; Check if time to jump
    jne !no_jump       ; Skip jump if not time
    str [MOVE_N], rC   ; Trigger jump
    set rD, 0          ; Reset step counter

!no_jump
    str [GFX_PRESENT], rC ; Update display
    set rE, 2000       ; Set delay duration
!delay1
    dec rE             ; Count down delay
    jne !delay1        ; Loop until delay done
    jmp !move_loop     ; Continue moving

;-------------------
; Animation Demo - Cycles colors and animation states
;-------------------
!run_anim
    set rA, 0          ; Initialize counters
    set rB, 0

!anim_loop
    ; Red color + standing
    set rD, 255        ; Red component
    str [SET_FG_COLOR], rD
    set rE, 0          ; Standing animation
    str [Animation_Stand], rE
    str [GFX_PRESENT], rC
    ; Print a colored marker to the terminal to show current color
    set rB, '*'
    str [PRINT_CHAR], rB
    ; Also nudge the player right so movement is visible even if spriteIndex
    ; rendering isn't wired in the prebuilt executable.
    str [MOVE_E], rC   ; issue move-right write
    set rA, 3000       ; Animation delay
!delay2
    dec rA
    jne !delay2

    ; Green color + move right
    set rD, 3840       ; Green component
    str [SET_FG_COLOR], rD
    set rE, 1
    str [Animation_MoveR], rE
    str [GFX_PRESENT], rC
    set rB, '*'
    str [PRINT_CHAR], rB
    ; continue moving right a bit to show motion
    str [MOVE_E], rC
    set rA, 2000
!delay3
    dec rA
    jne !delay3

    ; Blue color + move left
    set rD, 15         ; Blue component
    str [SET_FG_COLOR], rD
    set rE, 1
    str [Animation_MoveL], rE
    str [GFX_PRESENT], rC
    set rB, '*'
    str [PRINT_CHAR], rB
    ; then nudge left to create a back-and-forth
    str [MOVE_W], rC
    set rA, 2000
!delay4
    dec rA
    jne !delay4

    ; Jump animation
    set rE, 0
    str [Animation_UP], rE
    set rB, '*'
    str [PRINT_CHAR], rB
    set rA, 1000
!delay5
    dec rA
    jne !delay5

    jmp !anim_loop     ; Repeat animation cycle

;-------------------
; Program End
;-------------------
!halt
    jmp !halt        ; Loop forever