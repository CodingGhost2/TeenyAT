.const KEY_INPUT 0x4000

!main
    set rC, 0
    set rF, 0
    set rX, 255
    str [KEY_INPUT], rC
    hlt
