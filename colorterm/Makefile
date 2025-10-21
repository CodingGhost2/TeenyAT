CFLAGS += -I.. -I./include -O3 
WFLAGS = -Wall -Wextra -Wpedantic
CPPFLAGS += $(CFLAGS) --std=c++17
# Assume BUILD_DIR, SEP, MSG, EXE_EXT are defined in the root Makefile
# If not defined, hardcode them for local compilation:
BUILD_DIR = build
SEP = /
MSG =
EXE_EXT = .exe

ifeq ($(OS),Windows_NT)
    LDFLAGS += -s -lopengl32 -lgdi32
    SEP = \\  # Use double backslash for Windows paths in Makefiles
    EXE_EXT = .exe
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        LDFLAGS += -framework OpenGL -framework Cocoa
    else ifeq ($(UNAME_S),Linux)
        LDFLAGS += -s -lGLU -lGL -lX11
    endif
endif

LCD_BUILD_DIR = ..$(SEP)$(BUILD_DIR)$(SEP)platformer
BIN = $(LCD_BUILD_DIR)$(SEP)engine$(EXE_EXT)

.PHONY: all directories

all: directories
	@echo $(MSG) Building the Platformer Engine
	
	# Compile C files
	# TIGR source path corrected based on provided directory structure image.
	gcc -c ../tigr-master/src/tigr.c -o $(LCD_BUILD_DIR)$(SEP)tigr.o $(CFLAGS) 
	# Teenyat source is one level up
	gcc -c ../teenyat.c -o $(LCD_BUILD_DIR)$(SEP)teenyat.o $(CFLAGS) $(WFLAGS)
	
	# Compile C++ files (Platformer Core)
	g++ -c rogueutil.cpp -o $(LCD_BUILD_DIR)$(SEP)rogueutil.o $(CPPFLAGS) $(WFLAGS)
	# g++ -c color.cpp -o $(LCD_BUILD_DIR)$(SEP)color.o $(CPPFLAGS) $(WFLAGS)  <-- Removed
	# g++ -c util.cpp -o $(LCD_BUILD_DIR)$(SEP)util.o $(CPPFLAGS) $(WFLAGS)    <-- Removed
	# g++ -c screen.cpp -o $(LCD_BUILD_DIR)$(SEP)screen.o $(CPPFLAGS) $(WFLAGS)  <-- Removed
	g++ -c main.cpp -o $(LCD_BUILD_DIR)$(SEP)main.o $(CPPFLAGS) $(WFLAGS)
	
	# Link all object files
	g++ $(LCD_BUILD_DIR)$(SEP)tigr.o \
		$(LCD_BUILD_DIR)$(SEP)teenyat.o $(LCD_BUILD_DIR)$(SEP)main.o \
		$(LCD_BUILD_DIR)$(SEP)rogueutil.o \
		-o $(BIN) $(LDFLAGS)

directories:
	@echo $(MSG) Creating Engine build directory
	@mkdir -p $(subst \\,/,$(LCD_BUILD_DIR))
