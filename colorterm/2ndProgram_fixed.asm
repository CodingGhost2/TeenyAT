.const KEY_INPUT       0x4000
.const MOVE_E          0x9010
.const MOVE_W          0x9014
.const MOVE_N          0x9016
.const Animation_Stand 0x9025
.const Animation_MoveL 0x9026
.const Animation_MoveR 0x9027
.const Animation_UP    0x9028
.const SET_FG_COLOR    0x9000
.const GFX_PRESENT     0x9105

.const SELECT_DEMO     0

!main
    set rC, 0
    set rD, SELECT_DEMO
    cmp rD, 0
    je !run_move
    jmp !run_anim

; ------------------------------------------------------------------
; DEMO_MOVE: automatically move right and occasionally jump
; ------------------------------------------------------------------
!run_move
    set rE, 0        ; step counter
    set rF, 0        ; delay counter

!move_loop
    str [MOVE_E], rC
    inc rE
    cmp rE, 60
    jne !no_jump
    str [MOVE_N], rC
    set rE, 0

!no_jump
    str [GFX_PRESENT], rC
    set rF, 2000
!delay1
    dec rF
    jne !delay1
    jmp !move_loop

; ------------------------------------------------------------------
; DEMO_ANIM: cycle foreground color and animation states
; ------------------------------------------------------------------
!run_anim
    set rG, 0
    set rH, 0

!anim_loop
    ; RED
    set rX, 255
    str [SET_FG_COLOR], rX
    set rY, 0
    str [Animation_Stand], rY
    str [GFX_PRESENT], rC
    set rZ, 3000
!delay2
    dec rZ
    jne !delay2

    ; GREEN
    set rX, 3840
    str [SET_FG_COLOR], rX
    set rY, 1
    str [Animation_MoveR], rY
    str [GFX_PRESENT], rC
    set rZ, 2000
!delay3
    dec rZ
    jne !delay3

    ; BLUE
    set rX, 15
    str [SET_FG_COLOR], rX
    set rY, 1
    str [Animation_MoveL], rY
    str [GFX_PRESENT], rC
    set rZ, 2000
!delay4
    dec rZ
    jne !delay4

    ; JUMP anim
    set rY, 0
    str [Animation_UP], rY
    set rZ, 1000
!delay5
    dec rZ
    jne !delay5

    jmp !anim_loop

; ------------------------------------------------------------------
; Halt (never reached in these demos)
; ------------------------------------------------------------------
!halt
    hlt
