; ChaseClient.asm - Pursues the nearest enemy using the Vision API
; Corrected to use the 'lod' instruction as per the TeenyAT documentation.

;  MMIO CONSTANTS 
;  CLIENT IDENTITY 
.const  CLIENT_ID         0x9000 
.const  CLIENT_TEAM       0x9001
.const  CLIENT_X          0x9002 
.const  CLIENT_Y          0x9003 
.const  CLIENT_STATE      0x9004

;  MAP QUERIES 
.const  MAP_QUERY_X       0x9020
.const  MAP_QUERY_Y       0x9021
.const  MAP_RESULT        0x9022

;  VISION QUERIES 
.const  VISION_SCAN       0x9030
.const  VISION_COUNT      0x9031
.const  VISION_SELECT     0x9032
.const  VISION_ID         0x9033
.const  VISION_TEAM       0x9034
.const  VISION_X          0x9035
.const  VISION_Y          0x9036
.const  VISION_DIST       0x9037

;  MOVEMENT COMMANDS 
.const  MOVE_REQUEST      0x9100

;  MESSAGING 
.const  MSG_SEND_TO       0x9120
.const  MSG_SEND_TYPE     0x9121
.const  MSG_SEND_DATA     0x9122
.const  MSG_SEND_EXEC     0x9123

.const  MSG_INBOX_COUNT   0x9130
.const  MSG_READ_IDX      0x9131
.const  MSG_READ_FROM     0x9132
.const  MSG_READ_TYPE     0x9133
.const  MSG_READ_DATA     0x9134
.const  MSG_POP           0x9135

;  CLIENT CONTROL 
.const  CLIENT_YIELD      0x9200

!main
    ; Get my own team ID.
    lod rA, [CLIENT_TEAM]         ; rA  My Team

!loop
    ; --- VISION SCAN ---
    set rB, 1
    str [VISION_SCAN], rB
    
    ; Check how many entities the server found
    lod rC, [VISION_COUNT]         ; rC  vision count
    cmp rC, 0
    je !no_target_found

    ; --- FIND NEAREST ENEMY ---
    set rD, 0 ; rD  Index counter
!check_enemy_loop
    str [VISION_SELECT], rD ; Select entity at index rD
    
    lod rE, [VISION_TEAM]            ; rE  Entity's Team
    
    cmp rA, rE ; Is entity's team the same as my team?
    je !same_team

    ; --- ENEMY FOUND ---
    lod rB, [VISION_X]       ; rB  Relative X
    lod rC, [VISION_Y]       ; rC  Relative Y
    
    ; --- INLINED XY_TO_DIRECTION ---
    cmp rB, 0
    jg !check_east
    jl !check_west
    cmp rC, 0
    jg !is_south
    jl !is_north
    set rE, 0 ; Default to North
    jmp !move

!check_east
    cmp rC, 0
    jg !is_southeast
    jl !is_northeast
    set rE, 2 ; East
    jmp !move
!check_west
    cmp rC, 0
    jg !is_southwest
    jl !is_northwest
    set rE, 6 ; West
    jmp !move

!is_north
    set rE, 0
    jmp !move
!is_northeast
    set rE, 1
    jmp !move
!is_southeast
    set rE, 3
    jmp !move
!is_south
    set rE, 4
    jmp !move
!is_southwest
    set rE, 5
    jmp !move
!is_northwest
    set rE, 7
    jmp !move

!same_team
    inc rD
    lod rC, [VISION_COUNT]           ; Re-read count
    cmp rD, rC
    jl !check_enemy_loop
    
!no_target_found
    ; --- INLINED RANDOM MOVE ---
    add rE, 1
    mod rE, 8
    
!move
    str [MOVE_REQUEST], rE

!end_turn
    set rB, 1
    str [CLIENT_YIELD], rB
    jmp !loop
