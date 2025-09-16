.const RAND         0x8010

.const SET_FG_COLOR   0x9000
.const SET_BG_COLOR   0x9001
.const CLEAR_SCREEN   0x9002
.const SET_CHAR       0x9003
.const PRINT_CHAR     0x9004
.const SET_CURSOR_VIS 0x9005
.const SET_TITLE      0x9006
.const SET_X          0x9007
.const SET_Y          0x9008

    ;;; rE = number of ASCII printable character
    set rE, '~'
    sub rE, '!'
    inc rE

!main
    ;;; generate rA, a random printable character
    lod rA, [RAND]
    mod rA, rE
    add rA, '!'

    ;;; pick a random foreground color
    lod rB, [RAND]
    mod rB, 16
    str [SET_FG_COLOR], rB

    ;;; pick a random background color
    lod rB, [RAND]
    mod rB, 16
    str [SET_BG_COLOR], rB

    ;;; pick a random cursor X
    lod rB, [RAND]
    str [SET_X], rB

    ;;; pick a random cursor Y
    lod rB, [RAND]
    str [SET_Y], rB

    str [PRINT_CHAR], rA

    jmp !main
 