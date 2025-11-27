#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include "server.h"
extern "C" {
#include "teenyat.h"
}
#include "tigr.h"

// --- Globals ---
std::vector<ConnectedClient> clients;
Player players[NUM_CLIENTS];
int level[LEVEL_HEIGHT_TILES][LEVEL_WIDTH_TILES];
std::unordered_map<int, std::deque<Message>> clientInboxes;
int redScore = 0;
int blueScore = 0;

// --- Forward Declarations ---
void renderGame(Tigr* screen);
void updateGame();
ConnectedClient* findClientByVM(teenyat *t);
void bus_read(teenyat *t, tny_uword addr, tny_word *data, uint16_t *delay);
void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay);
void respawnPlayer(int clientID);

#ifdef ENABLE_TESTS
#include "tests.h"
#endif

// ============================================================================
// Game Logic
// ============================================================================

void initializeWorld() {
    // Initialize level with walls around the border
    for (int y = 0; y < LEVEL_HEIGHT_TILES; y++) {
        for (int x = 0; x < LEVEL_WIDTH_TILES; x++) {
            if (x == 0 || x == LEVEL_WIDTH_TILES - 1 || y == 0 || y == LEVEL_HEIGHT_TILES - 1) {
                level[y][x] = 1; // Wall
            } else {
                level[y][x] = 0; // Empty space
            }
        }
    }

    // Randomly place some internal walls
    for (int i = 0; i < 50; i++) {
        int x = rand() % (LEVEL_WIDTH_TILES - 2) + 1;
        int y = rand() % (LEVEL_HEIGHT_TILES - 2) + 1;
        level[y][x] = 1;
    }

    // Initialize players randomly
    for (int i = 0; i < NUM_CLIENTS; i++) {
        players[i].x = (float)(rand() % (SCREEN_WIDTH_PIXELS - 64) + 32);
        players[i].y = (float)(rand() % (SCREEN_HEIGHT_PIXELS - 64) + 32);
        players[i].team = (i < NUM_CLIENTS / 2) ? 0 : 1; // First half red, second half blue
        players[i].state = ACTIVE;
        players[i].freezeTimer = 0;
        players[i].immunityTimer = 0;
    }
}

void processMovementRequest(ConnectedClient* client, int direction) {
    if (players[client->clientID].state == FROZEN) return;

    float newX = players[client->clientID].x;
    float newY = players[client->clientID].y;
    int speed = 2; // Pixels per frame

    switch (direction) {
        case 0: newY -= speed; break; // N
        case 1: newY -= speed; newX += speed; break; // NE
        case 2: newX += speed; break; // E
        case 3: newY += speed; newX += speed; break; // SE
        case 4: newY += speed; break; // S
        case 5: newY += speed; newX -= speed; break; // SW
        case 6: newX -= speed; break; // W
        case 7: newY -= speed; newX -= speed; break; // NW
    }

    // Basic collision detection with walls
    int tileX = (int)(newX / TILE_SIZE);
    int tileY = (int)(newY / TILE_SIZE);

    if (tileX >= 0 && tileX < LEVEL_WIDTH_TILES &&
        tileY >= 0 && tileY < LEVEL_HEIGHT_TILES &&
        level[tileY][tileX] != 1) {
        
        int playerSize = 16;
        int corners[4][2] = {
            {(int)newX, (int)newY},
            {(int)newX + playerSize - 1, (int)newY},
            {(int)newX, (int)newY + playerSize - 1},
            {(int)newX + playerSize - 1, (int)newY + playerSize - 1}
        };

        bool collision = false;
        for (int i = 0; i < 4; i++) {
            int cornerTileX = corners[i][0] / TILE_SIZE;
            int cornerTileY = corners[i][1] / TILE_SIZE;
            if (cornerTileX < 0 || cornerTileX >= LEVEL_WIDTH_TILES ||
                cornerTileY < 0 || cornerTileY >= LEVEL_HEIGHT_TILES ||
                level[cornerTileY][cornerTileX] == 1) {
                collision = true;
                break;
            }
        }

        if (!collision) {
            players[client->clientID].x = newX;
            players[client->clientID].y = newY;
        }
    }
}

// Bresenham's Line Algorithm for Line of Sight (pixel-based)
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
    
    int spawnX, spawnY;
    do {
        if (team == 0) { // Red team: top-left
            spawnX = 2 + (rand() % 5);
            spawnY = 2 + (rand() % 5);
        } else { // Blue team: bottom-right
            spawnX = LEVEL_WIDTH_TILES - 7 + (rand() % 5);
            spawnY = LEVEL_HEIGHT_TILES - 7 + (rand() % 5);
        }
    } while (level[spawnY][spawnX] != 0); // Ensure spawning in an empty space
    
    players[clientID].x = spawnX * TILE_SIZE + (TILE_SIZE / 4);
    players[clientID].y = spawnY * TILE_SIZE + (TILE_SIZE / 4);
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
        fprintf(stderr, "Tag failed: Client %d -> %d out of range (%.2f > %d)\\n", taggerID, taggedID, dist, TAG_DISTANCE_PIXELS);
        return;
    }

    // Successful tag!
    taggerClient->lastTagResult = 1;
    if (players[taggerID].team == 0) redScore++;
    else blueScore++;

    players[taggedID].state = FROZEN;
    players[taggedID].freezeTimer = 180; // 3 seconds at 60 FPS
    respawnPlayer(taggedID);
}

void updateGame() {
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
                clients[i].state = ACTIVE; // Also reset the execution state
            }
        }
    }
}

ConnectedClient* findClientByVM(teenyat *t) {
    for (auto& client : clients) {
        if (&client.vm == t) {
            return &client;
        }
    }
    return nullptr;
}

// ============================================================================
// MMIO Bus Callbacks
// ============================================================================

void bus_read(teenyat *t, tny_uword addr, tny_word *data, uint16_t *) {
    ConnectedClient* client = findClientByVM(t);
    if (!client) { data->u = 0; return; }
    int id = client->clientID;
    switch(addr) {
        case CLIENT_ID:   data->u = id; break;
        case CLIENT_TEAM: data->u = players[id].team; break;
        case CLIENT_X:    data->u = (tny_uword)players[id].x; break;
        case CLIENT_Y:    data->u = (tny_uword)players[id].y; break;
        case CLIENT_STATE: data->u = players[id].state; break;

        case MAP_RESULT: {
            int tileX = client->mapQueryX;
            int tileY = client->mapQueryY;
            if (tileX >= 0 && tileX < LEVEL_WIDTH_TILES &&
                tileY >= 0 && tileY < LEVEL_HEIGHT_TILES) {
                data->u = level[tileY][tileX];
            } else {
                data->u = 1;
            }
            break;
        }

        case VISION_COUNT: data->u = client->visionResults.size(); break;
        case VISION_ID:    if(client->visionSelectIndex < client->visionResults.size()) data->u = client->visionResults[client->visionSelectIndex].id; break;
        case VISION_TEAM:  if(client->visionSelectIndex < client->visionResults.size()) data->u = client->visionResults[client->visionSelectIndex].team; break;
        case VISION_X:     if(client->visionSelectIndex < client->visionResults.size()) data->u = client->visionResults[client->visionSelectIndex].relX; break;
        case VISION_Y:     if(client->visionSelectIndex < client->visionResults.size()) data->u = client->visionResults[client->visionSelectIndex].relY; break;
        case VISION_DIST:  if(client->visionSelectIndex < client->visionResults.size()) data->u = client->visionResults[client->visionSelectIndex].dist; break;

        case MSG_INBOX_COUNT: data->u = clientInboxes[id].size(); break;
        case MSG_READ_FROM:   if(client->messageReadIndex < clientInboxes[id].size()) data->u = clientInboxes[id][client->messageReadIndex].fromID; break;
        case MSG_READ_TYPE:   if(client->messageReadIndex < clientInboxes[id].size()) data->u = clientInboxes[id][client->messageReadIndex].type; break;
        case MSG_READ_DATA:   if(client->messageReadIndex < clientInboxes[id].size()) data->u = clientInboxes[id][client->messageReadIndex].data; break;
        
        case TAG_RESULT: data->u = client->lastTagResult; break;

        default: data->u = 0; break;
    }
}

void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *) {
    ConnectedClient* client = findClientByVM(t);
    if (!client) return;
    
    switch(addr) {
        case MOVE_REQUEST: 
            client->lastMoveRequest = data.u;
            processMovementRequest(client, data.u); 
            break;
        case VISION_SCAN:
            performVisionScan(client, NUM_CLIENTS);
            break;
        case CLIENT_YIELD:
            if (data.u == 1) client->state = YIELDED;
            break;
        case MAP_QUERY_X: client->mapQueryX = data.u; break;
        case MAP_QUERY_Y: client->mapQueryY = data.u; break;
        case VISION_SELECT: client->visionSelectIndex = data.u; break;
        case MSG_SEND_TO: client->msgSendTo = data.u; break;
        case MSG_SEND_TYPE: client->msgSendType = data.u; break;
        case MSG_SEND_DATA: client->msgSendData = data.u; break;
        case MSG_SEND_EXEC: sendMessage(client->clientID, client->msgSendTo, client->msgSendType, client->msgSendData); break;
        case MSG_READ_IDX: client->messageReadIndex = data.u; break;
        case MSG_POP: if(!clientInboxes[client->clientID].empty()) clientInboxes[client->clientID].pop_front(); break;
        case TAG_REQUEST: handleTag(client->clientID, data.u); break;
    }
}

// ============================================================================
// Main Loop
// ============================================================================

int main(int argc, char *argv[]) {
#ifdef ENABLE_TESTS
    runAllTests(argc, argv);
    return 0;
#endif

    srand(time(NULL));
    initializeWorld();

    clients.reserve(NUM_CLIENTS);

    // --- Initialize Server ---
        const char* chase_client_path = "TagClient.bin";
    const char* minimal_client_path = "MinimalClient.bin";
    for (int i = 0; i < NUM_CLIENTS; i++) {
        const char* path = (i < NUM_CLIENTS / 2) ? chase_client_path : minimal_client_path;
        FILE *f = fopen(path, "rb");
        if (!f) { std::cerr << "FATAL: Could not open client binary " << path << std::endl; return 1; }
        
        clients.emplace_back();
        ConnectedClient& client = clients.back();

        client.clientID = i;
        client.teamID = (i < NUM_CLIENTS / 2) ? 0 : 1;
        client.state = ACTIVE;
        tny_init_from_file(&client.vm, f, bus_read, bus_write);
        tny_reset(&client.vm);
        fclose(f);
    }

    Tigr* screen = tigrWindow(SCREEN_WIDTH_PIXELS, SCREEN_HEIGHT_PIXELS, "TeenyAT Multi-Agent System", 0);
    while (!tigrClosed(screen)) {
        for (auto& client : clients) {
            if (client.state == YIELDED) client.state = ACTIVE;
            if (players[client.clientID].state == ACTIVE) {
                for (int i = 0; i < CYCLES_PER_FRAME; i++) {
                    tny_clock(&client.vm);
                    if (client.state == YIELDED) break;
                }
            }
        }

        updateGame();
        renderGame(screen);
        tigrUpdate(screen);
    }
    tigrFree(screen);

    return 0;
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
}
