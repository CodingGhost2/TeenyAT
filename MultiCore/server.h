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
const int VISION_RADIUS_PIXELS = 2000; // Large enough to see across entire map
const int MAX_INBOX_SIZE = 8;
const int TAG_DISTANCE_TILES = 1;
const int TAG_DISTANCE_PIXELS = 30;

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

struct Point {
    int x, y;
};

struct Node {
    Point pos;
    Node* parent;
    float g, h, f;
};


struct ConnectedClient {
    teenyat vm;  // TeenyAT virtual machine for this client
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
    int lastMoveResult;  // 0 = failed, 1 = success (for collision feedback)
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

