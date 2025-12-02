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
#include <cassert>
#include <chrono>
#include <functional>

extern "C" {
#include "teenyat.h"
}
#include "server.h"
#include "tests.h"

// --- Forward Declarations for Tests ---
void test_vision();
void test_messages();
void test_collision();
void test_tagging();
void test_client_movement();
void updateGame(); // Forward declare updateGame

// --- Externs for Server Globals ---
extern std::vector<ConnectedClient> clients;
extern Player players[];
extern int level[LEVEL_HEIGHT_TILES][LEVEL_WIDTH_TILES];
extern std::unordered_map<int, std::deque<Message>> clientInboxes;
extern int redScore;
extern int blueScore;

// --- Externs for Server Functions ---
extern void initializeWorld();
extern void processMovementRequest(ConnectedClient* client, int direction);
extern void performVisionScan(ConnectedClient* client, int numClientsToCheck);
extern void sendMessage(int fromID, int toID, int type, int data);
extern void handleTag(int tagger, int tagged);
extern void bus_read(teenyat *t, tny_uword addr, tny_word *data, uint16_t *delay);
extern void bus_write(teenyat *t, tny_uword addr, tny_word data, uint16_t *delay);


// ============================================================================
// Test Implementations
// ============================================================================

void test_vision() {
    printf("Testing vision system...\n");
    initializeWorld();
    players[0] = {.x=64, .y=64, .team=0, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0};
    players[1] = {.x=160, .y=64, .team=1, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0};
    players[2] = {.x=128, .y=64, .team=1, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0};
    ConnectedClient testClient = {
        .vm={}, .clientID=0, .teamID=0, .state=ACTIVE, .mapQueryX=0, .mapQueryY=0,
        .visionSelectIndex=0, .msgSendTo=0, .msgSendType=0, .msgSendData=0,
        .messageReadIndex=0, .tagTargetID=0, .lastTagResult=0, .visionResults={}
    };

    performVisionScan(&testClient, 3);
    assert(testClient.visionResults.size() == 2);
    printf("  - Vision detection works\n");
    
    level[2][3] = 1;
    performVisionScan(&testClient, 3);
    assert(testClient.visionResults.size() == 0);
    printf("  - Line-of-sight blocking works\n");
    level[2][3] = 0;

    players[1].x = 400;
    performVisionScan(&testClient, 3);
    assert(testClient.visionResults.size() == 1);
    printf("  - Vision radius limit works\n");

    players[1].x = 96;
    performVisionScan(&testClient, 3);
    assert(testClient.visionResults[0].dist < testClient.visionResults[1].dist);
    printf("  - Multiple entity sorting works\n");
    printf("--- Vision tests passed ---\n\n");
}

void test_messages() {
    printf("Testing message system...\n");
    clientInboxes.clear();
    sendMessage(0, 5, 0x01, 0xABCD);
    assert(clientInboxes[5].size() == 1);
    printf("  - Basic message send/receive works\n");
    
    sendMessage(0, 0, 0x01, 0x1234);
    assert(clientInboxes[0].empty());
    printf("  - Self-messaging prevention works\n");

    for (int i = 0; i < 10; i++) sendMessage(0, 7, 0x02, i);
    assert(clientInboxes[7].size() == 8);
    printf("  - Inbox size limit works\n");

    clientInboxes[1].clear();
    sendMessage(0, 1, 0x03, 0xAAAA);
    sendMessage(2, 1, 0x03, 0xBBBB);
    clientInboxes[1].pop_front();
    assert(clientInboxes[1].front().data == 0xBBBB);
    printf("  - Message queue FIFO works\n");
    printf("--- Message tests passed ---\n\n");
}

void test_collision() {
    printf("Testing collision system...\n");
    initializeWorld();
    ConnectedClient testClient = {
        .vm={}, .clientID=0, .teamID=0, .state=ACTIVE, .mapQueryX=0, .mapQueryY=0,
        .visionSelectIndex=0, .msgSendTo=0, .msgSendType=0, .msgSendData=0,
        .messageReadIndex=0, .tagTargetID=0, .lastTagResult=0, .visionResults={}
    };
    
    players[0] = {.x=64, .y=64, .team=0, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0};
    level[2][2] = 1;
    processMovementRequest(&testClient, 2);
    assert(players[0].x == 64);
    printf("  - Wall collision detection works\n");
    level[2][2] = 0;

    players[0].x = SCREEN_WIDTH_PIXELS - 10;
    processMovementRequest(&testClient, 2);
    assert(players[0].x == SCREEN_WIDTH_PIXELS - 10);
    printf("  - Bounds checking works\n");
    printf("--- Collision tests passed ---\n\n");
}

void test_tagging() {
    printf("Testing tag mechanics...\n");
    initializeWorld();
    redScore = 0;
    blueScore = 0;
    clients.clear(); // Ensure a clean state
    clients.emplace_back(ConnectedClient{
        .vm={}, .clientID=0, .teamID=0, .state=ACTIVE, .mapQueryX=0, .mapQueryY=0,
        .visionSelectIndex=0, .msgSendTo=0, .msgSendType=0, .msgSendData=0,
        .messageReadIndex=0, .tagTargetID=0, .lastTagResult=0, .visionResults={}
    });
    clients.emplace_back(ConnectedClient{
        .vm={}, .clientID=1, .teamID=1, .state=ACTIVE, .mapQueryX=0, .mapQueryY=0,
        .visionSelectIndex=0, .msgSendTo=0, .msgSendType=0, .msgSendData=0,
        .messageReadIndex=0, .tagTargetID=0, .lastTagResult=0, .visionResults={}
    });

    // Test 1: Successful tag
    players[0] = {.x=64, .y=64, .team=0, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0}; // Tagger
    players[1] = {.x=70, .y=70, .team=1, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0}; // Tagged
    
    handleTag(0, 1);
    assert(redScore == 1 && blueScore == 0);
    assert(players[1].state == FROZEN);
    assert(players[1].freezeTimer == 180);
    printf("  - Tag detection, scoring, and freezing works\n");

    // Test 2: Friendly fire prevention
    players[0] = {.x=64, .y=64, .team=0, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0};
    players[1] = {.x=70, .y=70, .team=0, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0}; // Same team
    redScore = 0;
    handleTag(0, 1);
    assert(redScore == 0);
    assert(players[1].state == ACTIVE);
    printf("  - Friendly-fire prevention works\n");

    // Test 3: State transition from FROZEN -> IMMUNE -> ACTIVE
    players[0] = {.x=64, .y=64, .team=0, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0};
    players[1] = {.x=70, .y=70, .team=1, .state=ACTIVE, .freezeTimer=0, .immunityTimer=0};
    handleTag(0, 1); // Player 1 is now FROZEN for 180 frames

    for(int i = 0; i < 180; i++) {
        updateGame();
    }
    assert(players[1].state == IMMUNE);
    assert(players[1].immunityTimer == 120);
    printf("  - State correctly transitions from FROZEN to IMMUNE\n");

    for(int i = 0; i < 120; i++) {
        updateGame();
    }
    assert(players[1].state == ACTIVE);
    printf("  - State correctly transitions from IMMUNE to ACTIVE\n");

    printf("--- Tag tests passed ---\n\n");
}

// Helper function to run a client for one "frame" and get its move request
int getClientMoveForFrame(ConnectedClient& client) {
    client.lastMoveRequest = -1; // Reset before run
    client.state = ACTIVE;
    players[client.clientID].state = ACTIVE;

    // Ensure the VM is reset to a clean state before the test tick
    tny_reset(&client.vm);

    for (int i = 0; i < CYCLES_PER_FRAME * 2; i++) { // Give it plenty of cycles
        if (client.state == YIELDED) break;
        tny_clock(&client.vm);
    }
    return client.lastMoveRequest;
}

void test_chase_logic() {
    printf("Testing chase logic AI...\n");
    // Setup a clean environment
    initializeWorld();
    clients.clear(); // Remove clients, but players[] array still has data
    for (int i = 2; i < NUM_CLIENTS; i++) {
        players[i].state = DISCONNECTED; // Neutralize other players
    }
    for (int y=0; y<LEVEL_HEIGHT_TILES; y++) for (int x=0; x<LEVEL_WIDTH_TILES; x++) level[y][x] = 0;

    // Load ChaseClient for testing chase logic
    FILE *f_chase = fopen("ChaseClient.bin", "rb");
    if (!f_chase) {
        printf("  ERROR: Could not open ChaseClient.bin - skipping chase logic tests\n");
        printf("  Please assemble ChaseClient.asm first.\n");
        return;
    }

    // Create Red 0 (the chaser)
    clients.emplace_back();
    ConnectedClient& redClient = clients.back();
    redClient.clientID = 0;
    redClient.teamID = 0;
    redClient.visionSelectIndex = 0;
    tny_init_from_file(&redClient.vm, f_chase, bus_read, bus_write);
    players[0] = {.x=500, .y=500, .team=0, .state=ACTIVE};

    // Create Blue 1 (the target)
    clients.emplace_back();
    ConnectedClient& blueClient = clients.back();
    blueClient.clientID = 1;
    blueClient.teamID = 1;
    players[1] = {.x=0, .y=0, .team=1, .state=ACTIVE};

    fclose(f_chase);

    // Direction encoding used in ChaseClient.asm:
    // 0=East, 1=SE, 2=South, 3=SW, 4=West, 5=NW, 6=North, 7=NE

    // Case: East (enemy to the right)
    players[0].x = 500; players[0].y = 500;
    players[1].x = 600; players[1].y = 500;
    assert(getClientMoveForFrame(redClient) == 0); // East = 0
    printf("  - Chase East works\n");

    // Case: South-East
    players[0].x = 500; players[0].y = 500;
    players[1].x = 600; players[1].y = 600;
    assert(getClientMoveForFrame(redClient) == 1); // SE = 1
    printf("  - Chase South-East works\n");

    // Case: South (enemy below)
    players[0].x = 500; players[0].y = 500;
    players[1].x = 500; players[1].y = 600;
    assert(getClientMoveForFrame(redClient) == 2); // South = 2
    printf("  - Chase South works\n");

    // Case: South-West
    players[0].x = 500; players[0].y = 500;
    players[1].x = 400; players[1].y = 600;
    assert(getClientMoveForFrame(redClient) == 3); // SW = 3
    printf("  - Chase South-West works\n");

    // Case: West (enemy to the left)
    players[0].x = 500; players[0].y = 500;
    players[1].x = 400; players[1].y = 500;
    assert(getClientMoveForFrame(redClient) == 4); // West = 4
    printf("  - Chase West works\n");

    // Case: North-West
    players[0].x = 500; players[0].y = 500;
    players[1].x = 400; players[1].y = 400;
    assert(getClientMoveForFrame(redClient) == 5); // NW = 5
    printf("  - Chase North-West works\n");

    // Case: North (enemy above)
    players[0].x = 500; players[0].y = 500;
    players[1].x = 500; players[1].y = 400;
    assert(getClientMoveForFrame(redClient) == 6); // North = 6
    printf("  - Chase North works\n");

    // Case: North-East
    players[0].x = 500; players[0].y = 500;
    players[1].x = 600; players[1].y = 400;
    assert(getClientMoveForFrame(redClient) == 7); // NE = 7
    printf("  - Chase North-East works\n");

    printf("--- Chase logic tests passed ---\n\n");
}


// ============================================================================
// Test Runner
// ============================================================================

void runAllTests(int argc, char *argv[]) {
    bool run_all = false;
    std::string specific_test;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--run-tests") == 0) run_all = true;
        if (strncmp(argv[i], "--test=", 7) == 0) {
            specific_test = argv[i] + 7;
        }
    }

    printf("==================================================\n");
    printf("         TeenyAT Multi-Agent System Tests        \n");
    printf("==================================================\n\n");

    if (run_all || specific_test == "vision") test_vision();
    if (run_all || specific_test == "messages") test_messages();
    if (run_all || specific_test == "collision") test_collision();
    if (run_all || specific_test == "tagging") test_tagging();
    if (run_all || specific_test == "chase_logic") test_chase_logic();
    
    if (specific_test.empty() && !run_all) {
        printf("No tests specified. Use --run-tests or --test=<name>\n");
    }

    printf("\n==================================================\n");
    printf("           ALL SPECIFIED TESTS PASSED \n");
    printf("==================================================\n");
}
