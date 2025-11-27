; DebugClient.asm - Sends a message and moves randomly (Rewritten for tnasm, rA-rE only)
.const CLIENT_ID        0x9000
.const MOVE_REQUEST     0x9100
.const CLIENT_YIELD     0x9200

.const MSG_SEND_TO      0x9120
.const MSG_SEND_TYPE    0x9121
.const MSG_SEND_DATA    0x9122
.const MSG_SEND_EXEC    0x9123

!main
    ; This client will send a message to client 1 (only if my ID is 0)
    lod rA, [CLIENT_ID]    ; rA = My client ID
    cmp rA, 0
    jne !not_sender        ; If not client 0, skip sending message

    ; Send message to client 1
    set rB, 1              ; rB = Recipient ID (Client 1)
    str [MSG_SEND_TO], rB
    set rC, 0xAB           ; rC = Message Type
    str [MSG_SEND_TYPE], rC
    set rD, 0x1234         ; rD = Message Data
    str [MSG_SEND_DATA], rD
    set rE, 1              ; rE = 1 (send command)
    str [MSG_SEND_EXEC], rE

!not_sender
    ; Main game loop
!loop
    ; Move randomly (move east for now)
    set rA, 2              ; rA = Direction 2 (East)
    str [MOVE_REQUEST], rA
    
    ; Yield CPU back to server
    set rA, 1              ; rA = 1 (yield command)
    str [CLIENT_YIELD], rA
    
    jmp !loop
