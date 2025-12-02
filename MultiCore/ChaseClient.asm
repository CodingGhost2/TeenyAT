; ChaseClient.asm - Pursues and tags the nearest enemy.
; Server handles collision avoidance, client just picks direction.

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

    ; Chase - get relative position
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
    ; Wander towards center
    lod rB, [CLIENT_X]
    cmp rB, 640
    jl !wander_east
    set rE, 4
    jmp !move
!wander_east
    set rE, 0

!move
    str [MOVE_REQUEST], rE

!yield
    set rB, 1
    str [CLIENT_YIELD], rB
    jmp !loop
