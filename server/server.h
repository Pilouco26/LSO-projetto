/**
 * LSO Project - Forza 4 (Connect 4) Multi-Client Server
 * 
 * Header file with declarations, constants, and data structures
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

// ============================================================================
// CONSTANTS
// ============================================================================

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 100
#define MAX_GAMES 50
#define MAX_USERNAME 32

// Connect 4 grid dimensions
#define GRID_ROWS 6
#define GRID_COLS 7

// Game states
typedef enum {
    GAME_CREATED,       // Just created, not yet waiting
    GAME_WAITING,       // Waiting for opponent
    GAME_IN_PROGRESS,   // Game is being played
    GAME_FINISHED       // Game has ended
} GameState;

// Player symbols
#define EMPTY '.'
#define PLAYER1 'X'
#define PLAYER2 'O'

// ============================================================================
// DATA STRUCTURES
// ============================================================================

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
    int processed;              // 0 = pending, 1 = accepted, -1 = rejected
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

// ============================================================================
// GLOBAL VARIABLES (extern declarations)
// ============================================================================

extern int server_socket;
extern Client clients[MAX_CLIENTS];
extern int client_count;
extern pthread_mutex_t clients_mutex;
extern Game games[MAX_GAMES];
extern int game_count;
extern pthread_mutex_t games_mutex;
extern volatile int server_running;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Utility functions
void send_to_client(int client_id, const char *message);
void broadcast_except(int exclude_id, const char *message);
void broadcast_all(const char *message);
Client* get_client_by_id(int client_id);
const char* get_username(int client_id);

// Connect 4 game logic
void init_grid(Game *game);
void format_grid(Game *game, char *buffer, size_t size);
int drop_piece(Game *game, int col, char piece);
int check_direction(Game *game, int row, int col, int dr, int dc, char piece);
int check_winner(Game *game, char piece);
int is_grid_full(Game *game);

// Game management
int create_game(int creator_id);
Game* get_game_by_id(int game_id);
int add_join_request(int game_id, int requester_id);
int process_join_request(int game_id, int requester_id, int accept);
int make_move(int game_id, int player_id, int column);
void cleanup_game(int game_id);
void reset_game_for_rematch(int game_id);

// Command handlers
void handle_help(Client *client);
void handle_list(Client *client);
void handle_status(Client *client);
void handle_create(Client *client);
void handle_join(Client *client, int game_id);
void handle_requests(Client *client);
void handle_accept_reject(Client *client, const char *username, int accept);
void handle_move(Client *client, int column);
void handle_grid(Client *client);
void handle_leave(Client *client);
void handle_rematch(Client *client);

// Client handler
void *handle_client(void *arg);

// Signal handler
void handle_signal(int sig);

#endif // SERVER_H

