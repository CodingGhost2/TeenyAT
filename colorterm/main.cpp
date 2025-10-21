// ============================================================================
// TeenyAT Color Terminal - Main Entry Point
// ============================================================================
// This file implements a game engine using the TeenyAT virtual machine,
// rogueutil for terminal control, and TIGR for graphics rendering.
// ============================================================================

#include <iostream>
#include <cstdlib>
#include <cstdio>

#include "teenyat.h"
#include "rogueutil.h"
#include "tigr.h"

using namespace std;
using namespace rogueutil;

// ============================================================================
// MEMORY-MAPPED I/O ADDRESSES
// ============================================================================

// --- INPUT ADDRESSES (Read by TeenyAT) ---
const tny_uword KEY_INPUT = 0x4000; 			// Read keyboard state (bitmask)
										// Bit 0: Left (A key)
										// Bit 1: Right (D key)
										// Bit 2: Jump (W key)

// --- TERMINAL CONTROL ADDRESSES (0x9000 - 0x902F) ---
const tny_uword SET_FG_COLOR = 0x9000; 		// Set foreground text color
const tny_uword SET_BG_COLOR = 0x9001; 		// Set background text color
const tny_uword CLEAR_SCREEN = 0x9002; 		// Clear the terminal screen
const tny_uword SET_CHAR = 0x9003; 			// Set character at cursor position
const tny_uword PRINT_CHAR = 0x9004; 		// Print character and advance cursor
const tny_uword SET_CURSOR_VIS = 0x9005; 	// Show/hide cursor
const tny_uword SET_TITLE = 0x9006; 		// Set window title (char by char)
const tny_uword SET_X = 0x9007; 			// Set cursor X position
const tny_uword SET_Y = 0x9008; 			// Set cursor Y position

// --- MOVEMENT ADDRESSES (0x9010 - 0x9020) ---
const tny_uword MOVE_E = 0x9010; 			// Move East (Right)
const tny_uword MOVE_SE = 0x9011; 			// Move South-East
const tny_uword MOVE_S = 0x9012; 			// Move South
const tny_uword MOVE_SW = 0x9013; 			// Move South-West
const tny_uword MOVE_W = 0x9014; 			// Move West (Left)
const tny_uword MOVE_NW = 0x9015; 			// Move North-West
const tny_uword MOVE_N = 0x9016; 			// Move North (Jump)
const tny_uword MOVE_NE = 0x9017; 			// Move North-East
const tny_uword MOVE = 0x9020; 				// Generic move (direction in data)

// --- SPRITE CONTROL (Optional Feature) ---
const tny_uword SPRITE_SET = 0x9024; 		// Change player sprite/texture
const tny_uword Animation_Stand = 0x9025; 	// Set animation for standing still
const tny_uword Animation_MoveL = 0x9026; 	// Set animation for moving left
const tny_uword Animation_MoveR = 0x9027; 	// Set animation for moving right
const tny_uword Animation_UP = 0x9028; 		// Set animation for jumping

// --- GRAPHICS ADDRESSES (0x9100 - 0x911F) ---
const tny_uword GFX_INIT = 0x9100; 			// Initialize graphics window
const tny_uword GFX_CLEAR = 0x9101; 		// Clear graphics buffer
const tny_uword GFX_SET_COLOR = 0x9102; 	// Set drawing color (RGB)
const tny_uword GFX_SET_POS = 0x9103; 		// Set drawing position (X, Y)
const tny_uword GFX_PUT_PIXEL = 0x9104; 	// Draw pixel at current position
const tny_uword GFX_PRESENT = 0x9105; 		// Swap/present frame buffer

const tny_uword COIN = 0x9106; 				// Coin Object
const tny_uword COIN_SET_POS = 0x9107; 		// Coin (X, Y) location
const tny_uword COIN_STATUS = 0x9108; 		// Coin Current status

// ============================================================================
// DIRECTION VECTORS FOR 8-DIRECTIONAL MOVEMENT
// ============================================================================
						/*E,SE, S,SW, W, NW,N,NE*/
const int dx[8] ={1, 1, 0,-1, -1,-1,-1, 1};
const int dy[8] ={0, 1, 1, 1, 0, -1,-1,-1};

Tigr* screen;

// --- Terminal State ---
int x = 0; 						// Current cursor X position
int y = 0; 						// Current cursor Y position
int interp_v = 0; 				// Interpreted direction value
string title = "SuperLeroy"; 	// Window title

// ============================================================================
// PLAYER OBJECT
// ============================================================================
struct Player {
	float x, y; 				// Position in world coordinates
	float velocityX, velocityY; // Current velocity
	int spriteIndex; 			// Current sprite/texture index
	bool onGround; 				// Is player touching ground?
	int TileX, TileY; 			// Current player tile position
};
Player player = {10.0f, 250.0f, 0.0f, 0.0f, 0, false};
const float RESPAWN_X = 10.0f;
const float RESPAWN_Y = 250.0f; // Set this to the safe starting Y position

// ============================================================================
// LEVEL DATA
// ============================================================================
const int TILE_SIZE = 32; 	// Size of each tile in pixels
const int SCREEN_WIDTH_PIXELS = 630;
const int SCREEN_HEIGHT_PIXELS = 340;
const int LEVEL_WIDTH = (SCREEN_WIDTH_PIXELS / TILE_SIZE) + 1;  // 19 + 1 = 20
const int LEVEL_HEIGHT = (SCREEN_HEIGHT_PIXELS / TILE_SIZE) + 1; // 10 + 1 = 11

// Level map: 0 = air, 1 = solid ground, 2 = flagpole (win condition)
// This map is now 11 rows high and 20 columns wide to fit the screen
int level[LEVEL_HEIGHT][LEVEL_WIDTH] = {
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // Row 0 (Y)
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // Row 1 (Clouds - 1 for cloud tile)
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // Row 2
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, // Row 3
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,0,1}, // Row 4 (Cloud)
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,1,0,1}, // Row 5
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1}, // Row 6
    {1,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,1,0,1}, // Row 7E
    {1,0,0,0,0,1,1,0,0,0,0,0,1,1,1,0,1,1,1,1}, // Row 8 (Platforms/Goal - Correct from image)
    {1,1,1,0,0,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1}, // Row 9 (Now ALL Solid Ground)
    {1,1,1,3,3,1,1,1,1,1,1,3,3,3,3,1,1,1,1,1}  // Row 10 (Still ALL Solid Ground)
};

// ============================================================================
// PHYSICS CONSTANTS
// ============================================================================
const float GRAVITY = 0.5f; 		// Downward acceleration per frame
const float FRICTION = 0.9f; 		// Horizontal velocity multiplier
const float MOVE_ACCELERATION = 1.0f; // Horizontal acceleration when moving
const float JUMP_IMPULSE = -15.0f; 	// Upward velocity when jumping
const float MAX_VELOCITY_X = 10.0f; 	// Maximum horizontal speed
const float MAX_VELOCITY_Y = 20.0f; 	// Maximum vertical speed

// ============================================================================
// INPUT STATE
// ============================================================================
bool keyLeft = false; 	// A key
bool keyRight = false; 	// D key
bool keyJump = false; 	// W key

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
void bus_read(teenyat *t, tny_uword addr, tny_word *data, uint16_t *delay);
void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay);

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
int main(int argc, char *argv[]) { 
	// --- Terminal Initialization ---
	saveDefaultColor();
	setConsoleTitle("TeenyAT Color Terminal");
	cls();
	setvbuf(stdout, NULL, _IONBF, 0);

	// --- Validate command-line arguments ---
	if (argc < 2) {
		cerr << "Error: No TeenyAT binary file specified." << endl;
		cerr << "Usage: " << argv[0] << " <binary_file.bin>" << endl;
		return EXIT_FAILURE;
	}

	// --- Load TeenyAT Binary ---
	FILE *bin_file = fopen(argv[1], "rb");
	if (!bin_file) {
		cerr << "Error: Could not open file '" << argv[1] << "'" << endl;
		return EXIT_FAILURE;
	}
	
	teenyat t;
	tny_init_from_file(&t, bin_file, bus_read, bus_write);
	fclose(bin_file);
    
	// Optional: allow overriding the number of VM steps per frame for debugging
	int vm_steps = 1000;
	if (argc >= 3) {
		vm_steps = atoi(argv[2]);
		printf("INFO: vm_steps set to %d (from argv)\n", vm_steps);
	}
	
	// --- Initialize Graphics Window ---
	screen = tigrWindow(SCREEN_WIDTH_PIXELS, SCREEN_HEIGHT_PIXELS, "TeenyAT Game Engine", 0);
	if (!screen) {
		cerr << "Error: Could not create graphics window" << endl;
		return EXIT_FAILURE;
	}

	// ============================================================================
	// GAME LOOP
	// ============================================================================
	float lastTime = tigrTime();
	
	while (!tigrClosed(screen)) {
		// --- Calculate Delta Time ---
		float currentTime = tigrTime();
		float deltaTime = currentTime - lastTime;
		lastTime = currentTime;
		
		// Limit delta time to prevent huge jumps
		if (deltaTime > 0.1f) deltaTime = 0.1f;
		if (deltaTime < 0.0f) deltaTime = 0.0f; // guard against negative readings

		// Use a fixed physics timestep (frame-based) to avoid small noisy deltaTime
		// causing jitter when mixing frame-based constants.
		const float physDT = 1.0f; // treat velocities/accelerations as per-frame units

		// Simple debug counter to periodically print player state
		static int debugCounter = 0;
		debugCounter++;
		if ((debugCounter % 30) == 0) {
			// Print to console so you can see how the physics values change
			printf("DEBUG: dt=%.4f pos=(%.2f,%.2f) vel=(%.2f,%.2f) onGround=%d tile=(%d,%d) keys=L:%d R:%d J:%d\\n",
				deltaTime, player.x, player.y, player.velocityX, player.velocityY,
				player.onGround ? 1 : 0, player.TileX, player.TileY,
				keyLeft ? 1 : 0, keyRight ? 1 : 0, keyJump ? 1 : 0);
		}
		
		// ========================================================================
		// 1. HANDLE EVENTS (Input)
		// ========================================================================
		keyLeft = tigrKeyHeld(screen, 'A');
		keyRight = tigrKeyHeld(screen, 'D');
		keyJump = tigrKeyDown(screen, 'W');
		
		// ========================================================================
		// 2. CLOCK THE TEENYAT CPU
		// ========================================================================
		for (int i = 0; i < vm_steps; i++) {
			tny_clock(&t);
		}

		// Fallback direct input control (keyboard)
		// If the loaded TeenyAT program does not issue movement writes, this
		// allows testing physics and input directly from the keyboard.
		if (keyLeft)  player.velocityX -= MOVE_ACCELERATION;
		if (keyRight) player.velocityX += MOVE_ACCELERATION;
		if (keyJump && player.onGround) {
			player.velocityY = JUMP_IMPULSE;
			player.onGround = false;
		}
		
		// ========================================================================
		// 3. UPDATE GAME STATE (Physics)
		// ========================================================================
		
		// Apply gravity (frame-based)
		player.velocityY += GRAVITY * physDT;

		// Apply friction to horizontal movement (frame-based)
		player.velocityX *= FRICTION;

		// Clamp velocities to maximum values
		if (player.velocityX > MAX_VELOCITY_X) player.velocityX = MAX_VELOCITY_X;
		if (player.velocityX < -MAX_VELOCITY_X) player.velocityX = -MAX_VELOCITY_X;
		if (player.velocityY > MAX_VELOCITY_Y) player.velocityY = MAX_VELOCITY_Y;

		// Calculate new position (frame-based)
		float newX = player.x + player.velocityX * physDT;
		float newY = player.y + player.velocityY * physDT;
		
		// Collision detection with level tiles
		player.onGround = false;
		
		// Check horizontal collision
		int tileXLeft = (int)(newX / TILE_SIZE);
		int tileXRight = (int)((newX + 15) / TILE_SIZE);
		int tileYTop = (int)(player.y / TILE_SIZE);
		int tileYBottom = (int)((player.y + 15) / TILE_SIZE);
		
		bool horizontalCollision = false;
		if (tileXLeft >= 0 && tileXLeft < LEVEL_WIDTH && tileXRight >= 0 && tileXRight < LEVEL_WIDTH) {
			for (int ty = tileYTop; ty <= tileYBottom; ty++) {
				if (ty >= 0 && ty < LEVEL_HEIGHT) {
					if (level[ty][tileXLeft] == 1 || level[ty][tileXRight] == 1) {
						horizontalCollision = true;
						break;
					}
				}
			}
		}
		
		if (!horizontalCollision) {
			player.x = newX;
		} else {
			// Resolve horizontal collision by snapping to tile edge to avoid small jitter
			if (player.velocityX > 0) {
				// moving right, snap to left side of blocking tile
				int collideTile = tileXRight;
				player.x = collideTile * TILE_SIZE - 16; // 16 = player width
			} else if (player.velocityX < 0) {
				// moving left, snap to right side of blocking tile
				int collideTile = tileXLeft;
				player.x = (collideTile + 1) * TILE_SIZE;
			}
			player.velocityX = 0;
			printf("DEBUG: HCOLLIDE snapX=%.2f velX=%.2f\n", player.x, player.velocityX);
		}
		
		// Check vertical collision
		tileXLeft = (int)(player.x / TILE_SIZE);
		tileXRight = (int)((player.x + 15) / TILE_SIZE);
		int tileYNewTop = (int)(newY / TILE_SIZE);
		int tileYNewBottom = (int)((newY + 15) / TILE_SIZE);
		
		bool verticalCollision = false;
		if (tileYNewTop >= 0 && tileYNewTop < LEVEL_HEIGHT && tileYNewBottom >= 0 && tileYNewBottom < LEVEL_HEIGHT) {
			for (int tx = tileXLeft; tx <= tileXRight; tx++) {
				if (tx >= 0 && tx < LEVEL_WIDTH) {
					if (level[tileYNewTop][tx] == 1 || level[tileYNewBottom][tx] == 1) {
						verticalCollision = true;
						if (player.velocityY > 0) {
							player.onGround = true;
						}
						break;
					}
				}
			}
		}
		
		if (!verticalCollision) {
			player.y = newY;
		} else {
			// Resolve vertical collision by snapping to tile edge
			if (player.velocityY > 0) {
				// falling, snap to top of blocking tile
				int collideTileY = tileYNewBottom;
				player.y = collideTileY * TILE_SIZE - 16; // place on top
				player.onGround = true;
			} else if (player.velocityY < 0) {
				// moving up, snap to bottom of blocking tile
				int collideTileY = tileYNewTop;
				player.y = (collideTileY + 1) * TILE_SIZE;
			}
			player.velocityY = 0;
			printf("DEBUG: VCOLLIDE snapY=%.2f velY=%.2f onGround=%d\n", player.y, player.velocityY, player.onGround ? 1 : 0);
		}
		
		// Update player tile position for win condition checking
		player.TileX = (int)((player.x + 8) / TILE_SIZE);
		player.TileY = (int)((player.y + 8) / TILE_SIZE);
		
		// Check win condition
		if (player.TileX >= 0 && player.TileX < LEVEL_WIDTH && 
			player.TileY >= 0 && player.TileY < LEVEL_HEIGHT) {
			if (level[player.TileY][player.TileX] == 2) {
				// Player reached the win tile: print a message, close the window and exit.
				tigrPrint(screen, tfont, 250, 150, tigrRGB(255, 255, 0), "YOU WIN!");
				// Ensure frame is presented so the player sees the message briefly
				tigrUpdate(screen);
				// Cleanup and exit immediately
				tigrFree(screen);
				resetColor();
				cls();
				return EXIT_SUCCESS;
			}

			if (level[player.TileY][player.TileX] == 3) {
				// Respawn the player back to the start
				player.x = RESPAWN_X;
				player.y = RESPAWN_Y;
				player.velocityX = 0.0f;
				player.velocityY = 0.0f;
				player.onGround = false;
				// Optionally print a death message for a moment
				tigrPrint(screen, tfont, 250, 180, tigrRGB(255, 0, 0), "PIT DEATH!");
			}
		}
					
		
		// ========================================================================
		// 4. RENDER GRAPHICS
		// ========================================================================
		
		// Clear screen
		tigrClear(screen, tigrRGB(135, 206, 235)); 	// Sky blue
		
		// Draw level tiles
		for (int y = 0; y < LEVEL_HEIGHT; y++) {
			for (int x = 0; x < LEVEL_WIDTH; x++) {
				if (level[y][x] == 1) {
					tigrFillRect(screen, x * TILE_SIZE, y * TILE_SIZE, 
								 TILE_SIZE, TILE_SIZE, tigrRGB(139, 69, 19));
				} else if (level[y][x] == 2) {
					tigrFillRect(screen, x * TILE_SIZE, y * TILE_SIZE,
								 TILE_SIZE, TILE_SIZE, tigrRGB(255, 0, 0));
				}
			}
		}
		
		// Draw player sprite
		tigrFillRect(screen, (int)player.x, (int)player.y, 
					 16, 16, tigrRGB(200, 0, 200));
		
		// Present the frame
		tigrUpdate(screen);
	}

	// --- Cleanup ---
	tigrFree(screen);
	resetColor();
	cls();
	return EXIT_SUCCESS;
}

// ============================================================================
// BUS READ HANDLER
// ============================================================================
void bus_read(teenyat *t, tny_uword addr, tny_word *data, uint16_t *delay) {
	data->u = 0;
	switch(addr) {
		case KEY_INPUT:
			if (keyLeft) 	data->u |= (1 << 0);
			if (keyRight) data->u |= (1 << 1);
			if (keyJump) 	data->u |= (1 << 2);
			break;
		
		default:
			break;
	}
}

// ============================================================================
// BUS WRITE HANDLER
// ============================================================================
void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay) {
	// Debug logging policy:
	// - Always log movement writes (useful for debugging physics input).
	// - For terminal/animation addresses (0x9000..0x902F), only log when the
	//   value actually changes to reduce repetitive spam (e.g. repeated writes
	//   of 0x0000 from a VM routine).
	if (addr == MOVE_E || addr == MOVE_W || addr == MOVE_N) {
		printf("BUS_WRITE: addr=0x%04X data=0x%04X\\n", addr, data.u);
	} else if (addr >= 0x9000 && addr <= 0x902F) {
		// remember last value per terminal/animation register
		static tny_uword lastTerminal[0x30];
		static bool termInit = false;
		if (!termInit) {
			for (int i = 0; i < 0x30; ++i) lastTerminal[i] = 0xFFFF; // sentinel
			termInit = true;
		}
		int idx = addr - 0x9000;
		if (lastTerminal[idx] != data.u) {
			printf("BUS_WRITE: addr=0x%04X data=0x%04X\\n", addr, data.u);
			lastTerminal[idx] = data.u;
		}
	}
	switch(addr) {
    // Legacy terminal commands (harmless, but unused in graphics mode)
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
		x = (x + 1) % LEVEL_WIDTH;
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
		x = data.u % LEVEL_WIDTH;
		gotoxy(x, y);
		break;
	case SET_Y:
		y = data.u % LEVEL_HEIGHT;
		gotoxy(x, y);
		break;

	// Physics-Based Movement Commands
	case MOVE_E:
		player.velocityX += MOVE_ACCELERATION;
		break;
	case MOVE_W:
		player.velocityX -= MOVE_ACCELERATION;
		break;
	case MOVE_N:
		if (player.onGround) {
			player.velocityY = JUMP_IMPULSE;
			player.onGround = false;
		}
		break;

	// Animation Control
	case Animation_Stand: player.spriteIndex = 0; break;
	case Animation_MoveL: player.spriteIndex = 1; break;
	case Animation_MoveR: player.spriteIndex = 2; break;
	case Animation_UP:    player.spriteIndex = 3; break;
	}
}
