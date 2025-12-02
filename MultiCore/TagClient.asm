; TagClient.asm - Aggressive tagger that moves toward center.
; Server handles collision avoidance, client picks direction.

;--- MMIO CONSTANTS ---
.const CLIENT_ID 0x9000
.const CLIENT_TEAM 0x9001
.const CLIENT_X 0x9002
.const CLIENT_Y 0x9003
.const VISION_SCAN 0x9030
.const VISION_COUNT 0x9031
.const VISION_SELECT 0x9032
.const VISION_ID 0x9033
.const VISION_TEAM 0x9034
.const VISION_X 0x9035
.const VISION_Y 0x9036
.const VISION_DIST 0x9037
.const MOVE_REQUEST 0x9100
.const TAG_REQUEST 0x9110
.const CLIENT_YIELD 0x9200

!main
    lod rA, [CLIENT_TEAM]

!loop
    ; --- VISION SCAN ---
    set rB, 1
    str [VISION_SCAN], rB
    lod rC, [VISION_COUNT]
    cmp rC, 0
    je !no_target

    ; --- FIND NEAREST ENEMY ---
    set rB, 0
!check_loop
    str [VISION_SELECT], rB
    lod rE, [VISION_TEAM]
    cmp rA, rE
    jne !found_enemy
    inc rB
    lod rC, [VISION_COUNT]
    cmp rB, rC
    jl !check_loop
    jmp !no_target

!found_enemy
    ; Check distance for tag
    lod rE, [VISION_DIST]
    cmp rE, 0
    je !tag_it

    ; Chase enemy
    lod rB, [VISION_X]
    lod rC, [VISION_Y]

    ; Determine direction
    cmp rB, 0
    jg !east_side
    jl !west_side
    cmp rC, 0
    jg !go_south
    set rE, 6
    jmp !move
!go_south
    set rE, 2
    jmp !move

!east_side
    cmp rC, 0
    jg !go_se
    jl !go_ne
    set rE, 0
    jmp !move
!go_se
    set rE, 1
    jmp !move
!go_ne
    set rE, 7
    jmp !move

!west_side
    cmp rC, 0
    jg !go_sw
    jl !go_nw
    set rE, 4
    jmp !move
!go_sw
    set rE, 3
    jmp !move
!go_nw
    set rE, 5
    jmp !move

!tag_it
    lod rB, [VISION_ID]
    str [TAG_REQUEST], rB
    jmp !yield

!no_target
    ; Move towards map center (640, 480)
    lod rB, [CLIENT_X]
    lod rC, [CLIENT_Y]

    ; Determine X direction
    set rD, 0
    cmp rB, 600
    jl !go_east_bias
    cmp rB, 680
    jg !go_west_bias
    jmp !check_y_dir

!go_east_bias
    set rD, 1
    jmp !check_y_dir
!go_west_bias
    set rD, 2

!check_y_dir
    ; Combine X and Y for diagonal
    cmp rC, 440
    jl !go_south_bias
    cmp rC, 520
    jg !go_north_bias

    ; Y is centered, use X direction only
    cmp rD, 1
    je !pure_east
    cmp rD, 2
    je !pure_west
    set rE, 2
    jmp !move
!pure_east
    set rE, 0
    jmp !move
!pure_west
    set rE, 4
    jmp !move

!go_south_bias
    cmp rD, 1
    je !dir_se
    cmp rD, 2
    je !dir_sw
    set rE, 2
    jmp !move
!dir_se
    set rE, 1
    jmp !move
!dir_sw
    set rE, 3
    jmp !move

!go_north_bias
    cmp rD, 1
    je !dir_ne
    cmp rD, 2
    je !dir_nw
    set rE, 6
    jmp !move
!dir_ne
    set rE, 7
    jmp !move
!dir_nw
    set rE, 5

!move
    str [MOVE_REQUEST], rE

!yield
    set rB, 1
    str [CLIENT_YIELD], rB
    jmp !loop
