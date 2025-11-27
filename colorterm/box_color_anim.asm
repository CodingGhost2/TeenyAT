.const KEY_INPUT       0x4000
.const MOVE_E          0x9010
.const MOVE_W          0x9014
.const MOVE_N          0x9016
.const Animation_Stand 0x9025
.const Animation_MoveL 0x9026
.const Animation_MoveR 0x9027
.const Animation_UP    0x9028
.const SET_FG_COLOR    0x9000
.const SET_BG_COLOR    0x9001
.const CLEAR_SCREEN    0x9002
.const SET_CHAR        0x9003
.const PRINT_CHAR      0x9004
.const SET_CURSOR_VIS  0x9005
.const SET_TITLE       0x9006
.const SET_X           0x9007
.const SET_Y           0x9008
.const GFX_PRESENT     0x9105

; Box geometry constants
.const BOX_X0          2
.const BOX_Y0          2
.const BOX_W           16
.const BOX_H           7
.const BOX_X1          17
.const BOX_Y1          8

; Character codes
.const CH_HORZ         45
.const CH_VERT         124
.const CH_CORNER       43
.const CH_SPACE        32
.const CH_A0           64
.const CH_A1           42

!main
    ; Initialize: hide cursor and clear screen
    set r0, 0
    str [SET_CURSOR_VIS], r0
    str [CLEAR_SCREEN], r0

    ; Set window title: "Box Color Anim" (send chars)
    set r0, 66  ; 'B'
    str [SET_TITLE], r0
    set r0, 111 ; 'o'
    str [SET_TITLE], r0
    set r0, 120 ; 'x'
    str [SET_TITLE], r0
    set r0, 32
    str [SET_TITLE], r0
    set r0, 67  ; 'C'
    str [SET_TITLE], r0
    set r0, 111 ; 'o'
    str [SET_TITLE], r0
    set r0, 108 ; 'l'
    str [SET_TITLE], r0
    set r0, 111 ; 'o'
    str [SET_TITLE], r0
    set r0, 114 ; 'r'
    str [SET_TITLE], r0
    set r0, 32
    str [SET_TITLE], r0
    set r0, 65  ; 'A'
    str [SET_TITLE], r0
    set r0, 110 ; 'n'
    str [SET_TITLE], r0
    set r0, 105 ; 'i'
    str [SET_TITLE], r0
    set r0, 109 ; 'm'
    str [SET_TITLE], r0

    ; --- Draw initial box border inline (uses r3,r4,r5)
    ; Top-left corner
    set r0, BOX_X0
    str [SET_X], r0
    set r0, BOX_Y0
    str [SET_Y], r0
    set r0, CH_CORNER
    str [PRINT_CHAR], r0

    ; Top horizontal line
    set r3, BOX_X0
    inc r3
!top_loop
    cmp r3, BOX_X1
    jge !top_done
    str [SET_X], r3
    set r0, BOX_Y0
    str [SET_Y], r0
    set r0, CH_HORZ
    str [PRINT_CHAR], r0
    inc r3
    jmp !top_loop
!top_done
    ; Top-right corner
    set r0, BOX_X1
    str [SET_X], r0
    set r0, BOX_Y0
    str [SET_Y], r0
    set r0, CH_CORNER
    str [PRINT_CHAR], r0

    ; Bottom-left corner
    set r0, BOX_X0
    str [SET_X], r0
    set r0, BOX_Y1
    str [SET_Y], r0
    set r0, CH_CORNER
    str [PRINT_CHAR], r0

    ; Bottom horizontal line
    set r3, BOX_X0
    inc r3
!bot_loop
    cmp r3, BOX_X1
    jge !bot_done
    str [SET_X], r3
    set r0, BOX_Y1
    str [SET_Y], r0
    set r0, CH_HORZ
    str [PRINT_CHAR], r0
    inc r3
    jmp !bot_loop
!bot_done
    ; Bottom-right corner
    set r0, BOX_X1
    str [SET_X], r0
    set r0, BOX_Y1
    str [SET_Y], r0
    set r0, CH_CORNER
    str [PRINT_CHAR], r0

    ; Vertical sides
    set r4, BOX_Y0
    inc r4
!vert_loop
    cmp r4, BOX_Y1
    jge !vert_done

    ; Left side
    set r0, BOX_X0
    str [SET_X], r0
    str [SET_Y], r4
    set r0, CH_VERT
    str [PRINT_CHAR], r0

    ; Right side
    set r0, BOX_X1
    str [SET_X], r0
    str [SET_Y], r4
    set r0, CH_VERT
    str [PRINT_CHAR], r0

    inc r4
    jmp !vert_loop
!vert_done

    ; Initialize animation state
    set r1, 0           ; frame counter (kept across loop)
    set r2, 0           ; character toggle (0/1)

!main_loop
    ; Compute frame mod 40 -> r3
    set r3, r1
    mod r3, 40

    cmp r3, 10
    jl !color_red
    cmp r3, 20
    jl !color_green
    cmp r3, 30
    jl !color_blue
    jmp !color_yellow

!color_red
    set r0, 0
    str [Animation_Stand], r0
    jmp !after_color
!color_green
    set r0, 1
    str [Animation_MoveL], r0
    jmp !after_color
!color_blue
    set r0, 1
    str [Animation_MoveR], r0
    jmp !after_color
!color_yellow
    set r0, 1
    str [Animation_UP], r0
!after_color

    ; Clear inner box area inline using r4 (row) and r5 (col)
    set r4, BOX_Y0
    inc r4
!row_loop
    cmp r4, BOX_Y1
    jge !clear_done

    set r5, BOX_X0
    inc r5
!col_loop
    cmp r5, BOX_X1
    jge !row_done

    str [SET_X], r5
    str [SET_Y], r4
    set r0, CH_SPACE
    str [PRINT_CHAR], r0

    inc r5
    jmp !col_loop
!row_done
    inc r4
    jmp !row_loop
!clear_done

    ; Compute bobbing Y position: y = BOX_Y0 + 2 + ((frame >> 2) & 1) -> r4 holds Y
    set r4, r1
    shr r4, 2
    and r4, 1
    set r5, BOX_Y0
    add r5, 2
    add r4, r5   ; r4 = Y

    ; Compute drifting X position: x = BOX_X0 + 2 + ((frame >> 1) & 3) -> r5 holds X
    set r5, r1
    shr r5, 1
    and r5, 3
    set r3, BOX_X0
    add r3, 2
    add r5, r3   ; r5 = X

    ; Toggle character: @ or *
    set r3, r2
    and r3, 1
    cmp r3, 0
    je !use_A0
    set r6, CH_A1
    jmp !have_char
!use_A0
    set r6, CH_A0
!have_char

    ; Position cursor and print character at (r5,r4)
    str [SET_X], r5
    str [SET_Y], r4
    str [PRINT_CHAR], r6

    ; Present frame
    set r0, 0
    str [GFX_PRESENT], r0

    ; Delay using r0 as loop counter
    set r0, 1500
!delay_loop
    dec r0
    jne !delay_loop

    ; Advance frame and toggle (r1,r2)
    inc r1
    xor r2, 1

    jmp !main_loop

; ------------------------------------------------------------------
; Halt
; ------------------------------------------------------------------
!halt
    jmp !halt