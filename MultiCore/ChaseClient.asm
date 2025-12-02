; ChaseClient.asm - Pursues and chases the nearest enemy with wall avoidance.
; Implements intelligent pathfinding and obstacle detection.

;--- MMIO CONSTANTS ---
.const CLIENT_ID 0x9000
.const CLIENT_TEAM 0x9001
.const CLIENT_X 0x9002
.const CLIENT_Y 0x9003
.const MAP_QUERY_X 0x9020
.const MAP_QUERY_Y 0x9021
.const MAP_RESULT 0x9022
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
    lod rA, [CLIENT_TEAM] ; rA = My Team (persistent)

!loop
    ; --- VISION SCAN ---
    set rB, 1
    str [VISION_SCAN], rB
    lod rC, [VISION_COUNT]
    cmp rC, 0
    je !no_target_found

    ; --- FIND NEAREST ENEMY ---
    set rD, 0 ; rD = Index counter
!check_enemy_loop
    str [VISION_SELECT], rD
    lod rE, [VISION_TEAM]
    cmp rA, rE ; Is it an enemy?
    jne !enemy_found
    
!same_team
    inc rD
    lod rC, [VISION_COUNT] ; Re-read count
    cmp rD, rC
    jl !check_enemy_loop
    jmp !no_target_found ; All visible entities were teammates

!enemy_found
    ; --- ENEMY FOUND, CHECK DISTANCE ---
    lod rE, [VISION_DIST] ; Reuse rE for distance
    cmp rE, 0 ; Is distance 0 tiles?
    je !tag_enemy

    ; --- ENEMY NOT CLOSE, CHASE ---
    lod rB, [VISION_X] ; Reuse rB for Relative X
    lod rC, [VISION_Y] ; Reuse rC for Relative Y
    
    ; --- INLINED XY_TO_DIRECTION (Result in rE) ---
    cmp rB, 0
    jg !check_east
    jl !check_west
    ; if relX == 0, check relY
    cmp rC, 0
    jg !is_south
    jl !is_north
    ; if relX and relY are 0, we should have tagged, but as a fallback, yield.
    jmp !end_turn

!check_east
    cmp rC, 0
    jg !is_southeast
    jl !is_northeast
    set rE, 0 ; East
    jmp !navigate
!check_west
    cmp rC, 0
    jg !is_southwest
    jl !is_northwest
    set rE, 4 ; West
    jmp !navigate

!is_north
    set rE, 6
    jmp !navigate
!is_northeast
    set rE, 7
    jmp !navigate
!is_southeast
    set rE, 1
    jmp !navigate
!is_south
    set rE, 2
    jmp !navigate
!is_southwest
    set rE, 3
    jmp !navigate
!is_northwest
    set rE, 5
    jmp !navigate

!tag_enemy
    ; --- ENEMY IS CLOSE, TAG ---
    lod rB, [VISION_ID] ; Reuse rB for the target's ID
    str [TAG_REQUEST], rB
    jmp !end_turn

    jmp !navigate

!no_target_found
    ; --- NO TARGET, MOVE NORTH ---
    set rE, 6
    jmp !move ; Don't bother with wall check for random movement

!move
    str [MOVE_REQUEST], rE
    jmp !end_turn

!navigate
    ; --- WALL DETECTION ---
    ; rE holds the desired direction. Check if it's a wall.

    ; 1. Get current position in tiles, using rD for the divisor to protect rA
    lod rB, [CLIENT_X]
    lod rC, [CLIENT_Y]
    set rD, 32
    div rB, rD ; rB = my tile X
    div rC, rD ; rC = my tile Y

    ; 2. Calculate target tile coordinates (rA is now safe to use as a temp register)
    set rA, 0  ; rA = rB (target tile X)
    add rA, rB
    set rD, 0  ; rD = rC (target tile Y)
    add rD, rC
    
    cmp rE, 0 ; E
    jne !not_E
    inc rA
    jmp !query_map
!not_E
    cmp rE, 1 ; SE
    jne !not_SE
    inc rD
    inc rA
    jmp !query_map
!not_SE
    cmp rE, 2 ; S
    jne !not_S
    inc rD
    jmp !query_map
!not_S
    cmp rE, 3 ; SW
    jne !not_SW
    inc rD
    dec rA
    jmp !query_map
!not_SW
    cmp rE, 4 ; W
    jne !not_W
    dec rA
    jmp !query_map
!not_W
    cmp rE, 5 ; NW
    jne !not_NW
    dec rD
    dec rA
    jmp !query_map
!not_NW
    cmp rE, 6 ; N
    jne !not_N
    dec rD
    jmp !query_map
!not_N
    ; Must be NE (7)
    dec rD
    inc rA

!query_map
    str [MAP_QUERY_X], rA
    str [MAP_QUERY_Y], rD
    lod rA, [MAP_RESULT]
    cmp rA, 1 ; Is it a wall?
    je !handle_wall ; If so, try to navigate around it.

    ; 3. Path is clear, proceed with move
    jmp !move

!handle_wall
    ; Wall detected. Try turning left or right relative to original direction (rE)
    ; For simplicity, we'll try turning right first. (e.g. if heading East (0), try Southeast(1))
    add rE, 1
    and rE, 7 ; Wrap around 8
    
    ; We need to re-calculate the target tile based on the new direction
    ; This is repetitive, a subroutine would be ideal, but for now we'll inline it.
    lod rB, [CLIENT_X]
    lod rC, [CLIENT_Y]
    set rD, 32
    div rB, rD ; rB = my tile X
    div rC, rD ; rC = my tile Y
    set rA, 0
    add rA, rB
    set rD, 0
    add rD, rC
    
    cmp rE, 0 ; E
    jne !not_E_2
    inc rA
    jmp !query_map_2
!not_E_2
    cmp rE, 1 ; SE
    jne !not_SE_2
    inc rD
    inc rA
    jmp !query_map_2
!not_SE_2
    cmp rE, 2 ; S
    jne !not_S_2
    inc rD
    jmp !query_map_2
!not_S_2
    cmp rE, 3 ; SW
    jne !not_SW_2
    inc rD
    dec rA
    jmp !query_map_2
!not_SW_2
    cmp rE, 4 ; W
    jne !not_W_2
    dec rA
    jmp !query_map_2
!not_W_2
    cmp rE, 5 ; NW
    jne !not_NW_2
    dec rD
    dec rA
    jmp !query_map_2
!not_NW_2
    cmp rE, 6 ; N
    jne !not_N_2
    dec rD
    jmp !query_map_2
!not_N_2
    ; Must be NE (7)
    dec rD
    inc rA

!query_map_2
    str [MAP_QUERY_X], rA
    str [MAP_QUERY_Y], rD
    lod rA, [MAP_RESULT]
    cmp rA, 1 ; Is it a wall?
    je !end_turn ; If the second attempt is also a wall, give up.

    jmp !move ; The new path is clear!

!end_turn
    set rB, 1
    str [CLIENT_YIELD], rB
    jmp !loop
