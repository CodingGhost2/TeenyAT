; ChaseClient.asm - Branchless version for tnasm
; This version attempts to use arithmetic to circumvent assembler limitations.
.const VISION_SCAN      0x9030
.const VISION_COUNT     0x9031
.const VISION_SELECT    0x9032
.const VISION_X         0x9035
.const VISION_Y         0x9036
.const CLIENT_X         0x9002
.const MOVE_REQUEST     0x9100
.const CLIENT_YIELD     0x9200

!main
!loop
    ; --- VISION ---
    set rA, 1
    str [VISION_SCAN], rA
    lod rA, [VISION_COUNT]
    cmp rA, 0
    je !random_walk ; If no enemies, perform a random walk. This is a backward jump.

    ; --- ENEMY EXISTS: CHASE LOGIC ---
    set rA, 0
    str [VISION_SELECT], rA ; Select the first (nearest) enemy
    lod rB, [VISION_X]      ; rB = relX
    lod rC, [VISION_Y]      ; rC = relY

    ; --- BRANCHLESS DIRECTION CALCULATION ---
    ; This uses an arithmetic trick to determine the primary direction (N, S, E, W).
    ; It calculates d1 = x-y and d2 = x+y, then uses their sign bits.
    ; Direction is determined by the combination of these two sign bits.
    
    ; Setup for SUB. `sub rD, rB, rC` isn't a valid instruction form.
    ; We need to do `neg rC; add rB, rC`
    set rD, 0
    add rD, rC ; rD = rC (relY)
    neg rD     ; rD = -relY
    add rD, rB ; rD = relX - relY
    
    set rE, 0
    add rE, rB ; rE = rB (relX)
    add rE, rC ; rE = relX + relY

    ; Now, rD = x-y and rE = x+y. Get the sign bits.
    ; Shift right by 15 to get the most significant bit (sign bit).
    shf rD, -15 ; rD is now 0 (positive) or 1 (negative)
    shf rE, -15 ; rE is now 0 (positive) or 1 (negative)

    ; Combine the bits to create a 2-bit index: index = (sign(d1) * 2) + sign(d2)
    add rD, rD ; rD = rD * 2
    add rD, rE ; rD is now our index (0, 1, 2, or 3)

    ; Use the index to determine the final direction.
    ; 0 (E), 1 (N), 2 (S), 3 (W)
    cmp rD, 0
    jne !check1
    set rA, 0 ; East
    jmp !apply_move
!check1:
    cmp rD, 1
    jne !check2
    set rA, 6 ; North
    jmp !apply_move
!check2:
    cmp rD, 2
    jne !check3
    set rA, 2 ; South
    jmp !apply_move
!check3:
    set rA, 4 ; West

!apply_move:
    str [MOVE_REQUEST], rA
    jmp !yield

!random_walk:
    lod rA, [CLIENT_X]
    add rA, 1
    mod rA, 8
    str [MOVE_REQUEST], rA

!yield:
    set rA, 1
    str [CLIENT_YIELD], rA
    jmp !loop