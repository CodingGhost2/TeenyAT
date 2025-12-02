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

echo [3/4] Compiling main.cpp...
g++ -std=c++17 -I../ -I../tigr-master/tigr-master -Wall -Wno-unused-variable -c main.cpp -o main.o
if errorlevel 1 (
    echo ERROR: Failed to compile main.cpp
    pause
    exit /b 1
)

REM Link everything
echo [4/4] Linking main.exe...
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
echo   FEATURES:
echo   - Maze battlefield with walls and corridors
echo   - Both teams can tag each other
echo   - 4 AI behavior types:
echo       * AGGRESSOR - Direct chase
echo       * FLANKER   - Approach from angles
echo       * DEFENDER  - Guard spawn area
echo       * PACK      - Team-based attacks
echo   - Retreat when outnumbered
echo   - Wall sliding movement
echo.
echo =====================================================
