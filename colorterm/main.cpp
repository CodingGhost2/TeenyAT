#include <iostream>
#include <cstdlib>

#include "teenyat.h"
#include "rogueutil.h"
#include "tigr.h"

using namespace std;
using namespace rogueutil;

const tny_uword SET_FG_COLOR = 0x9000;
const tny_uword SET_BG_COLOR = 0x9001;
const tny_uword CLEAR_SCREEN = 0x9002;
const tny_uword SET_CHAR = 0x9003;
const tny_uword PRINT_CHAR = 0x9004;
const tny_uword SET_CURSOR_VIS = 0x9005;
const tny_uword SET_TITLE = 0x9006;
const tny_uword SET_X = 0x9007;
const tny_uword SET_Y = 0x9008;
const tny_uword MOVE_E = 0x9010;
const tny_uword MOVE_SE = 0x9011;
const tny_uword MOVE_S = 0x9012;
const tny_uword MOVE_SW = 0x9013;
const tny_uword MOVE_W = 0x9014;
const tny_uword MOVE_NW = 0x9015;
const tny_uword MOVE_N = 0x9016;
const tny_uword MOVE_NE = 0x9017;
const tny_uword MOVE = 0x9020;
                /*E,SE, S,SW, W, NW,N,NE*/
const int dx[8] ={1, 1, 0,-1, -1,-1,-1, 1};
const int dy[8] ={0, 1, 1, 1, 0, -1,-1,-1};


int x = 0;
int y = 0;
int interp_v = 0;

string title = "colorterm";

void graphic(teenyat *t, tny_uword addr, tny_word data, uint16_t * delay) {
    
    return;
}

void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay);

int main(int argc, char *argv[]) { 
    saveDefaultColor();
    setConsoleTitle("TeenyAT Color Terminal");
    cls();

    setvbuf(stdout, NULL, _IONBF, 0);

    FILE *bin_file = fopen(argv[1], "rb");
	teenyat t;
	tny_init_from_file(&t, bin_file, nullptr, bus_write);

    for(;;) {
        tny_clock(&t);
    }

    resetColor();
    cls();
    return EXIT_SUCCESS;
}


void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay) {
    switch(addr) {
    case SET_FG_COLOR:
        setColor(data.u);
        break;
    case SET_BG_COLOR:
        setBackgroundColor(data.u);
        break;
    case CLEAR_SCREEN:
        cls();
        x = 0;
        y = 0;
        break;
    case SET_CHAR:
        setChar(data.u);
        break;
    case PRINT_CHAR:
        cout << (char)(data.bytes.byte0);
        x = (x + 1) % tcols();
        gotoxy(x, y);
        break;
    case SET_CURSOR_VIS:
        setCursorVisibility(data.u);
        break;
    case SET_TITLE:
        title += (char)(data.bytes.byte0);
        setConsoleTitle(title.c_str());
        break;
    case SET_X:
        x = data.u % tcols();
        gotoxy(x, y);
        break;
    case SET_Y:
        y = data.u % trows();
        gotoxy(x, y);
        break;

    case MOVE:
        interp_v = ((data.s % 8) + 8) % 8;
        x = ((x + dx[interp_v]) % tcols() + tcols()) % tcols();
        y = ((y + dy[interp_v]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
        
    case MOVE_E:
        x = ((x + dx[0]) % tcols() + tcols()) % tcols();
        y = ((y + dy[0]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    case MOVE_SE:
        x = ((x + dx[1]) % tcols() + tcols()) % tcols();
        y = ((y + dy[1]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    case MOVE_S:
        x = ((x + dx[2]) % tcols() + tcols()) % tcols();
        y = ((y + dy[2]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    case MOVE_SW:
        x = ((x + dx[3])% tcols() + tcols()) % tcols();
        y = ((y + dy[3])% trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    case MOVE_W:
        x = ((x + dx[4]) % tcols() + tcols()) % tcols();
        y = ((y + dy[4]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    case MOVE_NW:
        x = ((x + dx[5]) % tcols() + tcols()) % tcols();
        y = ((y + dy[5]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    case MOVE_N:
        x = ((x + dx[6]) % tcols() + tcols()) % tcols();
        y = ((y + dy[6]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    case MOVE_NE:
        x = ((x + dx[7]) % tcols() + tcols()) % tcols();
        y = ((y + dy[7]) % trows() + trows()) % trows();
        gotoxy(x,y);
        break;
    default:
        break;
    }


    return;
}
