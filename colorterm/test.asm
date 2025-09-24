; ==========================================================
;        Constants for Colorterm
; ==========================================================
.const RAND            0x8010
.const SET_FG_COLOR    0x9000
.const SET_BG_COLOR    0x9001
.const CLEAR_SCREEN    0x9002
.const PRINT_CHAR      0x9004
.const SET_X           0x9007
.const SET_Y           0x9008
.const MOVE_E          0x9010
.const MOVE_SE         0x9011
.const MOVE_S          0x9012
.const MOVE_SW         0x9013
.const MOVE_W          0x9014
.const MOVE_NW         0x9015
.const MOVE_N          0x9016
.const MOVE_NE         0x9017
.const MOVE            0x9020

; ==========================================================
;        Main Program
; ==========================================================
!main
    ; === Phase 1: Setup - Spatter 'O's on the screen ===
    
    str [CLEAR_SCREEN], rZ       ; Clear the terminal
    
    set rA, 9                    ; Set foreground color to blue
    str [SET_FG_COLOR], rA
    
    set rB, 'O'                  ; Character to print
    set rC, 250                  ; How many 'O's to draw

!draw_loop
    lod rA, [RAND]               ; Get a random number
    str [SET_X], rA              ; Use it for the X coordinate
    
    lod rA, [RAND]               ; Get another random number
    str [SET_Y], rA              ; Use it for the Y coordinate
    
    str [PRINT_CHAR], rB         ; Print the 'O'
    
    dec rC
    cmp rC, 0
    jne !draw_loop

    ; === Phase 2: Eraser - Hybrid wandering and erasing ===

    set rA, 0                    ; Set background to black
    str [SET_BG_COLOR], rA
    
    set rB, ' '                  ; The "eraser" is just a space
    set rD, 0                    ; Initialize the counter

!eraser_loop
    inc rD
    cmp rD, 50                   ; Check if counter equals 50
    je !fixed_path               ; If so, jump to the fixed path

    ; --- Random Wander Section ---
    lod rA, [RAND]               
    str [MOVE], rA
    str [PRINT_CHAR], rB
    jmp !eraser_loop

!fixed_path
    ; --- Fixed Path Section (A Square) ---
    set rE, 4                   ; Counter for the square's sides

!square_loop
    str [MOVE_E], rZ
    str [PRINT_CHAR], rB
    str [MOVE_S], rZ
    str [PRINT_CHAR], rB
    str [MOVE_W], rZ
    str [PRINT_CHAR], rB
    str [MOVE_N], rZ
    str [PRINT_CHAR], rB

    dec rE
    cmp rE, 0
    jne !square_loop

    ; --- Reset and Return ---
    set rD, 0                    ; Reset the main counter
    jmp !eraser_loop             ; Return to the main loop