; MinimalClient.asm - Smart wanderer that patrols and chases enemies.
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
    je !patrol

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
    jmp !patrol

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

!patrol
    ; Patrol behavior based on position and team
    lod rB, [CLIENT_X]
    lod rC, [CLIENT_Y]
    lod rD, [CLIENT_ID]

    ; Use client ID to add variety
    and rD, 3

    ; Move based on position - create patrol patterns
    cmp rB, 320
    jl !patrol_right
    cmp rB, 960
    jg !patrol_left

    ; In middle X zone, check Y
    cmp rC, 240
    jl !patrol_down
    cmp rC, 720
    jg !patrol_up

    ; In center - move based on ID
    cmp rD, 0
    je !patrol_ne
    cmp rD, 1
    je !patrol_se
    cmp rD, 2
    je !patrol_sw
    set rE, 5
    jmp !move

!patrol_ne
    set rE, 7
    jmp !move
!patrol_se
    set rE, 1
    jmp !move
!patrol_sw
    set rE, 3
    jmp !move

!patrol_right
    cmp rC, 480
    jl !patrol_se2
    set rE, 7
    jmp !move
!patrol_se2
    set rE, 1
    jmp !move

!patrol_left
    cmp rC, 480
    jl !patrol_sw2
    set rE, 5
    jmp !move
!patrol_sw2
    set rE, 3
    jmp !move

!patrol_down
    cmp rB, 640
    jl !patrol_se3
    set rE, 3
    jmp !move
!patrol_se3
    set rE, 1
    jmp !move

!patrol_up
    cmp rB, 640
    jl !patrol_ne2
    set rE, 5
    jmp !move
!patrol_ne2
    set rE, 7

!move
    str [MOVE_REQUEST], rE

!yield
    set rB, 1
    str [CLIENT_YIELD], rB
    jmp !loop
