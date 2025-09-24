; Constants for the color terminal
.const RAND           0x8010

.const SET_FG_COLOR   0x9000
.const SET_BG_COLOR   0x9001
.const CLEAR_SCREEN   0x9002
.const SET_CHAR       0x9003
.const PRINT_CHAR     0x9004
.const SET_CURSOR_VIS 0x9005
.const SET_TITLE      0x9006
.const SET_X          0x9007
.const SET_Y          0x9008
.const MOVE_E         0x9010
.const MOVE_SE        0x9011
.const MOVE_S         0x9012
.const MOVE_SW        0x9013
.const MOVE_W         0x9014
.const MOVE_NW        0x9015
.const MOVE_N         0x9016
.const MOVE_NE        0x9017
.const MOVE           0x9020

!main
    ;;; Set the character to 'X'
    set rA, 'X'

    ;;; Pick a random starting cursor X
    lod rB, [RAND]
    str [SET_X], rB

    ;;; Pick a random starting cursor Y
    lod rB, [RAND]
    str [SET_Y], rB

    ;;; Print the first 'X' at the random spot
    str [PRINT_CHAR], rA

!loop_start
    ;;; Move East and print
    str [MOVE_E], rZ         ; Correctly triggers the move
    str [PRINT_CHAR], rA

    ;;; Move South-East and print
    str [MOVE_SE], rZ
    str [PRINT_CHAR], rA

    ;;; Move South and print
    str [MOVE_S], rZ
    str [PRINT_CHAR], rA

    ;;; Move South-West and print
    str [MOVE_SW], rZ
    str [PRINT_CHAR], rA

    ;;; Move West and print
    str [MOVE_W], rZ
    str [PRINT_CHAR], rA

    ;;; Move North-West and print
    str [MOVE_NW], rZ
    str [PRINT_CHAR], rA

    ;;; Move North and print
    str [MOVE_N], rZ
    str [PRINT_CHAR], rA

    ;;; Move North-East and print
    str [MOVE_NE], rZ
    str [PRINT_CHAR], rA

    ;;; Jump back to the beginning of the loop to repeat
    jmp !loop_start