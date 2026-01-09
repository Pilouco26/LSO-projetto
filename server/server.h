/**
 * LSO Project - Forza 4 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

// =======================
// CONSTANTS
// ======================

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100
#define MAX_GAMES 50
#define MAX_USERNAME 32

// Grid dimensions
#define GRID_ROWS 6
#define GRID_COLS 7

// Player symbols
#define EMPTY '.'
#define PLAYER1 'X'
#define PLAYER2 'O'

// Game states
typedef enum {
    GAME_CREATED,       
    GAME_WAITING,       
    GAME_IN_PROGRESS,   
    GAME_FINISHED       
} GameState;

// ======================
// DATA STRUCTURES
// ======================

// Forward declarations
struct Game;
struct Client;

// Client structure
typedef struct Client {
    int id;
    int socket;
    char username[MAX_USERNAME];
    int is_connected;
    int current_game_id;        
    struct sockaddr_in address;
    pthread_t thread;
} Client;

// Join request structure
typedef struct JoinRequest {
    int requester_id;
    int processed;              
    struct JoinRequest *next;
} JoinRequest;

// Game structure
typedef struct Game {
    int id;
    char grid[GRID_ROWS][GRID_COLS];
    GameState state;
    int creator_id;             
    int opponent_id;            
    int current_turn;           
    int winner_id;              
    int is_active;              
    JoinRequest *join_requests; 
    pthread_mutex_t game_mutex; 
} Game;

// ==========================
// HEADER INCLUDES
// ==========================

#include "include/server_utils.h"
#include "include/server_game_logic.h"
#include "include/server_game_management.h"
#include "include/server_handlers.h"

// ===========================
// GLOBAL VARIABLES
// ===========================

extern int server_socket;
extern Client clients[MAX_CLIENTS];
extern int client_count;
extern pthread_mutex_t clients_mutex;
extern Game games[MAX_GAMES];
extern pthread_mutex_t games_mutex;
extern volatile int server_running;

#endif
