#define _USE_MATH_DEFINES  // For M_PI on Windows
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <unordered_map>
#include <algorithm>

extern "C" {
#include "teenyat.h"
}
#include "server.h"



#include "tigr.h"

// --- Globals ---
std::vector<ConnectedClient> clients;
Player players[NUM_CLIENTS];
int level[LEVEL_HEIGHT_TILES][LEVEL_WIDTH_TILES];
std::unordered_map<int, std::deque<Message>> clientInboxes;
int redScore = 0;
int blueScore = 0;
int redTotalScore = 0;   // Cumulative across rounds
int blueTotalScore = 0;  // Cumulative across rounds
int gameTimer = 1800; // 30 seconds * 60 FPS
int mapChangeTimer = 900; // 15 seconds * 60 FPS
int currentRound = 1;
const int MAX_ROUNDS = 5;
bool roundTransition = false;

// --- Pacman-Style Movement Constants ---
const float CHASE_SPEED = 2.5f;  // pixels per frame

// --- Forward Declarations ---
void renderGame(Tigr* screen);
void updateGame();
void checkTaggingEvents();
void updateDynamicMap();
ConnectedClient* findClientByVM(teenyat *t);
void bus_read(teenyat *t, tny_uword addr, tny_word *data, uint16_t *delay);
void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay);
void respawnPlayer(int clientID);
std::vector<Point> findPath(Point start, Point end);

#ifdef ENABLE_TESTS
#include "tests.h"
#endif

// ============================================================================
// Tactical AI System - War-style movement with different behaviors
// ============================================================================

// AI Behavior types
enum AIBehavior {
    AGGRESSOR,    // Direct chase, high aggression
    FLANKER,      // Approach from angles, avoid direct path
    DEFENDER,     // Stay near spawn, chase only when enemies close
    PACK_HUNTER   // Stay with teammates, attack together
};

// Assign behavior based on client ID (creates variety)
AIBehavior getAIBehavior(int clientID) {
    int role = clientID % 4;
    switch (role) {
        case 0: return AGGRESSOR;
        case 1: return FLANKER;
        case 2: return DEFENDER;
        default: return PACK_HUNTER;
    }
}

// Find the nearest enemy for a given player
int findNearestEnemy(int clientID) {
    int myTeam = players[clientID].team;
    float myX = players[clientID].x;
    float myY = players[clientID].y;

    int nearestID = -1;
    float nearestDist = 999999.0f;

    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (i == clientID) continue;
        if (players[i].team == myTeam) continue;
        if (players[i].state != ACTIVE) continue;

        float dx = players[i].x - myX;
        float dy = players[i].y - myY;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist < nearestDist) {
            nearestDist = dist;
            nearestID = i;
        }
    }

    return nearestID;
}

// Find nearest teammate
int findNearestTeammate(int clientID) {
    int myTeam = players[clientID].team;
    float myX = players[clientID].x;
    float myY = players[clientID].y;

    int nearestID = -1;
    float nearestDist = 999999.0f;

    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (i == clientID) continue;
        if (players[i].team != myTeam) continue;
        if (players[i].state != ACTIVE) continue;

        float dx = players[i].x - myX;
        float dy = players[i].y - myY;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist < nearestDist) {
            nearestDist = dist;
            nearestID = i;
        }
    }

    return nearestID;
}

// Count nearby enemies and allies
void countNearbyUnits(int clientID, float radius, int* enemies, int* allies) {
    *enemies = 0;
    *allies = 0;
    int myTeam = players[clientID].team;

    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (i == clientID) continue;
        if (players[i].state != ACTIVE) continue;

        float dx = players[i].x - players[clientID].x;
        float dy = players[i].y - players[clientID].y;
        float dist = sqrt(dx * dx + dy * dy);

        if (dist < radius) {
            if (players[i].team == myTeam) (*allies)++;
            else (*enemies)++;
        }
    }
}

// Check if a position is walkable
bool isWalkable(float x, float y) {
    int tileX = (int)(x / TILE_SIZE);
    int tileY = (int)(y / TILE_SIZE);

    if (tileX < 1 || tileX >= LEVEL_WIDTH_TILES - 1 ||
        tileY < 1 || tileY >= LEVEL_HEIGHT_TILES - 1) {
        return false;
    }
    return level[tileY][tileX] != 1;
}

// Stuck detection - track recent positions
struct StuckTracker {
    float lastX[10];
    float lastY[10];
    int index;
    int stuckCounter;
    float wallFollowAngle;  // Angle to follow when wall-following
    bool isWallFollowing;

    StuckTracker() : index(0), stuckCounter(0), wallFollowAngle(0), isWallFollowing(false) {
        for (int i = 0; i < 10; i++) { lastX[i] = 0; lastY[i] = 0; }
    }
};
std::vector<StuckTracker> stuckTrackers;

// Check if unit is stuck (hasn't moved much recently)
bool isStuck(int clientID) {
    StuckTracker& tracker = stuckTrackers[clientID];
    float currentX = players[clientID].x;
    float currentY = players[clientID].y;

    // Calculate average distance from recent positions
    float totalDist = 0;
    for (int i = 0; i < 10; i++) {
        float dx = currentX - tracker.lastX[i];
        float dy = currentY - tracker.lastY[i];
        totalDist += sqrt(dx*dx + dy*dy);
    }

    // Update history
    tracker.lastX[tracker.index] = currentX;
    tracker.lastY[tracker.index] = currentY;
    tracker.index = (tracker.index + 1) % 10;

    // If average movement is very small, we're stuck
    return (totalDist / 10.0f) < 2.0f;
}

// Try to move with smart wall avoidance
bool tryMoveWithAvoidance(int clientID, float desiredAngle, float speed, float* outX, float* outY) {
    float myX = players[clientID].x;
    float myY = players[clientID].y;
    StuckTracker& tracker = stuckTrackers[clientID];

    // If wall following, continue in that direction
    if (tracker.isWallFollowing) {
        float testX = myX + speed * cos(tracker.wallFollowAngle);
        float testY = myY + speed * sin(tracker.wallFollowAngle);

        if (isWalkable(testX, testY)) {
            // Check if we can now go towards target
            float targetX = myX + speed * cos(desiredAngle);
            float targetY = myY + speed * sin(desiredAngle);
            if (isWalkable(targetX, targetY)) {
                tracker.isWallFollowing = false;
                *outX = targetX;
                *outY = targetY;
                return true;
            }
            *outX = testX;
            *outY = testY;
            return true;
        } else {
            // Wall follow direction is also blocked, try rotating
            tracker.wallFollowAngle += M_PI / 4;
            tracker.stuckCounter++;
            if (tracker.stuckCounter > 8) {
                tracker.isWallFollowing = false;
                tracker.stuckCounter = 0;
            }
        }
    }

    // Try the desired direction first
    float testX = myX + speed * cos(desiredAngle);
    float testY = myY + speed * sin(desiredAngle);

    if (isWalkable(testX, testY)) {
        *outX = testX;
        *outY = testY;
        return true;
    }

    // Desired direction blocked - try alternative angles
    // Try increasingly wider angles to find a path around
    float angleOffsets[] = {
        M_PI/6,   -M_PI/6,    // 30 degrees
        M_PI/4,   -M_PI/4,    // 45 degrees
        M_PI/3,   -M_PI/3,    // 60 degrees
        M_PI/2,   -M_PI/2,    // 90 degrees
        2*M_PI/3, -2*M_PI/3,  // 120 degrees
        3*M_PI/4, -3*M_PI/4,  // 135 degrees
    };

    for (int i = 0; i < 12; i++) {
        float tryAngle = desiredAngle + angleOffsets[i];
        testX = myX + speed * cos(tryAngle);
        testY = myY + speed * sin(tryAngle);

        if (isWalkable(testX, testY)) {
            // Found a way around - use this direction
            *outX = testX;
            *outY = testY;

            // If we had to turn a lot, start wall following
            if (i >= 6) {  // Turned more than 90 degrees
                tracker.isWallFollowing = true;
                tracker.wallFollowAngle = tryAngle;
                tracker.stuckCounter = 0;
            }
            return true;
        }
    }

    // Completely stuck - try random direction
    for (int attempt = 0; attempt < 8; attempt++) {
        float randomAngle = (rand() % 360) * M_PI / 180.0f;
        testX = myX + speed * 0.5f * cos(randomAngle);
        testY = myY + speed * 0.5f * sin(randomAngle);

        if (isWalkable(testX, testY)) {
            *outX = testX;
            *outY = testY;
            tracker.isWallFollowing = true;
            tracker.wallFollowAngle = randomAngle;
            return true;
        }
    }

    return false;
}

// Get spawn point for retreat calculation
void getSpawnCenter(int team, float* x, float* y) {
    if (team == 0) {  // Red team
        *x = 4 * TILE_SIZE;
        *y = 3 * TILE_SIZE;
    } else {  // Blue team
        *x = 36 * TILE_SIZE;
        *y = 27 * TILE_SIZE;
    }
}

// Main tactical movement function
void tacticalMove(int clientID, float baseSpeed) {
    if (players[clientID].state != ACTIVE) return;

    AIBehavior behavior = getAIBehavior(clientID);
    int myTeam = players[clientID].team;
    float myX = players[clientID].x;
    float myY = players[clientID].y;

    int targetID = findNearestEnemy(clientID);
    int nearbyEnemies, nearbyAllies;
    countNearbyUnits(clientID, 150.0f, &nearbyEnemies, &nearbyAllies);

    float angle = 0;
    float speed = baseSpeed;
    bool shouldMove = true;
    const char* actionStr = "idle";

    // === BEHAVIOR-SPECIFIC LOGIC ===

    if (targetID == -1) {
        // No enemies visible - patrol towards center or wander
        float centerX = SCREEN_WIDTH_PIXELS / 2.0f;
        float centerY = SCREEN_HEIGHT_PIXELS / 2.0f;
        angle = atan2(centerY - myY, centerX - myX);
        angle += ((rand() % 60) - 30) * M_PI / 180.0f;  // Add randomness
        speed *= 0.7f;
        actionStr = "patrol";
    }
    else {
        float dx = players[targetID].x - myX;
        float dy = players[targetID].y - myY;
        float dist = sqrt(dx * dx + dy * dy);

        // Very close - don't move, let tag happen
        if (dist < 8.0f) {
            shouldMove = false;
            actionStr = "tagging";
        }
        else {
            switch (behavior) {
                case AGGRESSOR:
                    // Direct pursuit, slightly faster
                    angle = atan2(dy, dx);
                    speed *= 1.1f;
                    actionStr = "aggro";

                    // Retreat if heavily outnumbered
                    if (nearbyEnemies > nearbyAllies + 2) {
                        angle += M_PI;  // Run away
                        actionStr = "retreat";
                    }
                    break;

                case FLANKER:
                    // Approach at an angle, try to get behind
                    angle = atan2(dy, dx);
                    if (dist > 100) {
                        // Add perpendicular component for flanking
                        float flankDir = (clientID % 2 == 0) ? M_PI/3 : -M_PI/3;
                        angle += flankDir;
                        actionStr = "flank";
                    } else {
                        actionStr = "close-in";
                    }
                    break;

                case DEFENDER:
                    {
                        float spawnX, spawnY;
                        getSpawnCenter(myTeam, &spawnX, &spawnY);
                        float distToSpawn = sqrt(pow(myX - spawnX, 2) + pow(myY - spawnY, 2));

                        if (dist < 200 || distToSpawn < 300) {
                            // Enemy close or we're near spawn - attack!
                            angle = atan2(dy, dx);
                            actionStr = "defend-attack";
                        } else {
                            // Return towards spawn area
                            angle = atan2(spawnY - myY, spawnX - myX);
                            angle += ((rand() % 40) - 20) * M_PI / 180.0f;
                            speed *= 0.8f;
                            actionStr = "defend-hold";
                        }
                    }
                    break;

                case PACK_HUNTER:
                    {
                        int teammateID = findNearestTeammate(clientID);
                        if (teammateID != -1) {
                            float tdx = players[teammateID].x - myX;
                            float tdy = players[teammateID].y - myY;
                            float teammateDist = sqrt(tdx * tdx + tdy * tdy);

                            if (teammateDist > 80) {
                                // Too far from teammate - move towards them
                                angle = atan2(tdy, tdx);
                                actionStr = "regroup";
                            } else if (nearbyAllies >= 1) {
                                // Have backup - attack together!
                                angle = atan2(dy, dx);
                                speed *= 1.05f;
                                actionStr = "pack-attack";
                            } else {
                                // Wait for backup
                                angle = atan2(tdy, tdx);
                                speed *= 0.5f;
                                actionStr = "wait-pack";
                            }
                        } else {
                            // No teammates nearby - chase anyway
                            angle = atan2(dy, dx);
                            actionStr = "solo";
                        }
                    }
                    break;
            }
        }
    }

    if (!shouldMove) return;

    // Check if stuck and need to change strategy
    if (isStuck(clientID)) {
        // Force a random direction change to get unstuck
        angle += (rand() % 2 == 0 ? 1 : -1) * M_PI / 2;
        actionStr = "unstuck";
    }

    // Try to move with smart wall avoidance
    float newX, newY;
    if (tryMoveWithAvoidance(clientID, angle, speed, &newX, &newY)) {
        // Check player collision
        bool tooClose = false;
        for (int i = 0; i < NUM_CLIENTS; i++) {
            if (i == clientID) continue;
            float pdx = newX - players[i].x;
            float pdy = newY - players[i].y;
            float pdist = sqrt(pdx * pdx + pdy * pdy);
            if (pdist < 12.0f) {
                tooClose = true;
                // Try to go around the other player
                float avoidAngle = atan2(pdy, pdx) + M_PI/2;
                newX = players[clientID].x + speed * 0.5f * cos(avoidAngle);
                newY = players[clientID].y + speed * 0.5f * sin(avoidAngle);
                if (!isWalkable(newX, newY)) {
                    avoidAngle -= M_PI;  // Try other side
                    newX = players[clientID].x + speed * 0.5f * cos(avoidAngle);
                    newY = players[clientID].y + speed * 0.5f * sin(avoidAngle);
                }
                break;
            }
        }

        if (isWalkable(newX, newY)) {
            players[clientID].x = newX;
            players[clientID].y = newY;
        }
    }

    // Debug output (reduced frequency)
    static int frameCount = 0;
    if (frameCount++ % 60 == 0 && clientID < 4) {
        const char* behaviorStr[] = {"AGGRO", "FLANK", "DEFEND", "PACK"};
        StuckTracker& tracker = stuckTrackers[clientID];
        printf("[%s] Client %d (%s): %s %s\n",
               (myTeam == 0) ? "RED" : "BLU",
               clientID, behaviorStr[behavior], actionStr,
               tracker.isWallFollowing ? "[wall-follow]" : "");
    }
}

// ============================================================================
// Game Logic
// ============================================================================

bool canReachOpenSpace(int startX, int startY) {
    // This is a simplified check. A full flood fill would be more robust.
    // For now, just check immediate neighbors.
    int tileX = startX / TILE_SIZE;
    int tileY = startY / TILE_SIZE;
    if (tileX > 0 && level[tileY][tileX-1] == 0) return true;
    if (tileX < LEVEL_WIDTH_TILES - 1 && level[tileY][tileX+1] == 0) return true;
    if (tileY > 0 && level[tileY-1][tileX] == 0) return true;
    if (tileY < LEVEL_HEIGHT_TILES - 1 && level[tileY+1][tileX] == 0) return true;
    return false;
}

bool validateMapConnectivity() {
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (!canReachOpenSpace((int)players[i].x, (int)players[i].y)) {
            return false; // This change traps a client
        }
    }
    return true;
}

void updateDynamicMap() {
    mapChangeTimer--;
    if (mapChangeTimer <= 0) {
        mapChangeTimer = 900; // Reset timer

        // Try to flip a tile state up to 10 times
        for (int attempt = 0; attempt < 10; attempt++) {
            int x = rand() % (LEVEL_WIDTH_TILES - 2) + 1; // Don't change border walls
            int y = rand() % (LEVEL_HEIGHT_TILES - 2) + 1;
            
            int oldState = level[y][x];
            if (oldState == 2) continue; // Don't change goal tiles if they exist

            level[y][x] = (oldState == 0) ? 1 : 0; // Flip it
            
            // Ensure no client is trapped
            if (validateMapConnectivity()) {
                break; // Change accepted
            } else {
                level[y][x] = oldState; // Revert change
            }
        }
    }
}

// ============================================================================
// MAP DEFINITIONS - 4 Different Battlefields
// ============================================================================

const char* mapNames[] = {"ARENA", "CORRIDORS", "FORTRESS", "CROSSFIRE"};
int currentMapIndex = 0;

void clearMap() {
    // Reset to border walls only
    for (int y = 0; y < LEVEL_HEIGHT_TILES; y++) {
        for (int x = 0; x < LEVEL_WIDTH_TILES; x++) {
            if (x == 0 || x == LEVEL_WIDTH_TILES - 1 || y == 0 || y == LEVEL_HEIGHT_TILES - 1) {
                level[y][x] = 1; // Border walls
            } else {
                level[y][x] = 0; // Open space
            }
        }
    }
}

void generateMap_Arena() {
    // MAP 1: ARENA - Very open with small scattered cover
    clearMap();

    // Small corner covers (not blocking paths)
    level[4][4] = 1; level[4][5] = 1;
    level[4][34] = 1; level[4][35] = 1;
    level[25][4] = 1; level[25][5] = 1;
    level[25][34] = 1; level[25][35] = 1;

    // Small center pillars (with gaps between them)
    level[13][18] = 1; level[14][18] = 1;
    level[13][21] = 1; level[14][21] = 1;
    level[15][18] = 1; level[16][18] = 1;
    level[15][21] = 1; level[16][21] = 1;

    // Scattered small obstacles (single tiles for cover)
    level[10][10] = 1;
    level[19][10] = 1;
    level[10][29] = 1;
    level[19][29] = 1;
    level[14][8] = 1;
    level[14][31] = 1;
}

void generateMap_Corridors() {
    // MAP 2: CORRIDORS - Hallways with MANY openings
    clearMap();

    // Horizontal corridors with multiple gaps
    for (int x = 5; x < 35; x++) {
        // Many gaps for passage: at 8, 12, 16, 20, 24, 28, 32
        if (x % 4 != 0) {
            level[10][x] = 1;
            level[19][x] = 1;
        }
    }

    // Vertical corridors with multiple gaps
    for (int y = 5; y < 25; y++) {
        // Many gaps for passage: at 8, 12, 16, 20
        if (y % 4 != 0) {
            level[y][13] = 1;
            level[y][26] = 1;
        }
    }

    // Small center cover (not blocking)
    level[14][19] = 1;
    level[15][20] = 1;
}

void generateMap_Fortress() {
    // MAP 3: FORTRESS - Defensive structures with WIDE gates
    clearMap();

    // RED TEAM FORTRESS (top) - shorter walls with big gaps
    for (int x = 10; x < 14; x++) level[7][x] = 1;  // Left section
    for (int x = 26; x < 30; x++) level[7][x] = 1;  // Right section
    // Wide open center (columns 14-26 are all open!)

    // BLUE TEAM FORTRESS (bottom) - same pattern
    for (int x = 10; x < 14; x++) level[22][x] = 1;
    for (int x = 26; x < 30; x++) level[22][x] = 1;

    // Small scattered cover in no-man's land
    level[14][15] = 1;
    level[15][24] = 1;
    level[12][20] = 1;
    level[17][19] = 1;

    // Side pillars (single tiles, not walls)
    level[10][5] = 1;
    level[19][5] = 1;
    level[10][34] = 1;
    level[19][34] = 1;
}

void generateMap_Crossfire() {
    // MAP 4: CROSSFIRE - Short diagonal segments with big gaps
    clearMap();

    // Short diagonal barriers (top-left) - only 3 tiles each with gaps
    for (int i = 0; i < 3; i++) {
        level[8 + i][10 + i] = 1;
    }

    // Short diagonal barriers (top-right)
    for (int i = 0; i < 3; i++) {
        level[8 + i][29 - i] = 1;
    }

    // Short diagonal barriers (bottom-left)
    for (int i = 0; i < 3; i++) {
        level[20 - i][10 + i] = 1;
    }

    // Short diagonal barriers (bottom-right)
    for (int i = 0; i < 3; i++) {
        level[20 - i][29 - i] = 1;
    }

    // Small center cover
    level[14][19] = 1;
    level[15][20] = 1;

    // Tiny corner markers
    level[4][4] = 1;
    level[4][35] = 1;
    level[25][4] = 1;
    level[25][35] = 1;
}

void loadMap(int mapIndex) {
    currentMapIndex = mapIndex % 4;

    switch (currentMapIndex) {
        case 0: generateMap_Arena(); break;
        case 1: generateMap_Corridors(); break;
        case 2: generateMap_Fortress(); break;
        case 3: generateMap_Crossfire(); break;
    }

    printf("\n>>> MAP LOADED: %s <<<\n\n", mapNames[currentMapIndex]);
}

void loadRandomMap() {
    loadMap(rand() % 4);
}

void initializeWorld() {
    // Load a random map
    loadRandomMap();

    // Initialize players scattered across their team's half of the map
    for (int i = 0; i < NUM_CLIENTS; i++) {
        players[i].team = (i < NUM_CLIENTS / 2) ? 0 : 1;
        players[i].state = ACTIVE;
        players[i].freezeTimer = 0;
        players[i].immunityTimer = 0;

        int spawnTileX, spawnTileY;
        int attempts = 0;
        const int MAX_ATTEMPTS = 200;

        do {
            if (players[i].team == 0) { // Red Team - left half
                spawnTileX = 2 + (rand() % 16);   // columns 2-17
                spawnTileY = 2 + (rand() % 24);   // rows 2-25
            } else { // Blue Team - right half
                spawnTileX = 22 + (rand() % 16);  // columns 22-37
                spawnTileY = 2 + (rand() % 24);   // rows 2-25
            }
            attempts++;

            // Check wall and player collision
            if (level[spawnTileY][spawnTileX] == 0) {
                float newX = spawnTileX * TILE_SIZE + 16;
                float newY = spawnTileY * TILE_SIZE + 16;

                // Check distance from all already-spawned players
                bool tooClose = false;
                for (int j = 0; j < i; j++) {
                    float dx = newX - players[j].x;
                    float dy = newY - players[j].y;
                    float dist = sqrt(dx*dx + dy*dy);
                    if (dist < 64.0f) {  // Minimum 2 tiles apart
                        tooClose = true;
                        break;
                    }
                }
                if (!tooClose) break;
            }
        } while (attempts < MAX_ATTEMPTS);

        players[i].x = spawnTileX * TILE_SIZE + 16;
        players[i].y = spawnTileY * TILE_SIZE + 16;
    }
}



// Helper: Check if a tile position is walkable (no wall)
bool isTileWalkable(int tileX, int tileY) {
    if (tileX < 1 || tileX >= LEVEL_WIDTH_TILES - 1 ||
        tileY < 1 || tileY >= LEVEL_HEIGHT_TILES - 1) {
        return false;
    }
    return level[tileY][tileX] != 1;
}

// Helper: Check if a pixel position is valid (no wall, in bounds)
bool isPositionClear(float x, float y) {
    // Check the corners of the player's bounding box
    int margin = 6; // pixels from center to check
    int tileX1 = (int)((x - margin) / TILE_SIZE);
    int tileY1 = (int)((y - margin) / TILE_SIZE);
    int tileX2 = (int)((x + margin) / TILE_SIZE);
    int tileY2 = (int)((y + margin) / TILE_SIZE);

    return isTileWalkable(tileX1, tileY1) && isTileWalkable(tileX2, tileY1) &&
           isTileWalkable(tileX1, tileY2) && isTileWalkable(tileX2, tileY2);
}

// Helper: Check player collision
// Only blocks movement for teammates - enemies can overlap for tagging
bool hasPlayerCollision(int id, float x, float y) {
    int myTeam = players[id].team;
    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (i == id) continue;
        if (players[i].state == FROZEN || players[i].state == DISCONNECTED) continue;

        float dx_p = x - players[i].x;
        float dy_p = y - players[i].y;
        float dist = sqrt(dx_p*dx_p + dy_p*dy_p);

        // Teammates block each other to prevent stacking
        // Enemies can get very close (for tagging)
        float blockDist = (players[i].team == myTeam) ? 12.0f : 6.0f;
        if (dist < blockDist) {
            return true;
        }
    }
    return false;
}

// Helper: Full validity check
bool isValidPosition(int id, float x, float y) {
    return isPositionClear(x, y) && !hasPlayerCollision(id, x, y);
}

void processMovementRequest(ConnectedClient* client, int direction) {
    int id = client->clientID;
    client->lastMoveResult = 0;

    if (players[id].state == FROZEN) {
        return;
    }

    const float MOVE_SPEED = 2.5f;
    direction = direction % 8;

    float myX = players[id].x;
    float myY = players[id].y;

    // Direction vectors (E, SE, S, SW, W, NW, N, NE)
    const float dirX[] = {1, 0.707f, 0, -0.707f, -1, -0.707f, 0, 0.707f};
    const float dirY[] = {0, 0.707f, 1, 0.707f, 0, -0.707f, -1, -0.707f};

    // === PHASE 1: Try the exact requested direction ===
    float newX = myX + dirX[direction] * MOVE_SPEED;
    float newY = myY + dirY[direction] * MOVE_SPEED;

    if (isValidPosition(id, newX, newY)) {
        players[id].x = newX;
        players[id].y = newY;
        client->lastMoveResult = 1;
        return;
    }

    // === PHASE 2: Try adjacent directions (wall sliding) ===
    int leftDir = (direction + 7) % 8;  // Counter-clockwise
    int rightDir = (direction + 1) % 8; // Clockwise

    // Try sliding along walls
    newX = myX + dirX[leftDir] * MOVE_SPEED;
    newY = myY + dirY[leftDir] * MOVE_SPEED;
    if (isValidPosition(id, newX, newY)) {
        players[id].x = newX;
        players[id].y = newY;
        client->lastMoveResult = 1;
        return;
    }

    newX = myX + dirX[rightDir] * MOVE_SPEED;
    newY = myY + dirY[rightDir] * MOVE_SPEED;
    if (isValidPosition(id, newX, newY)) {
        players[id].x = newX;
        players[id].y = newY;
        client->lastMoveResult = 1;
        return;
    }

    // === PHASE 3: Try perpendicular directions ===
    int perpLeft = (direction + 6) % 8;
    int perpRight = (direction + 2) % 8;

    newX = myX + dirX[perpLeft] * MOVE_SPEED;
    newY = myY + dirY[perpLeft] * MOVE_SPEED;
    if (isValidPosition(id, newX, newY)) {
        players[id].x = newX;
        players[id].y = newY;
        client->lastMoveResult = 1;
        return;
    }

    newX = myX + dirX[perpRight] * MOVE_SPEED;
    newY = myY + dirY[perpRight] * MOVE_SPEED;
    if (isValidPosition(id, newX, newY)) {
        players[id].x = newX;
        players[id].y = newY;
        client->lastMoveResult = 1;
        return;
    }

    // === PHASE 4: Try any direction with full speed ===
    for (int d = 0; d < 8; d++) {
        newX = myX + dirX[d] * MOVE_SPEED;
        newY = myY + dirY[d] * MOVE_SPEED;
        if (isValidPosition(id, newX, newY)) {
            players[id].x = newX;
            players[id].y = newY;
            client->lastMoveResult = 1;
            return;
        }
    }

    // === PHASE 5: Try smaller steps in any direction ===
    for (int d = 0; d < 8; d++) {
        newX = myX + dirX[d] * MOVE_SPEED * 0.4f;
        newY = myY + dirY[d] * MOVE_SPEED * 0.4f;
        if (isValidPosition(id, newX, newY)) {
            players[id].x = newX;
            players[id].y = newY;
            client->lastMoveResult = 1;
            return;
        }
    }

    // === PHASE 6: Random jitter to escape stuck positions ===
    for (int attempt = 0; attempt < 16; attempt++) {
        float angle = (rand() % 360) * M_PI / 180.0f;
        float dist = 1.0f + (rand() % 20) / 10.0f;
        newX = myX + dist * cos(angle);
        newY = myY + dist * sin(angle);
        if (isValidPosition(id, newX, newY)) {
            players[id].x = newX;
            players[id].y = newY;
            client->lastMoveResult = 1;
            return;
        }
    }

    // Truly stuck - can't move at all
}

//Bresenham's Line Algorithm for Line of Sight (pixel-based)
bool hasLineOfSight(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        int tileX = x0 / TILE_SIZE;
        int tileY = y0 / TILE_SIZE;

        if (tileX >= 0 && tileX < LEVEL_WIDTH_TILES &&
            tileY >= 0 && tileY < LEVEL_HEIGHT_TILES &&
            level[tileY][tileX] == 1) {
            return false; // Wall in the way
        }

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
    return true;
}

void performVisionScan(ConnectedClient* client, int numClientsToCheck) {
    client->visionResults.clear();
    int clientX = (int)players[client->clientID].x;
    int clientY = (int)players[client->clientID].y;

    for (int i = 0; i < numClientsToCheck; i++) {
        if (i == client->clientID) continue; // Don't scan self
        if (players[i].state == DISCONNECTED) continue;

        int targetX = (int)players[i].x;
        int targetY = (int)players[i].y;

        float dist = sqrt(pow(targetX - clientX, 2) + pow(targetY - clientY, 2));
        if (dist > VISION_RADIUS_PIXELS) continue;

        if (hasLineOfSight(clientX, clientY, targetX, targetY)) {
            VisionResult vr;
            vr.id = i;
            vr.team = players[i].team;
            vr.relX = targetX - clientX;
            vr.relY = targetY - clientY;
            vr.dist = (int)(dist / TILE_SIZE);
            client->visionResults.push_back(vr);
        }
    }

    std::sort(client->visionResults.begin(), client->visionResults.end(),
              [](const VisionResult& a, const VisionResult& b) { return a.dist < b.dist; });
}

void sendMessage(int fromID, int toID, int type, int data) {
    if (fromID == toID) return;
    Message msg = {fromID, toID, type, data};
    if (clientInboxes[toID].size() < MAX_INBOX_SIZE) {
        clientInboxes[toID].push_back(msg);
    } else {
        clientInboxes[toID].pop_front();
        clientInboxes[toID].push_back(msg);
    }
}

void respawnPlayer(int clientID) {
    int team = players[clientID].team;

    int spawnTileX, spawnTileY;
    int attempts = 0;
    const int MAX_ATTEMPTS = 200;

    do {
        if (team == 0) { // Red team: spread across left side
            spawnTileX = 2 + (rand() % 10);  // columns 2-11
            spawnTileY = 2 + (rand() % 24);  // rows 2-25
        } else { // Blue team: spread across right side
            spawnTileX = 26 + (rand() % 10); // columns 26-35
            spawnTileY = 2 + (rand() % 24);  // rows 2-25
        }
        attempts++;

        // Check both wall collision and player collision
        if (level[spawnTileY][spawnTileX] == 0) {
            float newX = spawnTileX * TILE_SIZE + 16;
            float newY = spawnTileY * TILE_SIZE + 16;
            if (!hasPlayerCollision(clientID, newX, newY)) {
                break; // Valid position found
            }
        }
    } while (attempts < MAX_ATTEMPTS);

    players[clientID].x = spawnTileX * TILE_SIZE + 16;
    players[clientID].y = spawnTileY * TILE_SIZE + 16;
}

int getChaseClientMove(ConnectedClient* client) {
    performVisionScan(client, NUM_CLIENTS);
    
    // Find nearest enemy
    int nearestEnemyID = -1;
    float min_dist = 10000.0f;
    for (const auto& result : client->visionResults) {
        if (result.team != client->teamID) {
            if (result.dist < min_dist) {
                min_dist = result.dist;
                nearestEnemyID = result.id;
            }
        }
    }

    if (nearestEnemyID != -1) {
        Point start = {(int)(players[client->clientID].x / TILE_SIZE), (int)(players[client->clientID].y / TILE_SIZE)};
        Point end = {(int)(players[nearestEnemyID].x / TILE_SIZE), (int)(players[nearestEnemyID].y / TILE_SIZE)};
        printf("[AI PATH] Client %d (Team %d): Start (%d,%d), End (%d,%d)\n", client->clientID, client->teamID, start.x, start.y, end.x, end.y);
        std::vector<Point> path = findPath(start, end);
        if (path.size() > 1) {
            printf("[AI PATH] Client %d Path: ", client->clientID);
            for (const auto& p : path) {
                printf("(%d,%d) ", p.x, p.y);
            }
            printf("\n");

            float relX = (path[1].x * TILE_SIZE + 16) - players[client->clientID].x;
            float relY = (path[1].y * TILE_SIZE + 16) - players[client->clientID].y;
            printf("[AI MOVE] Client %d (Team %d): Next Step (%d,%d). RelX: %.2f, RelY: %.2f\n", client->clientID, client->teamID, path[1].x, path[1].y, relX, relY);
            if (std::abs(relX) > std::abs(relY)) {
                printf("[AI MOVE] Client %d chose direction: %d\n", client->clientID, (relX > 0) ? 0 : 4);
                return (relX > 0) ? 0 : 4; // East or West
            } else {
                printf("[AI MOVE] Client %d chose direction: %d\n", client->clientID, (relY > 0) ? 2 : 6);
                return (relY > 0) ? 2 : 6; // South or North
            }
        }
    }

    // No enemy in sight or no path, move randomly
    printf("[AI MOVE] Client %d (Team %d): No path, moving randomly. Direction: %d\n", client->clientID, client->teamID, rand() % 8);
    return rand() % 8;
}

int getTagClientMove(ConnectedClient* client) {
    performVisionScan(client, NUM_CLIENTS);

    // Find nearest enemy
    int nearestEnemyID = -1;
    float min_dist = 10000.0f;
    for (const auto& result : client->visionResults) {
        if (result.team != client->teamID) {
            if (result.dist < min_dist) {
                min_dist = result.dist;
                nearestEnemyID = result.id;
            }
        }
    }

    if (nearestEnemyID != -1) {
        Point start = {(int)(players[client->clientID].x / TILE_SIZE), (int)(players[client->clientID].y / TILE_SIZE)};
        Point end = {(int)(players[nearestEnemyID].x / TILE_SIZE), (int)(players[nearestEnemyID].y / TILE_SIZE)};
        printf("[AI PATH] Client %d (Team %d): Start (%d,%d), End (%d,%d)\n", client->clientID, client->teamID, start.x, start.y, end.x, end.y);
        std::vector<Point> path = findPath(start, end);
        if (path.size() > 1) {
            printf("[AI PATH] Client %d Path: ", client->clientID);
            for (const auto& p : path) {
                printf("(%d,%d) ", p.x, p.y);
            }
            printf("\n");
            
            float relX = (path[1].x * TILE_SIZE + 16) - players[client->clientID].x;
            float relY = (path[1].y * TILE_SIZE + 16) - players[client->clientID].y;
            printf("[AI MOVE] Client %d (Team %d): Next Step (%d,%d). RelX: %.2f, RelY: %.2f\n", client->clientID, client->teamID, path[1].x, path[1].y, relX, relY);
            if (std::abs(relX) > std::abs(relY)) {
                printf("[AI MOVE] Client %d chose direction: %d\n", client->clientID, (relX > 0) ? 0 : 4);
                return (relX > 0) ? 0 : 4; // East or West
            } else {
                printf("[AI MOVE] Client %d chose direction: %d\n", client->clientID, (relY > 0) ? 2 : 6);
                return (relY > 0) ? 2 : 6; // South or North
            }
        }
    }

    // No enemy in sight or no path, move North
    printf("[AI MOVE] Client %d (Team %d): No path, moving North. Direction: 6\n", client->clientID, client->teamID);
    return 6;
}

// A* Pathfinding Implementation
float calculate_h_value(Point src, Point dest) {
    return (float)sqrt(pow(src.x - dest.x, 2) + pow(src.y - dest.y, 2));
}

Node* get_node_in_list(std::vector<Node*>& list, Point p) {
    for (Node* n : list) {
        if (n->pos.x == p.x && n->pos.y == p.y) {
            return n;
        }
    }
    return nullptr;
}

std::vector<Point> reconstruct_path(Node* current) {
    std::vector<Point> path;
    while (current != nullptr) {
        path.push_back(current->pos);
        current = current->parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Point> findPath(Point start, Point end) {
    std::vector<Node*> openList;
    std::vector<Node*> closedList;
    
    Node* startNode = new Node({start, nullptr, 0.0f, 0.0f, 0.0f});
    startNode->h = calculate_h_value(start, end);
    startNode->f = startNode->g + startNode->h;
    openList.push_back(startNode);

    printf("[A* DEBUG] Starting path from (%d,%d) to (%d,%d)\n", start.x, start.y, end.x, end.y);

    while (!openList.empty()) {
        // Find node with lowest f value
        Node* currentNode = openList[0];
        int currentIndex = 0;
        for (size_t i = 1; i < openList.size(); i++) {
            if (openList[i]->f < currentNode->f) {
                currentNode = openList[i];
                currentIndex = i;
            }
        }

        openList.erase(openList.begin() + currentIndex);
        closedList.push_back(currentNode);

        printf("[A* DEBUG] Current Node: (%d,%d) g=%.2f h=%.2f f=%.2f\n", currentNode->pos.x, currentNode->pos.y, currentNode->g, currentNode->h, currentNode->f);

        // Goal reached
        if (currentNode->pos.x == end.x && currentNode->pos.y == end.y) {
            printf("[A* DEBUG] Goal reached at (%d,%d)!\n", end.x, end.y);
            std::vector<Point> path = reconstruct_path(currentNode);
            
            // Clean up memory
            for (Node* n : openList) delete n;
            for (Node* n : closedList) delete n;
            
            return path;
        }

        // Check all 8 directions
        const int dx[] = {1, 1, 0, -1, -1, -1, 0, 1};
        const int dy[] = {0, 1, 1, 1, 0, -1, -1, -1};
        
        for (int i = 0; i < 8; i++) {
            Point neighborPos = {currentNode->pos.x + dx[i], currentNode->pos.y + dy[i]};

            printf("[A* DEBUG]   Checking neighbor: (%d,%d) for current (%d,%d)\n", neighborPos.x, neighborPos.y, currentNode->pos.x, currentNode->pos.y);

            // Bounds check
            if (neighborPos.x < 0 || neighborPos.x >= LEVEL_WIDTH_TILES || 
                neighborPos.y < 0 || neighborPos.y >= LEVEL_HEIGHT_TILES) {
                printf("[A* DEBUG]     Neighbor (%d,%d) out of bounds. Skipping.\n", neighborPos.x, neighborPos.y);
                continue;
            }
            
            // Wall check
            if (level[neighborPos.y][neighborPos.x] == 1) {
                printf("[A* DEBUG]     Neighbor (%d,%d) is a wall. Skipping.\n", neighborPos.x, neighborPos.y);
                continue;
            }
            
            // Diagonal movement check - ensure both adjacent cells are passable
            if (i % 2 == 1) { // Diagonal move (indices 1, 3, 5, 7)
                int adj1_x = currentNode->pos.x + dx[i];
                int adj1_y = currentNode->pos.y;
                int adj2_x = currentNode->pos.x;
                int adj2_y = currentNode->pos.y + dy[i];
                
                if (level[adj1_y][adj1_x] == 1 || level[adj2_y][adj2_x] == 1) {
                    printf("[A* DEBUG]     Neighbor (%d,%d) is diagonal with corner cut. Skipping.\n", neighborPos.x, neighborPos.y);
                    continue; // Can't cut corners
                }
            }

            if (get_node_in_list(closedList, neighborPos) != nullptr) {
                printf("[A* DEBUG]     Neighbor (%d,%d) in closed list. Skipping.\n", neighborPos.x, neighborPos.y);
                continue;
            }

            // Calculate cost (1.0 for cardinal, 1.414 for diagonal)
            float moveCost = (i % 2 == 0) ? 1.0f : 1.414f;
            float tentative_g = currentNode->g + moveCost;

            Node* neighborNode = get_node_in_list(openList, neighborPos);

            if (neighborNode == nullptr) {
                neighborNode = new Node({neighborPos, currentNode, tentative_g, 0.0f, 0.0f});
                neighborNode->h = calculate_h_value(neighborPos, end);
                neighborNode->f = neighborNode->g + neighborNode->h;
                openList.push_back(neighborNode);
                printf("[A* DEBUG]     Neighbor (%d,%d) added to open list. g=%.2f h=%.2f f=%.2f\n", neighborPos.x, neighborPos.y, neighborNode->g, neighborNode->h, neighborNode->f);
            } else if (tentative_g < neighborNode->g) {
                printf("[A* DEBUG]     Neighbor (%d,%d) in open list, found better path! Old g=%.2f New g=%.2f\n", neighborPos.x, neighborPos.y, neighborNode->g, tentative_g);
                neighborNode->parent = currentNode;
                neighborNode->g = tentative_g;
                neighborNode->f = neighborNode->g + neighborNode->h;
            }
        }
    }

    printf("[A* DEBUG] Open list is empty, no path found from (%d,%d) to (%d,%d)!\n", start.x, start.y, end.x, end.y);
    // No path found - clean up memory
    for (Node* n : openList) delete n;
    for (Node* n : closedList) delete n;
    
    return std::vector<Point>();
}



// ============================================================================
// TeenyAT MMIO Bus Handlers
// ============================================================================

ConnectedClient* findClientByVM(teenyat *t) {
    for (auto& client : clients) {
        if (&client.vm == t) {
            return &client;
        }
    }
    return nullptr;
}

void bus_read(teenyat *t, tny_uword addr, tny_word *data, uint16_t *delay) {
    ConnectedClient* client = findClientByVM(t);
    if (!client) {
        data->u = 0;
        return;
    }

    int id = client->clientID;

    switch (addr) {
        case 0x9000: // CLIENT_ID
            data->u = id;
            break;
        case 0x9001: // CLIENT_TEAM
            data->u = players[id].team;
            break;
        case 0x9002: // CLIENT_X
            data->u = (int)players[id].x;
            break;
        case 0x9003: // CLIENT_Y
            data->u = (int)players[id].y;
            break;
        case 0x9022: // MAP_RESULT
            if (client->mapQueryX >= 0 && client->mapQueryX < LEVEL_WIDTH_TILES &&
                client->mapQueryY >= 0 && client->mapQueryY < LEVEL_HEIGHT_TILES) {
                data->u = level[client->mapQueryY][client->mapQueryX];
            } else {
                data->u = 1; // Out of bounds = wall
            }
            break;
        case 0x9031: // VISION_COUNT
            data->u = client->visionResults.size();
            break;
        case 0x9033: // VISION_ID
            if (client->visionSelectIndex < (int)client->visionResults.size()) {
                data->u = client->visionResults[client->visionSelectIndex].id;
            } else {
                data->u = 0;
            }
            break;
        case 0x9034: // VISION_TEAM
            if (client->visionSelectIndex < (int)client->visionResults.size()) {
                data->u = client->visionResults[client->visionSelectIndex].team;
            } else {
                data->u = 0;
            }
            break;
        case 0x9035: // VISION_X (relative)
            if (client->visionSelectIndex < (int)client->visionResults.size()) {
                data->s = client->visionResults[client->visionSelectIndex].relX;
            } else {
                data->s = 0;
            }
            break;
        case 0x9036: // VISION_Y (relative)
            if (client->visionSelectIndex < (int)client->visionResults.size()) {
                data->s = client->visionResults[client->visionSelectIndex].relY;
            } else {
                data->s = 0;
            }
            break;
        case 0x9037: // VISION_DIST
            if (client->visionSelectIndex < (int)client->visionResults.size()) {
                data->u = client->visionResults[client->visionSelectIndex].dist;
            } else {
                data->u = 0;
            }
            break;
        case 0x9101: // MOVE_RESULT (0 = failed, 1 = success)
            data->u = client->lastMoveResult;
            break;
        default:
            data->u = 0;
            break;
    }

    if (delay) *delay = 0;
}

void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay) {
    ConnectedClient* client = findClientByVM(t);
    if (!client) return;

    switch (addr) {
        case 0x9020: // MAP_QUERY_X
            client->mapQueryX = data.u;
            break;
        case 0x9021: // MAP_QUERY_Y
            client->mapQueryY = data.u;
            break;
        case 0x9030: // VISION_SCAN (trigger scan)
            if (data.u != 0) {
                performVisionScan(client, NUM_CLIENTS);
            }
            break;
        case 0x9032: // VISION_SELECT
            client->visionSelectIndex = data.u;
            break;
        case 0x9100: // MOVE_REQUEST
            client->lastMoveRequest = data.u;
            printf("[VM MOVE] Client %d (Team %d) requests direction %d\n",
                   client->clientID, client->teamID, data.u);
            processMovementRequest(client, data.u);
            break;
        case 0x9110: // TAG_REQUEST
            client->tagTargetID = data.u;
            handleTag(client->clientID, data.u);
            break;
        case 0x9200: // CLIENT_YIELD
            if (data.u != 0) {
                client->state = YIELDED;
            }
            break;
        default:
            break;
    }

    if (delay) *delay = 0;
}

void handleTag(int taggerID, int taggedID) {
    ConnectedClient* taggerClient = &clients[taggerID];
    taggerClient->lastTagResult = 0; // Default to fail

    if (taggedID < 0 || taggedID >= NUM_CLIENTS) return;
    if (players[taggerID].team == players[taggedID].team) return;
    if (players[taggedID].state != ACTIVE) return;
    if (players[taggerID].state == FROZEN) return;

    float dist = sqrt(pow(players[taggerID].x - players[taggedID].x, 2) +
                      pow(players[taggerID].y - players[taggedID].y, 2));
    if (dist > TAG_DISTANCE_PIXELS) {
        fprintf(stderr, "Tag failed: Client %d -> %d out of range (%.2f > %d)\n", taggerID, taggedID, dist, TAG_DISTANCE_PIXELS);
        return;
    }

    // Successful tag!
    printf("[TAGGING] Client %d successfully tagged client %d!\n", taggerID, taggedID);
    taggerClient->lastTagResult = 1;
    if (players[taggerID].team == 0) redScore++;
    else blueScore++;

    players[taggedID].state = FROZEN;
    players[taggedID].freezeTimer = 180; // 3 seconds at 60 FPS
    respawnPlayer(taggedID);
}

void startNewRound() {
    // Add round scores to total
    redTotalScore += redScore;
    blueTotalScore += blueScore;

    printf("\n");
    printf("+===================================================+\n");
    printf("|           ROUND %d COMPLETE!                       |\n", currentRound);
    printf("+===================================================+\n");
    printf("|  Round Score:  RED: %-3d  |  BLUE: %-3d            |\n", redScore, blueScore);
    printf("|  Total Score:  RED: %-3d  |  BLUE: %-3d            |\n", redTotalScore, blueTotalScore);
    printf("|  Winner: %-41s|\n",
           redScore > blueScore ? "RED TEAM!" :
           blueScore > redScore ? "BLUE TEAM!" : "TIE!");
    printf("+===================================================+\n");

    currentRound++;

    if (currentRound > MAX_ROUNDS) {
        printf("\n");
        printf("+===================================================+\n");
        printf("|              GAME OVER - FINAL RESULTS            |\n");
        printf("+===================================================+\n");
        printf("|  FINAL SCORE:  RED: %-3d  |  BLUE: %-3d            |\n", redTotalScore, blueTotalScore);
        printf("|  CHAMPION: %-40s|\n",
               redTotalScore > blueTotalScore ? "RED TEAM!!!" :
               blueTotalScore > redTotalScore ? "BLUE TEAM!!!" : "IT'S A TIE!!!");
        printf("+===================================================+\n");
        exit(0);
    }

    // Reset for new round
    redScore = 0;
    blueScore = 0;
    gameTimer = 1800; // 30 seconds * 60 FPS

    // Load new random map
    loadRandomMap();

    // Respawn all players
    for (int i = 0; i < NUM_CLIENTS; i++) {
        players[i].state = ACTIVE;
        players[i].freezeTimer = 0;
        players[i].immunityTimer = 0;

        // Spawn locations
        int spawnTileX, spawnTileY;
        if (players[i].team == 0) {
            spawnTileX = 2 + (i % 5) * 2;
            spawnTileY = 2 + (rand() % 2);
        } else {
            int idx = i - NUM_CLIENTS / 2;
            spawnTileX = 33 + (idx % 5) * (-2);
            spawnTileY = 26 + (rand() % 2);
        }

        while (level[spawnTileY][spawnTileX] == 1) {
            spawnTileX = (spawnTileX + 1) % (LEVEL_WIDTH_TILES - 2) + 1;
        }

        players[i].x = spawnTileX * TILE_SIZE + 16;
        players[i].y = spawnTileY * TILE_SIZE + 16;

        // Reset stuck trackers
        stuckTrackers[i] = StuckTracker();
        stuckTrackers[i].lastX[0] = players[i].x;
        stuckTrackers[i].lastY[0] = players[i].y;

        // Reset TeenyAT VM to start fresh for new round
        clients[i].state = ACTIVE;
        tny_reset(&clients[i].vm);
    }

    printf("\n>>> ROUND %d STARTING! <<<\n\n", currentRound);
}

void updateGame() {
    if (gameTimer > 0) {
        gameTimer--;
        if (gameTimer == 0) {
            startNewRound();
        }
    }

    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (players[i].state == FROZEN) {
            players[i].freezeTimer--;
            if (players[i].freezeTimer <= 0) {
                players[i].state = IMMUNE;
                players[i].immunityTimer = 120; // 2 seconds
            }
        } else if (players[i].state == IMMUNE) {
            players[i].immunityTimer--;
            if (players[i].immunityTimer <= 0) {
                players[i].state = ACTIVE;
                clients[i].state = ACTIVE;
            }
        }
    }

    // --- SEPARATION FORCE: Gently push apart players that are too close ---
    // Only applies to same-team players to prevent teammate collisions
    // Enemy collisions should result in tags, not separation
    const float SEPARATION_DIST = 20.0f;  // Only push if VERY close (same team blocking)
    const float PUSH_STRENGTH = 0.5f;     // Gentle push to avoid shaking

    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (players[i].state == FROZEN || players[i].state == DISCONNECTED) continue;

        for (int j = i + 1; j < NUM_CLIENTS; j++) {
            if (players[j].state == FROZEN || players[j].state == DISCONNECTED) continue;

            // Only separate teammates - enemies should be able to get close for tagging
            if (players[i].team != players[j].team) continue;

            float dx = players[j].x - players[i].x;
            float dy = players[j].y - players[i].y;
            float dist = sqrt(dx*dx + dy*dy);

            if (dist > 0 && dist < SEPARATION_DIST) {
                // Normalize and push apart gently
                float pushX = (dx / dist) * PUSH_STRENGTH;
                float pushY = (dy / dist) * PUSH_STRENGTH;

                // Try to push player i away (opposite direction)
                float newXi = players[i].x - pushX;
                float newYi = players[i].y - pushY;
                if (isPositionClear(newXi, newYi)) {
                    players[i].x = newXi;
                    players[i].y = newYi;
                }

                // Try to push player j away
                float newXj = players[j].x + pushX;
                float newYj = players[j].y + pushY;
                if (isPositionClear(newXj, newYj)) {
                    players[j].x = newXj;
                    players[j].y = newYj;
                }
            }
        }
    }

    updateDynamicMap();
}



// ============================================================================
// Main Loop
// ============================================================================

// Client program assignments - which ASM binary each client uses
const char* clientPrograms[] = {
    "ChaseClient.bin",    // Client 0 - Red Team
    "TagClient.bin",      // Client 1 - Red Team
    "MinimalClient.bin",  // Client 2 - Red Team
    "ChaseClient.bin",    // Client 3 - Red Team
    "TagClient.bin",      // Client 4 - Red Team
    "MinimalClient.bin",  // Client 5 - Red Team
    "ChaseClient.bin",    // Client 6 - Red Team
    "TagClient.bin",      // Client 7 - Red Team
    "MinimalClient.bin",  // Client 8 - Red Team
    "ChaseClient.bin",    // Client 9 - Red Team
    "TagClient.bin",      // Client 10 - Blue Team
    "MinimalClient.bin",  // Client 11 - Blue Team
    "ChaseClient.bin",    // Client 12 - Blue Team
    "TagClient.bin",      // Client 13 - Blue Team
    "MinimalClient.bin",  // Client 14 - Blue Team
    "ChaseClient.bin",    // Client 15 - Blue Team
    "TagClient.bin",      // Client 16 - Blue Team
    "MinimalClient.bin",  // Client 17 - Blue Team
    "ChaseClient.bin",    // Client 18 - Blue Team
    "TagClient.bin",      // Client 19 - Blue Team
};

int main(int argc, char *argv[]) {
#ifdef ENABLE_TESTS
    runAllTests(argc, argv);
    return 0;
#endif

    srand(time(NULL));
    initializeWorld();

    clients.reserve(NUM_CLIENTS);
    stuckTrackers.resize(NUM_CLIENTS);  // Initialize stuck trackers

    // --- Initialize clients with TeenyAT VMs ---
    for (int i = 0; i < NUM_CLIENTS; i++) {
        clients.emplace_back();
        ConnectedClient& client = clients.back();
        client.clientID = i;
        client.teamID = (i < NUM_CLIENTS / 2) ? 0 : 1;
        client.state = ACTIVE;
        client.visionSelectIndex = 0;
        client.mapQueryX = 0;
        client.mapQueryY = 0;
        client.lastMoveRequest = -1;

        // Load the appropriate ASM binary for this client
        const char* programFile = clientPrograms[i % 20];
        FILE* binFile = fopen(programFile, "rb");
        if (binFile) {
            if (!tny_init_unclocked(&client.vm, binFile, bus_read, bus_write)) {
                fprintf(stderr, "Failed to initialize VM for client %d with %s\n", i, programFile);
            } else {
                printf("[INIT] Client %d loaded %s (Team %s)\n", i, programFile,
                       client.teamID == 0 ? "RED" : "BLUE");
            }
            fclose(binFile);
        } else {
            fprintf(stderr, "Failed to open %s for client %d\n", programFile, i);
        }

        // Initialize stuck tracker with starting position
        stuckTrackers[i].lastX[0] = players[i].x;
        stuckTrackers[i].lastY[0] = players[i].y;
    }

    printf("=====================================================\n");
    printf("        TACTICAL TAG WAR - All-Out Battle\n");
    printf("=====================================================\n");
    printf("  %d units per team | Both teams can tag!\n", NUM_CLIENTS/2);
    printf("-----------------------------------------------------\n");
    printf("  AI BEHAVIORS:\n");
    printf("    AGGRESSOR - Direct chase, retreats when outnumbered\n");
    printf("    FLANKER   - Approaches from angles\n");
    printf("    DEFENDER  - Guards spawn, attacks nearby enemies\n");
    printf("    PACK      - Stays with teammates, attacks together\n");
    printf("-----------------------------------------------------\n");
    printf("  Red Team: Clients 0-%d\n", NUM_CLIENTS/2 - 1);
    printf("  Blue Team: Clients %d-%d\n", NUM_CLIENTS/2, NUM_CLIENTS - 1);
    printf("=====================================================\n\n");

    Tigr* screen = tigrWindow(SCREEN_WIDTH_PIXELS, SCREEN_HEIGHT_PIXELS, "Tactical Tag War - Multiagent System", 0);
    while (!tigrClosed(screen)) {
        // === MULTIAGENT SYSTEM: Execute each client's TeenyAT VM ===
        for (int i = 0; i < NUM_CLIENTS; i++) {
            // Skip frozen/disconnected players - their VMs don't run
            if (players[i].state == FROZEN || players[i].state == DISCONNECTED) {
                continue;
            }

            // Reset client state for this frame (clear YIELDED status)
            if (clients[i].state == YIELDED) {
                clients[i].state = ACTIVE;
            }

            // Execute VM cycles until client yields or hits cycle limit
            // The VM will call bus_write(MOVE_REQUEST) which calls processMovementRequest()
            // Server-side collision detection happens inside processMovementRequest()
            for (int cycle = 0; cycle < CYCLES_PER_FRAME; cycle++) {
                if (clients[i].state == YIELDED) {
                    break;  // Client yielded control, stop executing
                }
                tny_clock(&clients[i].vm);
            }
        }

        updateGame();
        checkTaggingEvents();
        renderGame(screen);
        tigrUpdate(screen);
    }
    tigrFree(screen);

    return 0;
}

void checkTaggingEvents() {
    // Collect all potential tag pairs first to avoid order bias
    struct TagPair {
        int tagger;
        int tagged;
        float dist;
    };
    std::vector<TagPair> potentialTags;

    for (int i = 0; i < NUM_CLIENTS; i++) {
        if (players[i].state != ACTIVE) continue;

        for (int j = i + 1; j < NUM_CLIENTS; j++) {  // Only check each pair once
            if (players[j].state != ACTIVE) continue;
            if (players[i].team == players[j].team) continue;

            float dx = players[i].x - players[j].x;
            float dy = players[i].y - players[j].y;
            float dist = sqrt(dx*dx + dy*dy);

            if (dist < TAG_DISTANCE_PIXELS) {
                // Randomly decide who tags who (50/50 chance)
                if (rand() % 2 == 0) {
                    potentialTags.push_back({i, j, dist});
                } else {
                    potentialTags.push_back({j, i, dist});
                }
            }
        }
    }

    // Process tags - but skip if either player already tagged this frame
    std::vector<bool> taggedThisFrame(NUM_CLIENTS, false);
    for (const auto& tag : potentialTags) {
        if (!taggedThisFrame[tag.tagger] && !taggedThisFrame[tag.tagged]) {
            if (players[tag.tagger].state == ACTIVE && players[tag.tagged].state == ACTIVE) {
                handleTag(tag.tagger, tag.tagged);
                taggedThisFrame[tag.tagged] = true;
            }
        }
    }
}

void renderGame(Tigr* screen) {
    tigrClear(screen, tigrRGB(0x20, 0x20, 0x20));

    for (int y = 0; y < LEVEL_HEIGHT_TILES; y++) {
        for (int x = 0; x < LEVEL_WIDTH_TILES; x++) {
            if (level[y][x] == 1) {
                tigrFill(screen, x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, tigrRGB(100, 100, 100));
            }
        }
    }

    for (int i = 0; i < NUM_CLIENTS; i++) {
        TPixel color = (players[i].team == 0) ? tigrRGB(255, 0, 0) : tigrRGB(0, 0, 255);
        if (players[i].state == IMMUNE) color = tigrRGB(0, 255, 0);
        if (players[i].state == FROZEN) color = tigrRGB(128, 128, 128);
        tigrFill(screen, (int)players[i].x, (int)players[i].y, 16, 16, color);
    }

    char scoreText[64];
    sprintf(scoreText, "Red: %d  Blue: %d", redScore, blueScore);
    tigrPrint(screen, tfont, 10, 10, tigrRGB(255, 255, 255), scoreText);

    char timerText[64];
    sprintf(timerText, "Time: %d.%02d", gameTimer / 60, (gameTimer % 60) * 100 / 60);
    tigrPrint(screen, tfont, SCREEN_WIDTH_PIXELS - 150, 10, tigrRGB(255, 255, 255), timerText);

    // Display round and map info
    char roundText[64];
    sprintf(roundText, "Round %d/%d  Map: %s", currentRound, MAX_ROUNDS, mapNames[currentMapIndex]);
    tigrPrint(screen, tfont, SCREEN_WIDTH_PIXELS / 2 - 100, 10, tigrRGB(255, 255, 0), roundText);

    // Display total scores if in multi-round game
    if (currentRound > 1 || redTotalScore > 0 || blueTotalScore > 0) {
        char totalText[64];
        sprintf(totalText, "Total - Red: %d  Blue: %d", redTotalScore, blueTotalScore);
        tigrPrint(screen, tfont, SCREEN_WIDTH_PIXELS / 2 - 80, 30, tigrRGB(200, 200, 200), totalText);
    }
}