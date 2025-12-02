@echo off
echo =====================================================
echo      TACTICAL TAG WAR - Build Script
echo =====================================================
echo.

REM Clean old files
echo [1/4] Cleaning old files...
del /f /q *.o 2>nul
del /f /q main.exe 2>nul

REM Compile the C/C++ code
echo [2/4] Compiling teenyat.c...
gcc -I../ -I../tigr-master/tigr-master -Wall -c ../teenyat.c -o teenyat.o
if errorlevel 1 (
    echo ERROR: Failed to compile teenyat.c
    pause
    exit /b 1
)

echo [2/4] Compiling tigr.c...
gcc -I../ -I../tigr-master/tigr-master -Wall -c ../tigr-master/tigr-master/tigr.c -o tigr.o
if errorlevel 1 (
    echo ERROR: Failed to compile tigr.c
    pause
    exit /b 1
)

echo [3/4] Assembling client programs...
.\tnasm ChaseClient.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble ChaseClient.asm
    pause
    exit /b 1
)
.\tnasm TagClient.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble TagClient.asm
    pause
    exit /b 1
)
.\tnasm MinimalClient.asm
if errorlevel 1 (
    echo ERROR: Failed to assemble MinimalClient.asm
    pause
    exit /b 1
)

echo [4/5] Compiling main.cpp...
g++ -std=c++17 -I../ -I../tigr-master/tigr-master -Wall -Wno-unused-variable -c main.cpp -o main.o
if errorlevel 1 (
    echo ERROR: Failed to compile main.cpp
    pause
    exit /b 1
)

REM Link everything
echo [5/5] Linking main.exe...
g++ main.o teenyat.o tigr.o -o main.exe -lopengl32 -lgdi32
if errorlevel 1 (
    echo ERROR: Failed to link main.exe
    pause
    exit /b 1
)

echo.
echo =====================================================
echo   BUILD SUCCESSFUL!
echo =====================================================
echo.
echo   Run: main.exe
echo.
echo   MULTIAGENT SYSTEM FEATURES:
echo   - 20 TeenyAT VM clients (10 per team)
echo   - 3 different ASM client programs:
echo       * ChaseClient.asm  - Direct pursuit
echo       * TagClient.asm    - Center-focused tagger
echo       * MinimalClient.asm - Smart wanderer
echo   - Server-authoritative collision detection
echo   - MOVE_RESULT feedback for collision recovery
echo   - Vision system with line-of-sight
echo   - 4 rotating battlefield maps
echo.
echo =====================================================
