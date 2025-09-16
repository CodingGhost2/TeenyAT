#include <iostream>
#include <cstdlib>

#include "teenyat.h"
#include "rogueutil.h"

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

int x = 0;
int y = 0;

string title = "";

void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay);

int main(int argc, char *argv[]) { 
    saveDefaultColor();
    setConsoleTitle("TeenyAT Color Terminal");
    cls();

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
    default:
        break;
    }


    return;
}
