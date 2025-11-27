# Basic compiler settings
CPP = g++
CC = gcc
CPPFLAGS = -std=c++17 -I../ -I../tigr-master/tigr-master
CFLAGS = -I../ -I../tigr-master/tigr-master
WFLAGS = -Wall -Wextra -Wpedantic
LDFLAGS =

# OS-specific settings
ifeq ($(OS),Windows_NT)
    EXE = main.exe
    LDFLAGS += -lopengl32 -lgdi32
    TEENYAT_SRC = ..\\teenyat.c
    TIGR_SRC = ..\\tigr-master\\tigr-master\\tigr.c
else
    EXE = main
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        LDFLAGS += -framework OpenGL -framework Cocoa
    else ifeq ($(UNAME_S),Linux)
        LDFLAGS += -lGLU -lGL -lX11
    endif
    TEENYAT_SRC = ../teenyat.c
    TIGR_SRC = ../tigr-master/tigr-master/tigr.c
endif

OBJS = main.o teenyat.o tigr.o

.PHONY: all clean

all: $(EXE) TagClient.bin

TagClient.bin: TagClient.asm
	.\\tnasm.exe TagClient.asm

$(EXE): $(OBJS)
	$(CPP) $^ -o $@ $(LDFLAGS)

$(EXE): $(OBJS)
	$(CPP) $^ -o $@ $(LDFLAGS)

main.o: main.cpp
	$(CPP) $(CPPFLAGS) $(WFLAGS) -c $< -o $@

teenyat.o: $(TEENYAT_SRC)
	$(CC) $(CFLAGS) $(WFLAGS) -c $< -o $@

tigr.o: $(TIGR_SRC)
	$(CC) $(CFLAGS) $(WFLAGS) -c $< -o $@

test: TagClient.bin
	$(CPP) $(CPPFLAGS) $(WFLAGS) -DENABLE_TESTS -o main_test.exe main.cpp tests.cpp $(TEENYAT_SRC) $(TIGR_SRC) $(LDFLAGS)

clean:
	del /f /q *.o $(EXE)
