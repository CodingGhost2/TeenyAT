#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <deque>
#include <unordered_map>
#include "teenyat.h"


// ============================================================================
// Game World Constants
// ============================================================================
const int TILE_SIZE = 32;
const int LEVEL_WIDTH_TILES = 40;
const int LEVEL_HEIGHT_TILES = 30;
const int SCREEN_WIDTH_PIXELS = LEVEL_WIDTH_TILES * TILE_SIZE;
const int SCREEN_HEIGHT_PIXELS = LEVEL_HEIGHT_TILES * TILE_SIZE;
const int NUM_CLIENTS = 20;
const int CYCLES_PER_FRAME = 1000;
const int VISION_RADIUS_PIXELS = 160;
const int MAX_INBOX_SIZE = 8;
const int TAG_DISTANCE_PIXELS = 32;

// ============================================================================
// Data Structures
// ============================================================================

enum ClientStateEnum {
    ACTIVE,
    FROZEN,
    IMMUNE,
    DISCONNECTED,
    YIELDED
};

struct Player {
    float x, y;
    int team;
    ClientStateEnum state;
    int freezeTimer;
    int immunityTimer;
};

struct VisionResult {
    int id;
    int team;
    int relX;
    int relY;
    int dist;
};

struct Message {
    int fromID;
    int toID;
    int type;
    int data;
};

struct ConnectedClient {
    teenyat vm;
    int clientID;
    int teamID;
    ClientStateEnum state;

    // MMIO state
    int mapQueryX, mapQueryY;
    int visionSelectIndex;
    int msgSendTo, msgSendType, msgSendData;
    int messageReadIndex;
    int tagTargetID;
    int lastTagResult;
    int lastMoveRequest; // For testing
    std::vector<VisionResult> visionResults;
};

struct ConnectedClient; // Forward declaration

// ============================================================================
// Function Declarations
// ============================================================================
void initializeWorld();
void processMovementRequest(ConnectedClient* client, int direction);
void performVisionScan(ConnectedClient* client, int numClientsToCheck);
void sendMessage(int fromID, int toID, int type, int data);
void handleTag(int tagger, int tagged);

#endif // SERVER_H
// ============================================================================
// MMIO Protocol (Client <-> Server Interface)
// ============================================================================


// === CLIENT IDENTITY ===
const tny_uword CLIENT_ID        = 0x9000;
const tny_uword CLIENT_TEAM      = 0x9001;
const tny_uword CLIENT_X         = 0x9002;
const tny_uword CLIENT_Y         = 0x9003;
const tny_uword CLIENT_STATE     = 0x9004;

// === WORLD STATE QUERIES ===
const tny_uword WORLD_TIME       = 0x9010;
const tny_uword WORLD_RED_SCORE  = 0x9011;
const tny_uword WORLD_BLUE_SCORE = 0x9012;

// === MAP QUERIES ===
const tny_uword MAP_QUERY_X      = 0x9020;
const tny_uword MAP_QUERY_Y      = 0x9021;
const tny_uword MAP_RESULT       = 0x9022;

// === VISION QUERIES ===
const tny_uword VISION_SCAN      = 0x9030;
const tny_uword VISION_COUNT     = 0x9031;
const tny_uword VISION_SELECT    = 0x9032;
const tny_uword VISION_ID        = 0x9033;
const tny_uword VISION_TEAM      = 0x9034;
const tny_uword VISION_X         = 0x9035;
const tny_uword VISION_Y         = 0x9036;
const tny_uword VISION_DIST      = 0x9037;

// === MOVEMENT COMMANDS ===
const tny_uword MOVE_REQUEST     = 0x9100;

// === INTERACTION COMMANDS ===
const tny_uword TAG_REQUEST      = 0x9110;
const tny_uword TAG_RESULT       = 0x9111;

// === MESSAGING ===
const tny_uword MSG_SEND_TO      = 0x9120;
const tny_uword MSG_SEND_TYPE    = 0x9121;
const tny_uword MSG_SEND_DATA    = 0x9122;
const tny_uword MSG_SEND_EXEC    = 0x9123;

const tny_uword MSG_INBOX_COUNT  = 0x9130;
const tny_uword MSG_READ_IDX     = 0x9131;
const tny_uword MSG_READ_FROM    = 0x9132;
const tny_uword MSG_READ_TYPE    = 0x9133;
const tny_uword MSG_READ_DATA    = 0x9134;
const tny_uword MSG_POP          = 0x9135;

// === CLIENT CONTROL ===
const tny_uword CLIENT_YIELD     = 0x9200;
