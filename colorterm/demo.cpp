#include <iostream>
#include <cstdlib>

#include "rogueutil.h"

using namespace std;
using namespace rogueutil;

int main() { 
    saveDefaultColor();
    setConsoleTitle("Amos Confer");

    cls();
    puts("Hello, world!");
    anykey("Press any key...");

    int key = KEY_ENTER;
    while(key != KEY_ESCAPE) {
        int fg = rand() % (WHITE + 1);
        int bg = rand() % (WHITE + 1);

        setColor(fg);
        setBackgroundColor(bg);
        cls();

        gotoxy(rand() % tcols(), rand() % trows());

        puts("Hello, world!");
        puts("Press any key");
        key = getkey();
    }

    resetColor();
    cls();
    return EXIT_SUCCESS;
}