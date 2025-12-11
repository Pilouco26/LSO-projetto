/**
 * LSO Project - Forza 4 (Connect 4) Multi-Client Server
 * 
 * A multi-threaded server for playing Connect 4.
 * Features:
 * - Multiple concurrent games
 * - Game states: CREATED -> WAITING -> IN_PROGRESS -> FINISHED
 * - Thread-safe operations with mutex synchronization
 * - Player notifications and game management
 */

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
    int current_game_id;        // Game currently playing (-1 if none)
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
    int creator_id;             // Client ID of creator
    int opponent_id;            // Client ID of opponent (-1 if none)
    int current_turn;           // Client ID of whose turn it is
    int winner_id;              // Client ID of winner (-1 if draw, 0 if ongoing)
    int is_active;              // Whether game slot is in use
    JoinRequest *join_requests; // Linked list of join requests
    pthread_mutex_t game_mutex; // Per-game mutex
} Game;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Server socket
int server_socket = -1;

// Client management
Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Game management
Game games[MAX_GAMES];
int game_count = 0;
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;

// Running flag
volatile int server_running = 1;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Send a message to a specific client
 */
void send_to_client(int client_id, const char *message) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected && clients[i].id == client_id) {
            send(clients[i].socket, message, strlen(message), 0);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/**
 * Send a message to all connected clients except one
 */
void broadcast_except(int exclude_id, const char *message) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected && clients[i].id != exclude_id) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/**
 * Send a message to all connected clients
 */
void broadcast_all(const char *message) {
    broadcast_except(-1, message);
}

/**
 * Get client by ID
 */
Client* get_client_by_id(int client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected && clients[i].id == client_id) {
            return &clients[i];
        }
    }
    return NULL;
}

/**
 * Get username by client ID
 */
const char* get_username(int client_id) {
    Client *c = get_client_by_id(client_id);
    return c ? c->username : "Unknown";
}

// ============================================================================
// CONNECT 4 GAME LOGIC
// ============================================================================

/**
 * Initialize the game grid
 */
void init_grid(Game *game) {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            game->grid[r][c] = EMPTY;
        }
    }
}

/**
 * Format the grid as a string for display
 */
void format_grid(Game *game, char *buffer, size_t size) {
    char *ptr = buffer;
    int remaining = size;
    int written;
    
    written = snprintf(ptr, remaining, "\n  1 2 3 4 5 6 7\n");
    ptr += written; remaining -= written;
    
    written = snprintf(ptr, remaining, " +---------------+\n");
    ptr += written; remaining -= written;
    
    for (int r = 0; r < GRID_ROWS; r++) {
        written = snprintf(ptr, remaining, " | ");
        ptr += written; remaining -= written;
        
        for (int c = 0; c < GRID_COLS; c++) {
            written = snprintf(ptr, remaining, "%c ", game->grid[r][c]);
            ptr += written; remaining -= written;
        }
        
        written = snprintf(ptr, remaining, "|\n");
        ptr += written; remaining -= written;
    }
    
    written = snprintf(ptr, remaining, " +---------------+\n");
}

/**
 * Drop a piece in a column
 * Returns the row where the piece landed, or -1 if column is full
 */
int drop_piece(Game *game, int col, char piece) {
    if (col < 0 || col >= GRID_COLS) return -1;
    
    for (int r = GRID_ROWS - 1; r >= 0; r--) {
        if (game->grid[r][col] == EMPTY) {
            game->grid[r][col] = piece;
            return r;
        }
    }
    return -1; // Column is full
}

/**
 * Check for a win starting from a position in a direction
 */
int check_direction(Game *game, int row, int col, int dr, int dc, char piece) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        int r = row + i * dr;
        int c = col + i * dc;
        if (r < 0 || r >= GRID_ROWS || c < 0 || c >= GRID_COLS) break;
        if (game->grid[r][c] == piece) count++;
        else break;
    }
    return count >= 4;
}

/**
 * Check if a player has won
 */
int check_winner(Game *game, char piece) {
    // Check all positions for potential wins
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            // Horizontal
            if (check_direction(game, r, c, 0, 1, piece)) return 1;
            // Vertical
            if (check_direction(game, r, c, 1, 0, piece)) return 1;
            // Diagonal down-right
            if (check_direction(game, r, c, 1, 1, piece)) return 1;
            // Diagonal down-left
            if (check_direction(game, r, c, 1, -1, piece)) return 1;
        }
    }
    return 0;
}

/**
 * Check if the grid is full (draw)
 */
int is_grid_full(Game *game) {
    for (int c = 0; c < GRID_COLS; c++) {
        if (game->grid[0][c] == EMPTY) return 0;
    }
    return 1;
}

// ============================================================================
// GAME MANAGEMENT
// ============================================================================

/**
 * Create a new game
 */
int create_game(int creator_id) {
    pthread_mutex_lock(&games_mutex);
    
    // Find free slot
    int game_id = -1;
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].is_active) {
            game_id = i;
            break;
        }
    }
    
    if (game_id == -1) {
        pthread_mutex_unlock(&games_mutex);
        return -1; // No free slots
    }
    
    Game *game = &games[game_id];
    game->id = game_id;
    game->state = GAME_WAITING;  // Immediately in waiting state
    game->creator_id = creator_id;
    game->opponent_id = -1;
    game->current_turn = creator_id;
    game->winner_id = 0;
    game->is_active = 1;
    game->join_requests = NULL;
    init_grid(game);
    pthread_mutex_init(&game->game_mutex, NULL);
    
    game_count++;
    
    pthread_mutex_unlock(&games_mutex);
    
    // Update creator's current game
    pthread_mutex_lock(&clients_mutex);
    Client *creator = get_client_by_id(creator_id);
    if (creator) {
        creator->current_game_id = game_id;
    }
    pthread_mutex_unlock(&clients_mutex);
    
    return game_id;
}

/**
 * Get game by ID
 */
Game* get_game_by_id(int game_id) {
    if (game_id < 0 || game_id >= MAX_GAMES) return NULL;
    if (!games[game_id].is_active) return NULL;
    return &games[game_id];
}

/**
 * Add a join request to a game
 */
int add_join_request(int game_id, int requester_id) {
    Game *game = get_game_by_id(game_id);
    if (!game) return -1;
    
    pthread_mutex_lock(&game->game_mutex);
    
    if (game->state != GAME_WAITING) {
        pthread_mutex_unlock(&game->game_mutex);
        return -2; // Game not in waiting state
    }
    
    if (game->creator_id == requester_id) {
        pthread_mutex_unlock(&game->game_mutex);
        return -3; // Can't join your own game
    }
    
    // Check if already requested
    JoinRequest *req = game->join_requests;
    while (req) {
        if (req->requester_id == requester_id && req->processed == 0) {
            pthread_mutex_unlock(&game->game_mutex);
            return -4; // Already requested
        }
        req = req->next;
    }
    
    // Add new request
    JoinRequest *new_req = malloc(sizeof(JoinRequest));
    new_req->requester_id = requester_id;
    new_req->processed = 0;
    new_req->next = game->join_requests;
    game->join_requests = new_req;
    
    pthread_mutex_unlock(&game->game_mutex);
    return 0;
}

/**
 * Process a join request (accept or reject)
 */
int process_join_request(int game_id, int requester_id, int accept) {
    Game *game = get_game_by_id(game_id);
    if (!game) return -1;
    
    pthread_mutex_lock(&game->game_mutex);
    
    if (game->state != GAME_WAITING) {
        pthread_mutex_unlock(&game->game_mutex);
        return -2;
    }
    
    // Find the request
    JoinRequest *req = game->join_requests;
    while (req) {
        if (req->requester_id == requester_id && req->processed == 0) {
            req->processed = accept ? 1 : -1;
            
            if (accept) {
                game->opponent_id = requester_id;
                game->state = GAME_IN_PROGRESS;
                game->current_turn = game->creator_id; // Creator goes first
                
                // Update opponent's current game
                pthread_mutex_lock(&clients_mutex);
                Client *opponent = get_client_by_id(requester_id);
                if (opponent) {
                    opponent->current_game_id = game_id;
                }
                pthread_mutex_unlock(&clients_mutex);
            }
            
            pthread_mutex_unlock(&game->game_mutex);
            return 0;
        }
        req = req->next;
    }
    
    pthread_mutex_unlock(&game->game_mutex);
    return -3; // Request not found
}

/**
 * Make a move in the game
 */
int make_move(int game_id, int player_id, int column) {
    Game *game = get_game_by_id(game_id);
    if (!game) return -1;
    
    pthread_mutex_lock(&game->game_mutex);
    
    if (game->state != GAME_IN_PROGRESS) {
        pthread_mutex_unlock(&game->game_mutex);
        return -2; // Game not in progress
    }
    
    if (game->current_turn != player_id) {
        pthread_mutex_unlock(&game->game_mutex);
        return -3; // Not your turn
    }
    
    char piece = (player_id == game->creator_id) ? PLAYER1 : PLAYER2;
    int row = drop_piece(game, column, piece);
    
    if (row < 0) {
        pthread_mutex_unlock(&game->game_mutex);
        return -4; // Invalid move (column full)
    }
    
    // Check for winner
    if (check_winner(game, piece)) {
        game->winner_id = player_id;
        game->state = GAME_FINISHED;
    } else if (is_grid_full(game)) {
        game->winner_id = -1; // Draw
        game->state = GAME_FINISHED;
    } else {
        // Switch turn
        game->current_turn = (player_id == game->creator_id) ? 
                             game->opponent_id : game->creator_id;
    }
    
    pthread_mutex_unlock(&game->game_mutex);
    return 0;
}

/**
 * Clean up a finished game
 */
void cleanup_game(int game_id) {
    Game *game = get_game_by_id(game_id);
    if (!game) return;
    
    pthread_mutex_lock(&game->game_mutex);
    
    // Free join requests
    JoinRequest *req = game->join_requests;
    while (req) {
        JoinRequest *next = req->next;
        free(req);
        req = next;
    }
    game->join_requests = NULL;
    
    // Clear player references
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].current_game_id == game_id) {
            clients[i].current_game_id = -1;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    game->is_active = 0;
    game_count--;
    
    pthread_mutex_unlock(&game->game_mutex);
}

/**
 * Reset game for rematch
 */
void reset_game_for_rematch(int game_id) {
    Game *game = get_game_by_id(game_id);
    if (!game) return;
    
    pthread_mutex_lock(&game->game_mutex);
    
    init_grid(game);
    game->state = GAME_IN_PROGRESS;
    game->winner_id = 0;
    // Swap who goes first
    game->current_turn = (game->current_turn == game->creator_id) ? 
                         game->opponent_id : game->creator_id;
    
    pthread_mutex_unlock(&game->game_mutex);
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void handle_help(Client *client) {
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg),
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║              FORZA 4 - COMANDI DISPONIBILI                     ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  GENERALI:                                                     ║\n"
        "║    help              - Mostra questo messaggio                 ║\n"
        "║    list              - Lista partite disponibili               ║\n"
        "║    status            - Stato attuale del giocatore             ║\n"
        "║    quit              - Disconnetti dal server                  ║\n"
        "║                                                                ║\n"
        "║  GESTIONE PARTITE:                                             ║\n"
        "║    create            - Crea una nuova partita                  ║\n"
        "║    join <id>         - Richiedi di unirti alla partita <id>    ║\n"
        "║    requests          - Vedi richieste di partecipazione        ║\n"
        "║    accept <username> - Accetta richiesta di <username>         ║\n"
        "║    reject <username> - Rifiuta richiesta di <username>         ║\n"
        "║    leave             - Abbandona la partita corrente           ║\n"
        "║                                                                ║\n"
        "║  DURANTE IL GIOCO:                                             ║\n"
        "║    move <1-7>        - Inserisci gettone nella colonna 1-7     ║\n"
        "║    grid              - Mostra la griglia di gioco              ║\n"
        "║    rematch           - Proponi/accetta rivincita               ║\n"
        "╚═══════════════════════════════════════════════════════════════╝\n\n");
    send(client->socket, msg, strlen(msg), 0);
}

void handle_list(Client *client) {
    char msg[BUFFER_SIZE];
    char *ptr = msg;
    int remaining = sizeof(msg);
    int written;
    
    written = snprintf(ptr, remaining,
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║                    LISTA PARTITE                               ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n");
    ptr += written; remaining -= written;
    
    pthread_mutex_lock(&games_mutex);
    
    int found = 0;
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].is_active) {
            found = 1;
            const char *state_str;
            switch (games[i].state) {
                case GAME_WAITING: state_str = "In attesa"; break;
                case GAME_IN_PROGRESS: state_str = "In corso"; break;
                case GAME_FINISHED: state_str = "Terminata"; break;
                default: state_str = "Creata"; break;
            }
            
            const char *creator_name = get_username(games[i].creator_id);
            written = snprintf(ptr, remaining,
                "║  Partita #%-3d | Creatore: %-12s | Stato: %-12s  ║\n",
                games[i].id, creator_name, state_str);
            ptr += written; remaining -= written;
        }
    }
    
    pthread_mutex_unlock(&games_mutex);
    
    if (!found) {
        written = snprintf(ptr, remaining,
            "║           Nessuna partita disponibile                          ║\n");
        ptr += written; remaining -= written;
    }
    
    snprintf(ptr, remaining,
        "╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_status(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[STATUS] Username: %s | Non sei in nessuna partita.\n"
            "         Usa 'create' per creare una partita o 'join <id>' per unirti.\n\n",
            client->username);
    } else {
        Game *game = get_game_by_id(client->current_game_id);
        if (game) {
            const char *state_str;
            switch (game->state) {
                case GAME_WAITING: state_str = "In attesa di avversario"; break;
                case GAME_IN_PROGRESS: 
                    state_str = (game->current_turn == client->id) ? 
                               "In corso - E' IL TUO TURNO!" : "In corso - Turno avversario";
                    break;
                case GAME_FINISHED: state_str = "Terminata"; break;
                default: state_str = "Creata"; break;
            }
            
            snprintf(msg, sizeof(msg),
                "\n[STATUS] Username: %s | Partita #%d | %s\n\n",
                client->username, client->current_game_id, state_str);
        } else {
            client->current_game_id = -1;
            snprintf(msg, sizeof(msg),
                "\n[STATUS] Username: %s | Non sei in nessuna partita.\n\n",
                client->username);
        }
    }
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_create(Client *client) {
    char msg[BUFFER_SIZE];
    
    // Check if already in a game
    if (client->current_game_id >= 0) {
        Game *current = get_game_by_id(client->current_game_id);
        if (current && current->state != GAME_FINISHED) {
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Sei già in una partita attiva (Partita #%d).\n"
                "         Usa 'leave' per abbandonare prima di crearne una nuova.\n\n",
                client->current_game_id);
            send(client->socket, msg, strlen(msg), 0);
            return;
        }
    }
    
    int game_id = create_game(client->id);
    
    if (game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Impossibile creare la partita. Server pieno.\n\n");
    } else {
        snprintf(msg, sizeof(msg),
            "\n╔═══════════════════════════════════════════════════════════════╗\n"
            "║                   PARTITA CREATA!                              ║\n"
            "╠═══════════════════════════════════════════════════════════════╣\n"
            "║  ID Partita: %-3d                                              ║\n"
            "║  Stato: In attesa di un avversario...                          ║\n"
            "║                                                                ║\n"
            "║  Gli altri giocatori possono unirsi con: join %d               ║\n"
            "║  Usa 'requests' per vedere le richieste di partecipazione      ║\n"
            "╚═══════════════════════════════════════════════════════════════╝\n\n",
            game_id, game_id);
        
        // Notify other players
        char broadcast_msg[BUFFER_SIZE];
        snprintf(broadcast_msg, sizeof(broadcast_msg),
            "\n[NOTIFICA] %s ha creato la partita #%d. Usa 'join %d' per partecipare!\n\n",
            client->username, game_id, game_id);
        broadcast_except(client->id, broadcast_msg);
    }
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_join(Client *client, int game_id) {
    char msg[BUFFER_SIZE];
    
    // Check if already in a game
    if (client->current_game_id >= 0) {
        Game *current = get_game_by_id(client->current_game_id);
        if (current && current->state != GAME_FINISHED) {
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Sei già in una partita attiva (Partita #%d).\n\n",
                client->current_game_id);
            send(client->socket, msg, strlen(msg), 0);
            return;
        }
    }
    
    int result = add_join_request(game_id, client->id);
    
    switch (result) {
        case 0:
            snprintf(msg, sizeof(msg),
                "\n[OK] Richiesta di partecipazione inviata per la partita #%d.\n"
                "     Attendi che il creatore accetti la tua richiesta...\n\n",
                game_id);
            
            // Notify creator
            Game *game = get_game_by_id(game_id);
            if (game) {
                char notify[BUFFER_SIZE];
                snprintf(notify, sizeof(notify),
                    "\n[RICHIESTA] %s vuole unirsi alla tua partita #%d!\n"
                    "            Usa 'accept %s' o 'reject %s'\n\n",
                    client->username, game_id, client->username, client->username);
                send_to_client(game->creator_id, notify);
            }
            break;
        case -1:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Partita #%d non trovata.\n\n", game_id);
            break;
        case -2:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] La partita #%d non è in attesa di giocatori.\n\n", game_id);
            break;
        case -3:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Non puoi unirti alla tua stessa partita!\n\n");
            break;
        case -4:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Hai già inviato una richiesta per questa partita.\n\n");
            break;
        default:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Errore sconosciuto.\n\n");
    }
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_requests(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non hai creato nessuna partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game || game->creator_id != client->id) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non sei il creatore di questa partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    pthread_mutex_lock(&game->game_mutex);
    
    char *ptr = msg;
    int remaining = sizeof(msg);
    int written;
    
    written = snprintf(ptr, remaining,
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║              RICHIESTE DI PARTECIPAZIONE                       ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n");
    ptr += written; remaining -= written;
    
    int found = 0;
    JoinRequest *req = game->join_requests;
    while (req) {
        if (req->processed == 0) {
            found = 1;
            const char *requester_name = get_username(req->requester_id);
            written = snprintf(ptr, remaining,
                "║  - %s (in attesa)                                              \n",
                requester_name);
            ptr += written; remaining -= written;
        }
        req = req->next;
    }
    
    if (!found) {
        written = snprintf(ptr, remaining,
            "║          Nessuna richiesta in sospeso                           ║\n");
        ptr += written; remaining -= written;
    }
    
    pthread_mutex_unlock(&game->game_mutex);
    
    snprintf(ptr, remaining,
        "╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_accept_reject(Client *client, const char *username, int accept) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non hai una partita attiva.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game || game->creator_id != client->id) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non sei il creatore di questa partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    // Find requester by username
    int requester_id = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected && strcmp(clients[i].username, username) == 0) {
            requester_id = clients[i].id;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    if (requester_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Giocatore '%s' non trovato.\n\n", username);
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    int result = process_join_request(client->current_game_id, requester_id, accept);
    
    if (result == 0) {
        if (accept) {
            // Notify both players and start game
            snprintf(msg, sizeof(msg),
                "\n╔═══════════════════════════════════════════════════════════════╗\n"
                "║                    LA PARTITA INIZIA!                          ║\n"
                "╠═══════════════════════════════════════════════════════════════╣\n"
                "║  Hai accettato %s nella partita.                               \n"
                "║  Tu giochi con: X (primo turno)                                ║\n"
                "║  Usa 'move <1-7>' per fare la tua mossa!                       ║\n"
                "╚═══════════════════════════════════════════════════════════════╝\n\n",
                username);
            send(client->socket, msg, strlen(msg), 0);
            
            // Send grid to creator
            char grid_msg[BUFFER_SIZE];
            format_grid(game, grid_msg, sizeof(grid_msg));
            send(client->socket, grid_msg, strlen(grid_msg), 0);
            
            // Notify opponent
            char opponent_msg[BUFFER_SIZE];
            snprintf(opponent_msg, sizeof(opponent_msg),
                "\n╔═══════════════════════════════════════════════════════════════╗\n"
                "║                    LA PARTITA INIZIA!                          ║\n"
                "╠═══════════════════════════════════════════════════════════════╣\n"
                "║  %s ha accettato la tua richiesta!                             \n"
                "║  Tu giochi con: O                                              ║\n"
                "║  Attendi il turno dell'avversario...                           ║\n"
                "╚═══════════════════════════════════════════════════════════════╝\n\n",
                client->username);
            send_to_client(requester_id, opponent_msg);
            send_to_client(requester_id, grid_msg);
            
            // Notify others that game started
            char broadcast_msg[BUFFER_SIZE];
            snprintf(broadcast_msg, sizeof(broadcast_msg),
                "\n[NOTIFICA] La partita #%d tra %s e %s è iniziata!\n\n",
                client->current_game_id, client->username, username);
            broadcast_except(client->id, broadcast_msg);
            // Don't double-notify the opponent
            
        } else {
            snprintf(msg, sizeof(msg),
                "\n[OK] Hai rifiutato la richiesta di %s.\n\n", username);
            send(client->socket, msg, strlen(msg), 0);
            
            char reject_msg[BUFFER_SIZE];
            snprintf(reject_msg, sizeof(reject_msg),
                "\n[NOTIFICA] %s ha rifiutato la tua richiesta per la partita #%d.\n\n",
                client->username, client->current_game_id);
            send_to_client(requester_id, reject_msg);
        }
    } else {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Impossibile processare la richiesta.\n\n");
        send(client->socket, msg, strlen(msg), 0);
    }
}

void handle_move(Client *client, int column) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non sei in nessuna partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Partita non trovata.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    // Convert to 0-based index
    int col = column - 1;
    
    int result = make_move(client->current_game_id, client->id, col);
    
    switch (result) {
        case 0: {
            // Move successful
            char grid_msg[BUFFER_SIZE];
            format_grid(game, grid_msg, sizeof(grid_msg));
            
            int opponent_id = (client->id == game->creator_id) ? 
                             game->opponent_id : game->creator_id;
            
            if (game->state == GAME_FINISHED) {
                // Game over
                if (game->winner_id == client->id) {
                    // Current player won
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                      HAI VINTO! 🎉                             ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  Complimenti! Hai collegato 4 gettoni!                         ║\n"
                        "║  Usa 'rematch' per proporre una rivincita.                     ║\n"
                        "╚═══════════════════════════════════════════════════════════════╝\n\n",
                        grid_msg);
                    send(client->socket, msg, strlen(msg), 0);
                    
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                      HAI PERSO! 😢                             ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  %s ha collegato 4 gettoni.                                    \n"
                        "║  Usa 'rematch' per accettare una rivincita.                    ║\n"
                        "╚═══════════════════════════════════════════════════════════════╝\n\n",
                        grid_msg, client->username);
                    send_to_client(opponent_id, msg);
                    
                } else if (game->winner_id == -1) {
                    // Draw
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                      PAREGGIO! 🤝                              ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  La griglia è piena! Nessun vincitore.                         ║\n"
                        "║  Usa 'rematch' per proporre/accettare una rivincita.           ║\n"
                        "╚═══════════════════════════════════════════════════════════════╝\n\n",
                        grid_msg);
                    send(client->socket, msg, strlen(msg), 0);
                    send_to_client(opponent_id, msg);
                }
                
                // Notify others
                const char *opponent_name = get_username(opponent_id);
                char broadcast_msg[BUFFER_SIZE];
                if (game->winner_id == -1) {
                    snprintf(broadcast_msg, sizeof(broadcast_msg),
                        "\n[NOTIFICA] La partita #%d tra %s e %s è terminata in pareggio!\n\n",
                        game->id, client->username, opponent_name);
                } else {
                    snprintf(broadcast_msg, sizeof(broadcast_msg),
                        "\n[NOTIFICA] La partita #%d è terminata! Vincitore: %s\n\n",
                        game->id, get_username(game->winner_id));
                }
                broadcast_except(client->id, broadcast_msg);
                
            } else {
                // Game continues
                snprintf(msg, sizeof(msg),
                    "%s\n[OK] Mossa effettuata nella colonna %d. Attendi il turno avversario...\n\n",
                    grid_msg, column);
                send(client->socket, msg, strlen(msg), 0);
                
                snprintf(msg, sizeof(msg),
                    "%s\n[TURNO] %s ha giocato nella colonna %d. È il tuo turno!\n"
                    "        Usa 'move <1-7>' per fare la tua mossa.\n\n",
                    grid_msg, client->username, column);
                send_to_client(opponent_id, msg);
            }
            break;
        }
        case -2:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] La partita non è in corso.\n\n");
            send(client->socket, msg, strlen(msg), 0);
            break;
        case -3:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Non è il tuo turno!\n\n");
            send(client->socket, msg, strlen(msg), 0);
            break;
        case -4:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Colonna piena o non valida. Scegli una colonna da 1 a 7.\n\n");
            send(client->socket, msg, strlen(msg), 0);
            break;
        default:
            snprintf(msg, sizeof(msg),
                "\n[ERRORE] Errore durante la mossa.\n\n");
            send(client->socket, msg, strlen(msg), 0);
    }
}

void handle_grid(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non sei in nessuna partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Partita non trovata.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    char grid_msg[BUFFER_SIZE];
    format_grid(game, grid_msg, sizeof(grid_msg));
    send(client->socket, grid_msg, strlen(grid_msg), 0);
    
    if (game->state == GAME_IN_PROGRESS) {
        if (game->current_turn == client->id) {
            snprintf(msg, sizeof(msg), "[INFO] È il tuo turno! Usa 'move <1-7>'.\n\n");
        } else {
            snprintf(msg, sizeof(msg), "[INFO] Attendi il turno dell'avversario...\n\n");
        }
        send(client->socket, msg, strlen(msg), 0);
    }
}

void handle_leave(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non sei in nessuna partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game) {
        client->current_game_id = -1;
        snprintf(msg, sizeof(msg),
            "\n[OK] Hai lasciato la partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    int game_id = client->current_game_id;
    int opponent_id = -1;
    
    pthread_mutex_lock(&game->game_mutex);
    
    if (game->state == GAME_IN_PROGRESS) {
        // Opponent wins by forfeit
        opponent_id = (client->id == game->creator_id) ? 
                     game->opponent_id : game->creator_id;
        game->winner_id = opponent_id;
        game->state = GAME_FINISHED;
    }
    
    pthread_mutex_unlock(&game->game_mutex);
    
    client->current_game_id = -1;
    
    snprintf(msg, sizeof(msg),
        "\n[OK] Hai abbandonato la partita #%d.\n\n", game_id);
    send(client->socket, msg, strlen(msg), 0);
    
    if (opponent_id >= 0) {
        snprintf(msg, sizeof(msg),
            "\n╔═══════════════════════════════════════════════════════════════╗\n"
            "║                      HAI VINTO! 🎉                             ║\n"
            "╠═══════════════════════════════════════════════════════════════╣\n"
            "║  %s ha abbandonato la partita!                                 \n"
            "║  Vittoria per abbandono.                                       ║\n"
            "╚═══════════════════════════════════════════════════════════════╝\n\n",
            client->username);
        send_to_client(opponent_id, msg);
        
        // Notify others
        char broadcast_msg[BUFFER_SIZE];
        snprintf(broadcast_msg, sizeof(broadcast_msg),
            "\n[NOTIFICA] La partita #%d è terminata. %s ha abbandonato.\n\n",
            game_id, client->username);
        broadcast_except(client->id, broadcast_msg);
    }
    
    // Cleanup if game is finished or waiting
    if (game->state == GAME_FINISHED || game->state == GAME_WAITING) {
        cleanup_game(game_id);
    }
}

void handle_rematch(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] Non sei in nessuna partita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game || game->state != GAME_FINISHED) {
        snprintf(msg, sizeof(msg),
            "\n[ERRORE] La partita deve essere terminata per richiedere una rivincita.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    int opponent_id = (client->id == game->creator_id) ? 
                     game->opponent_id : game->creator_id;
    
    // Reset game for rematch
    reset_game_for_rematch(client->current_game_id);
    
    const char *first_player = get_username(game->current_turn);
    char your_symbol = (client->id == game->creator_id) ? PLAYER1 : PLAYER2;
    char opp_symbol = (client->id == game->creator_id) ? PLAYER2 : PLAYER1;
    
    snprintf(msg, sizeof(msg),
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║                    RIVINCITA INIZIATA!                         ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  La griglia è stata resettata.                                 ║\n"
        "║  Tu giochi con: %c                                              ║\n"
        "║  Primo turno: %s                                               \n"
        "╚═══════════════════════════════════════════════════════════════╝\n\n",
        your_symbol, first_player);
    send(client->socket, msg, strlen(msg), 0);
    
    char grid_msg[BUFFER_SIZE];
    format_grid(game, grid_msg, sizeof(grid_msg));
    send(client->socket, grid_msg, strlen(grid_msg), 0);
    
    snprintf(msg, sizeof(msg),
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║                    RIVINCITA INIZIATA!                         ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  %s ha accettato la rivincita!                                 \n"
        "║  Tu giochi con: %c                                              ║\n"
        "║  Primo turno: %s                                               \n"
        "╚═══════════════════════════════════════════════════════════════╝\n\n",
        client->username, opp_symbol, first_player);
    send_to_client(opponent_id, msg);
    send_to_client(opponent_id, grid_msg);
    
    // Notify others
    char broadcast_msg[BUFFER_SIZE];
    snprintf(broadcast_msg, sizeof(broadcast_msg),
        "\n[NOTIFICA] Rivincita iniziata nella partita #%d!\n\n",
        client->current_game_id);
    broadcast_except(client->id, broadcast_msg);
}

// ============================================================================
// CLIENT HANDLER
// ============================================================================

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    printf("[SERVER] Client #%d connesso da %s:%d\n",
           client->id,
           inet_ntoa(client->address.sin_addr),
           ntohs(client->address.sin_port));
    
    // Request username
    char welcome[] = 
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║           BENVENUTO AL SERVER FORZA 4!                         ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  Inserisci il tuo username:                                    ║\n"
        "╚═══════════════════════════════════════════════════════════════╝\n\n"
        "Username: ";
    send(client->socket, welcome, strlen(welcome), 0);
    
    // Read username
    bytes_read = recv(client->socket, buffer, MAX_USERNAME - 1, 0);
    if (bytes_read <= 0) {
        printf("[SERVER] Client #%d disconnesso durante login\n", client->id);
        goto cleanup;
    }
    
    buffer[bytes_read] = '\0';
    // Remove newlines
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if (newline) *newline = '\0';
    
    strncpy(client->username, buffer, MAX_USERNAME - 1);
    client->username[MAX_USERNAME - 1] = '\0';
    
    printf("[SERVER] Client #%d registrato come '%s'\n", client->id, client->username);
    
    // Send confirmation
    char confirm_msg[BUFFER_SIZE];
    snprintf(confirm_msg, sizeof(confirm_msg),
        "\n[OK] Benvenuto %s! Digita 'help' per vedere i comandi disponibili.\n\n",
        client->username);
    send(client->socket, confirm_msg, strlen(confirm_msg), 0);
    
    // Notify others
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg),
        "\n[NOTIFICA] %s si è connesso al server.\n\n", client->username);
    broadcast_except(client->id, join_msg);
    
    // Main command loop
    while (server_running && (bytes_read = recv(client->socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Remove newlines
        newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        newline = strchr(buffer, '\r');
        if (newline) *newline = '\0';
        
        // Skip empty commands
        if (strlen(buffer) == 0) continue;
        
        printf("[SERVER] %s: %s\n", client->username, buffer);
        
        // Parse command
        char cmd[64];
        char arg[64];
        int num_arg;
        
        if (sscanf(buffer, "%63s %63s", cmd, arg) < 1) continue;
        
        // Convert command to lowercase
        for (int i = 0; cmd[i]; i++) {
            if (cmd[i] >= 'A' && cmd[i] <= 'Z') {
                cmd[i] = cmd[i] + 32;
            }
        }
        
        // Handle commands
        if (strcmp(cmd, "help") == 0) {
            handle_help(client);
        }
        else if (strcmp(cmd, "list") == 0) {
            handle_list(client);
        }
        else if (strcmp(cmd, "status") == 0) {
            handle_status(client);
        }
        else if (strcmp(cmd, "create") == 0) {
            handle_create(client);
        }
        else if (strcmp(cmd, "join") == 0) {
            if (sscanf(buffer, "%*s %d", &num_arg) == 1) {
                handle_join(client, num_arg);
            } else {
                send(client->socket, "\n[ERRORE] Uso: join <id_partita>\n\n", 35, 0);
            }
        }
        else if (strcmp(cmd, "requests") == 0) {
            handle_requests(client);
        }
        else if (strcmp(cmd, "accept") == 0) {
            if (strlen(arg) > 0 && strcmp(arg, cmd) != 0) {
                handle_accept_reject(client, arg, 1);
            } else {
                send(client->socket, "\n[ERRORE] Uso: accept <username>\n\n", 35, 0);
            }
        }
        else if (strcmp(cmd, "reject") == 0) {
            if (strlen(arg) > 0 && strcmp(arg, cmd) != 0) {
                handle_accept_reject(client, arg, 0);
            } else {
                send(client->socket, "\n[ERRORE] Uso: reject <username>\n\n", 35, 0);
            }
        }
        else if (strcmp(cmd, "move") == 0) {
            if (sscanf(buffer, "%*s %d", &num_arg) == 1 && num_arg >= 1 && num_arg <= 7) {
                handle_move(client, num_arg);
            } else {
                send(client->socket, "\n[ERRORE] Uso: move <1-7>\n\n", 28, 0);
            }
        }
        else if (strcmp(cmd, "grid") == 0) {
            handle_grid(client);
        }
        else if (strcmp(cmd, "leave") == 0) {
            handle_leave(client);
        }
        else if (strcmp(cmd, "rematch") == 0) {
            handle_rematch(client);
        }
        else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            send(client->socket, "\n[OK] Arrivederci!\n\n", 21, 0);
            break;
        }
        else {
            char err_msg[BUFFER_SIZE];
            snprintf(err_msg, sizeof(err_msg),
                "\n[ERRORE] Comando sconosciuto: %s. Digita 'help' per aiuto.\n\n", cmd);
            send(client->socket, err_msg, strlen(err_msg), 0);
        }
    }
    
cleanup:
    // Handle disconnect
    printf("[SERVER] Client '%s' (#%d) disconnesso\n", client->username, client->id);
    
    // Leave current game if any
    if (client->current_game_id >= 0) {
        handle_leave(client);
    }
    
    // Notify others
    char leave_msg[BUFFER_SIZE];
    snprintf(leave_msg, sizeof(leave_msg),
        "\n[NOTIFICA] %s si è disconnesso.\n\n", client->username);
    broadcast_except(client->id, leave_msg);
    
    // Cleanup client
    pthread_mutex_lock(&clients_mutex);
    close(client->socket);
    client->is_connected = 0;
    client->socket = -1;
    pthread_mutex_unlock(&clients_mutex);
    
    return NULL;
}

// ============================================================================
// SIGNAL HANDLER
// ============================================================================

void handle_signal(int sig) {
    printf("\n[SERVER] Arresto del server in corso...\n");
    server_running = 0;
    if (server_socket != -1) {
        close(server_socket);
    }
    exit(0);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port = PORT;
    
    // Parse port argument
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    // Initialize client array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].is_connected = 0;
        clients[i].socket = -1;
        clients[i].current_game_id = -1;
    }
    
    // Initialize games array
    for (int i = 0; i < MAX_GAMES; i++) {
        games[i].is_active = 0;
    }
    
    // Setup signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("[SERVER] Errore creazione socket");
        exit(EXIT_FAILURE);
    }
    
    // Allow socket reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[SERVER] Errore setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // Configure address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[SERVER] Errore binding");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    // Listen
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("[SERVER] Errore listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           FORZA 4 - SERVER MULTIGIOCATORE                     ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Porta: %-5d                                                 ║\n", port);
    printf("║  In attesa di connessioni...                                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    // Accept loop
    while (server_running) {
        int client_socket = accept(server_socket, 
                                   (struct sockaddr *)&client_addr, 
                                   &client_len);
        
        if (client_socket < 0) {
            if (server_running) {
                perror("[SERVER] Errore accept");
            }
            continue;
        }
        
        // Find free client slot
        pthread_mutex_lock(&clients_mutex);
        
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].is_connected) {
                slot = i;
                break;
            }
        }
        
        if (slot < 0) {
            pthread_mutex_unlock(&clients_mutex);
            char *full_msg = "Server pieno. Riprova più tardi.\n";
            send(client_socket, full_msg, strlen(full_msg), 0);
            close(client_socket);
            continue;
        }
        
        // Initialize client
        client_count++;
        clients[slot].id = client_count;
        clients[slot].socket = client_socket;
        clients[slot].is_connected = 1;
        clients[slot].current_game_id = -1;
        clients[slot].address = client_addr;
        strcpy(clients[slot].username, "");
        
        pthread_mutex_unlock(&clients_mutex);
        
        // Create handler thread
        if (pthread_create(&clients[slot].thread, NULL, handle_client, &clients[slot]) != 0) {
            perror("[SERVER] Errore creazione thread");
            pthread_mutex_lock(&clients_mutex);
            clients[slot].is_connected = 0;
            close(client_socket);
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        
        pthread_detach(clients[slot].thread);
    }
    
    close(server_socket);
    return 0;
}
