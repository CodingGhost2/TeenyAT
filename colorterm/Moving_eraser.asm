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
.const SPRITE_SET     0x9024;        
.const Animation_Stand = 0x9025;  
.const Animation_MoveL = 0x9026;   
.const Animation_MoveR = 0x9027; 
.const Animation_UP = 0x9028;   

.const GFX_INIT = 0x9100;       
.const GFX_CLEAR = 0x9101;    
.const GFX_SET_COLOR = 0x9102;   
.const GFX_SET_POS = 0x9103;     
.const GFX_PUT_PIXEL = 0x9104;   
.const GFX_PRESENT = 0x9105;   
.const COIN = 0x9106;         
.const COIN_SET_POS = 0x9107;
.const COIN_STATUS = 0x9108;  

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


!main
    set 

!animation 
    set rA