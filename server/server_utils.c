/**
 * LSO Project - Forza 4 (Connect 4) Multi-Client Server
 * 
 * Implementation of all server functions except main
 * 
 * Miguel Lopes - m.lopespereira@studenti.unina.it
 * Oriol Poblet - o.pobletroca@studenti.unina.it
 */

#include "server.h"

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
    return -1;
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
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (check_direction(game, r, c, 0, 1, piece)) return 1;
            if (check_direction(game, r, c, 1, 0, piece)) return 1;
            if (check_direction(game, r, c, 1, 1, piece)) return 1;
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
    int game_id = -1;
    for (int i = 0; i < MAX_GAMES; i++) {
        if (!games[i].is_active) {
            game_id = i;
            break;
        }
    }
    
    if (game_id == -1) {
        pthread_mutex_unlock(&games_mutex);
        return -1;
    }
    
    Game *game = &games[game_id];
    game->id = game_id;
    game->state = GAME_WAITING;
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
        return -2;
    }
    
    if (game->creator_id == requester_id) {
        pthread_mutex_unlock(&game->game_mutex);
        return -3;
    }
    
    JoinRequest *req = game->join_requests;
    while (req) {
        if (req->requester_id == requester_id && req->processed == 0) {
            pthread_mutex_unlock(&game->game_mutex);
            return -4;
        }
        req = req->next;
    }
    
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
    
    JoinRequest *req = game->join_requests;
    while (req) {
        if (req->requester_id == requester_id && req->processed == 0) {
            req->processed = accept ? 1 : -1;
            
            if (accept) {
                game->opponent_id = requester_id;
                game->state = GAME_IN_PROGRESS;
                game->current_turn = game->creator_id;
                
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
    return -3;
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
        return -2;
    }
    
    if (game->current_turn != player_id) {
        pthread_mutex_unlock(&game->game_mutex);
        return -3;
    }
    
    char piece = (player_id == game->creator_id) ? PLAYER1 : PLAYER2;
    int row = drop_piece(game, column, piece);
    
    if (row < 0) {
        pthread_mutex_unlock(&game->game_mutex);
        return -4;
    }
    
    if (check_winner(game, piece)) {
        game->winner_id = player_id;
        game->state = GAME_FINISHED;
    } else if (is_grid_full(game)) {
        game->winner_id = -1;
        game->state = GAME_FINISHED;
    } else {
        game->current_turn = (player_id == game->creator_id) ? game->opponent_id : game->creator_id;
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
    
    JoinRequest *req = game->join_requests;
    while (req) {
        JoinRequest *next = req->next;
        free(req);
        req = next;
    }
    game->join_requests = NULL;
    
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
    game->current_turn = (game->current_turn == game->creator_id) ? game->opponent_id : game->creator_id;
    
    pthread_mutex_unlock(&game->game_mutex);
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void handle_help(Client *client) {
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg),
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║              CONNECT 4 - AVAILABLE COMMANDS                    ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  GENERAL:                                                      ║\n"
        "║    help              - Show this message                       ║\n"
        "║    list              - List available games                    ║\n"
        "║    status            - Current player status                   ║\n"
        "║    quit              - Disconnect from server                  ║\n"
        "║                                                                ║\n"
        "║  GAME MANAGEMENT:                                              ║\n"
        "║    create            - Create a new game                       ║\n"
        "║    join <id>         - Request to join game <id>               ║\n"
        "║    requests          - View join requests                      ║\n"
        "║    accept <username> - Accept request from <username>          ║\n"
        "║    reject <username> - Reject request from <username>          ║\n"
        "║    leave             - Leave current game                      ║\n"
        "║                                                                ║\n"
        "║  DURING GAME:                                                  ║\n"
        "║    move <1-7>        - Drop piece in column 1-7                ║\n"
        "║    grid              - Show game grid                          ║\n"
        "║    rematch           - Propose/accept rematch                  ║\n"
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
        "║                      GAME LIST                                 ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n");
    ptr += written; remaining -= written;
    
    pthread_mutex_lock(&games_mutex);
    
    int found = 0;
    for (int i = 0; i < MAX_GAMES; i++) {
        if (games[i].is_active) {
            found = 1;
            const char *state_str;
            switch (games[i].state) {
                case GAME_WAITING: state_str = "Waiting"; break;
                case GAME_IN_PROGRESS: state_str = "In progress"; break;
                case GAME_FINISHED: state_str = "Finished"; break;
                default: state_str = "Created"; break;
            }
            
            const char *creator_name = get_username(games[i].creator_id);
            written = snprintf(ptr, remaining,
                "║  Game #%-3d    | Creator: %-12s  | Status: %-12s ║\n",
                games[i].id, creator_name, state_str);
            ptr += written; remaining -= written;
        }
    }
    
    pthread_mutex_unlock(&games_mutex);
    
    if (!found) {
        written = snprintf(ptr, remaining,
            "║              No games available                                ║\n");
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
            "\n[STATUS] Username: %s | You are not in any game.\n"
            "         Use 'create' to create a game or 'join <id>' to join one.\n\n",
            client->username);
    } else {
        Game *game = get_game_by_id(client->current_game_id);
        if (game) {
            const char *state_str;
            switch (game->state) {
                case GAME_WAITING: state_str = "Waiting for opponent"; break;
                case GAME_IN_PROGRESS: 
                    state_str = (game->current_turn == client->id) ? "In progress - IT'S YOUR TURN!" : "In progress - Opponent's turn";
                    break;
                case GAME_FINISHED: state_str = "Finished"; break;
                default: state_str = "Created"; break;
            }
            
            snprintf(msg, sizeof(msg),
                "\n[STATUS] Username: %s | Game #%d | %s\n\n",
                client->username, client->current_game_id, state_str);
        } else {
            client->current_game_id = -1;
            snprintf(msg, sizeof(msg),
                "\n[STATUS] Username: %s | You are not in any game.\n\n",
                client->username);
        }
    }
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_create(Client *client) {
    char msg[BUFFER_SIZE];
    if (client->current_game_id >= 0) {
        Game *current = get_game_by_id(client->current_game_id);
        if (current && current->state != GAME_FINISHED) {
            snprintf(msg, sizeof(msg),
                "\n[ERROR] You are already in an active game (Game #%d).\n"
                "        Use 'leave' to leave before creating a new one.\n\n",
                client->current_game_id);
            send(client->socket, msg, strlen(msg), 0);
            return;
        }
    }
    
    int game_id = create_game(client->id);
    
    if (game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] Cannot create game. Server is full.\n\n");
    } else {
        snprintf(msg, sizeof(msg),
            "\n╔═══════════════════════════════════════════════════════════════╗\n"
            "║                     GAME CREATED!                              ║\n"
            "╠═══════════════════════════════════════════════════════════════╣\n"
            "║  Game ID: %-3d                                                 ║\n"
            "║  Status: Waiting for an opponent...                            ║\n"
            "║                                                                ║\n"
            "║  Other players can join with: join %d                          ║\n"
            "║  Use 'requests' to see join requests                           ║\n"
            "╚═══════════════════════════════════════════════════════════════╝\n\n",
            game_id, game_id);
        
        char broadcast_msg[BUFFER_SIZE];
        snprintf(broadcast_msg, sizeof(broadcast_msg),
            "\n[NOTICE] %s created game #%d. Use 'join %d' to participate!\n\n",
            client->username, game_id, game_id);
        broadcast_except(client->id, broadcast_msg);
    }
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_join(Client *client, int game_id) {
    char msg[BUFFER_SIZE];
    if (client->current_game_id >= 0) {
        Game *current = get_game_by_id(client->current_game_id);
        if (current && current->state != GAME_FINISHED) {
            snprintf(msg, sizeof(msg),
                "\n[ERROR] You are already in an active game (Game #%d).\n\n",
                client->current_game_id);
            send(client->socket, msg, strlen(msg), 0);
            return;
        }
    }
    
    int result = add_join_request(game_id, client->id);
    
    switch (result) {
        case 0:
            snprintf(msg, sizeof(msg),
                "\n[OK] Join request sent for game #%d.\n"
                "     Waiting for the creator to accept your request...\n\n",
                game_id);
            
            Game *game = get_game_by_id(game_id);
            if (game) {
                char notify[BUFFER_SIZE];
                snprintf(notify, sizeof(notify),
                    "\n[REQUEST] %s wants to join your game #%d!\n"
                    "          Use 'accept %s' or 'reject %s'\n\n",
                    client->username, game_id, client->username, client->username);
                send_to_client(game->creator_id, notify);
            }
            break;
        case -1:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] Game #%d not found.\n\n", game_id);
            break;
        case -2:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] Game #%d is not waiting for players.\n\n", game_id);
            break;
        case -3:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] You cannot join your own game!\n\n");
            break;
        case -4:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] You have already sent a request for this game.\n\n");
            break;
        default:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] Unknown error.\n\n");
    }
    
    send(client->socket, msg, strlen(msg), 0);
}

void handle_requests(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] You have not created any game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game || game->creator_id != client->id) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] You are not the creator of this game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    pthread_mutex_lock(&game->game_mutex);
    
    char *ptr = msg;
    int remaining = sizeof(msg);
    int written;
    
    written = snprintf(ptr, remaining,
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║                    JOIN REQUESTS                               ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n");
    ptr += written; remaining -= written;
    
    int found = 0;
    JoinRequest *req = game->join_requests;
    while (req) {
        if (req->processed == 0) {
            found = 1;
            const char *requester_name = get_username(req->requester_id);
            written = snprintf(ptr, remaining,
                "║  - %s (pending)                                                \n",
                requester_name);
            ptr += written; remaining -= written;
        }
        req = req->next;
    }
    
    if (!found) {
        written = snprintf(ptr, remaining,
            "║             No pending requests                                 ║\n");
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
            "\n[ERROR] You don't have an active game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game || game->creator_id != client->id) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] You are not the creator of this game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
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
            "\n[ERROR] Player '%s' not found.\n\n", username);
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    int result = process_join_request(client->current_game_id, requester_id, accept);
    
    if (result == 0) {
        if (accept) {
            snprintf(msg, sizeof(msg),
                "\n╔═══════════════════════════════════════════════════════════════╗\n"
                "║                    THE GAME BEGINS!                            ║\n"
                "╠═══════════════════════════════════════════════════════════════╣\n"
                "║  You accepted %s into the game.                                \n"
                "║  You play with: X (first turn)                                 ║\n"
                "║  Use 'move <1-7>' to make your move!                           ║\n"
                "╚═══════════════════════════════════════════════════════════════╝\n\n",
                username);
            send(client->socket, msg, strlen(msg), 0);
            
            char grid_msg[BUFFER_SIZE];
            format_grid(game, grid_msg, sizeof(grid_msg));
            send(client->socket, grid_msg, strlen(grid_msg), 0);
            
            char opponent_msg[BUFFER_SIZE];
            snprintf(opponent_msg, sizeof(opponent_msg),
                "\n╔═══════════════════════════════════════════════════════════════╗\n"
                "║                    THE GAME BEGINS!                            ║\n"
                "╠═══════════════════════════════════════════════════════════════╣\n"
                "║  %s accepted your request!                                     \n"
                "║  You play with: O                                              ║\n"
                "║  Wait for opponent's turn...                                   ║\n"
                "╚═══════════════════════════════════════════════════════════════╝\n\n",
                client->username);
            send_to_client(requester_id, opponent_msg);
            send_to_client(requester_id, grid_msg);
            
            char broadcast_msg[BUFFER_SIZE];
            snprintf(broadcast_msg, sizeof(broadcast_msg),
                "\n[NOTICE] Game #%d between %s and %s has started!\n\n",
                client->current_game_id, client->username, username);
            broadcast_except(client->id, broadcast_msg);
        } else {
            snprintf(msg, sizeof(msg),
                "\n[OK] You rejected %s's request.\n\n", username);
            send(client->socket, msg, strlen(msg), 0);
            
            char reject_msg[BUFFER_SIZE];
            snprintf(reject_msg, sizeof(reject_msg),
                "\n[NOTICE] %s rejected your request for game #%d.\n\n",
                client->username, client->current_game_id);
            send_to_client(requester_id, reject_msg);
        }
    } else {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] Unable to process the request.\n\n");
        send(client->socket, msg, strlen(msg), 0);
    }
}

void handle_move(Client *client, int column) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] You are not in any game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] Game not found.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    int col = column - 1;
    int result = make_move(client->current_game_id, client->id, col);
    
    switch (result) {
        case 0: {
            char grid_msg[BUFFER_SIZE];
            format_grid(game, grid_msg, sizeof(grid_msg));
            
            int opponent_id = (client->id == game->creator_id) ? game->opponent_id : game->creator_id;
            
            if (game->state == GAME_FINISHED) {
                if (game->winner_id == client->id) {
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                      YOU WON! 🎉                               ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  Congratulations! You connected 4 pieces!                      ║\n"
                        "║  Use 'rematch' to propose a rematch.                           ║\n"
                        "╚═══════════════════════════════════════════════════════════════╝\n\n",
                        grid_msg);
                    send(client->socket, msg, strlen(msg), 0);
                    
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                      YOU LOST! 😢                              ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  %s connected 4 pieces.                                        \n"
                        "║  Use 'rematch' to accept a rematch.                            ║\n"
                        "╚═══════════════════════════════════════════════════════════════╝\n\n",
                        grid_msg, client->username);
                    send_to_client(opponent_id, msg);
                    
                } else if (game->winner_id == -1) {
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                        DRAW! 🤝                                ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  The grid is full! No winner.                                  ║\n"
                        "║  Use 'rematch' to propose/accept a rematch.                    ║\n"
                        "╚═══════════════════════════════════════════════════════════════╝\n\n",
                        grid_msg);
                    send(client->socket, msg, strlen(msg), 0);
                    send_to_client(opponent_id, msg);
                }
                
                const char *opponent_name = get_username(opponent_id);
                char broadcast_msg[BUFFER_SIZE];
                if (game->winner_id == -1) {
                    snprintf(broadcast_msg, sizeof(broadcast_msg),
                        "\n[NOTICE] Game #%d between %s and %s ended in a draw!\n\n",
                        game->id, client->username, opponent_name);
                } else {
                    snprintf(broadcast_msg, sizeof(broadcast_msg),
                        "\n[NOTICE] Game #%d is over! Winner: %s\n\n",
                        game->id, get_username(game->winner_id));
                }
                broadcast_except(client->id, broadcast_msg);
                
            } else {
                snprintf(msg, sizeof(msg),
                    "%s\n[OK] Move made in column %d. Wait for opponent's turn...\n\n",
                    grid_msg, column);
                send(client->socket, msg, strlen(msg), 0);
                
                snprintf(msg, sizeof(msg),
                    "%s\n[TURN] %s played in column %d. It's your turn!\n"
                    "       Use 'move <1-7>' to make your move.\n\n",
                    grid_msg, client->username, column);
                send_to_client(opponent_id, msg);
            }
            break;
        }
        case -2:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] The game is not in progress.\n\n");
            send(client->socket, msg, strlen(msg), 0);
            break;
        case -3:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] It's not your turn!\n\n");
            send(client->socket, msg, strlen(msg), 0);
            break;
        case -4:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] Column full or invalid. Choose a column from 1 to 7.\n\n");
            send(client->socket, msg, strlen(msg), 0);
            break;
        default:
            snprintf(msg, sizeof(msg),
                "\n[ERROR] Error during move.\n\n");
            send(client->socket, msg, strlen(msg), 0);
    }
}

void handle_grid(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] You are not in any game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] Game not found.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    char grid_msg[BUFFER_SIZE];
    format_grid(game, grid_msg, sizeof(grid_msg));
    send(client->socket, grid_msg, strlen(grid_msg), 0);
    
    if (game->state == GAME_IN_PROGRESS) {
        if (game->current_turn == client->id) {
            snprintf(msg, sizeof(msg), "[INFO] It's your turn! Use 'move <1-7>'.\n\n");
        } else {
            snprintf(msg, sizeof(msg), "[INFO] Wait for opponent's turn...\n\n");
        }
        send(client->socket, msg, strlen(msg), 0);
    }
}

void handle_leave(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] You are not in any game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game) {
        client->current_game_id = -1;
        snprintf(msg, sizeof(msg),
            "\n[OK] You left the game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    int game_id = client->current_game_id;
    int opponent_id = -1;
    
    pthread_mutex_lock(&game->game_mutex);
    
    if (game->state == GAME_IN_PROGRESS) {
        opponent_id = (client->id == game->creator_id) ? game->opponent_id : game->creator_id;
        game->winner_id = opponent_id;
        game->state = GAME_FINISHED;
    }
    
    pthread_mutex_unlock(&game->game_mutex);
    
    client->current_game_id = -1;
    
    snprintf(msg, sizeof(msg),
        "\n[OK] You left game #%d.\n\n", game_id);
    send(client->socket, msg, strlen(msg), 0);
    
    if (opponent_id >= 0) {
        snprintf(msg, sizeof(msg),
            "\n╔═══════════════════════════════════════════════════════════════╗\n"
            "║                      YOU WON! 🎉                               ║\n"
            "╠═══════════════════════════════════════════════════════════════╣\n"
            "║  %s left the game!                                             \n"
            "║  Victory by forfeit.                                           ║\n"
            "╚═══════════════════════════════════════════════════════════════╝\n\n",
            client->username);
        send_to_client(opponent_id, msg);
        
        char broadcast_msg[BUFFER_SIZE];
        snprintf(broadcast_msg, sizeof(broadcast_msg),
            "\n[NOTICE] Game #%d is over. %s left.\n\n",
            game_id, client->username);
        broadcast_except(client->id, broadcast_msg);
    }
    
    if (game->state == GAME_FINISHED || game->state == GAME_WAITING) {
        cleanup_game(game_id);
    }
}

void handle_rematch(Client *client) {
    char msg[BUFFER_SIZE];
    
    if (client->current_game_id < 0) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] You are not in any game.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    Game *game = get_game_by_id(client->current_game_id);
    if (!game || game->state != GAME_FINISHED) {
        snprintf(msg, sizeof(msg),
            "\n[ERROR] The game must be finished to request a rematch.\n\n");
        send(client->socket, msg, strlen(msg), 0);
        return;
    }
    
    int opponent_id = (client->id == game->creator_id) ? game->opponent_id : game->creator_id;
    
    reset_game_for_rematch(client->current_game_id);
    
    const char *first_player = get_username(game->current_turn);
    char your_symbol = (client->id == game->creator_id) ? PLAYER1 : PLAYER2;
    char opp_symbol = (client->id == game->creator_id) ? PLAYER2 : PLAYER1;
    
    snprintf(msg, sizeof(msg),
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║                    REMATCH STARTED!                            ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  The grid has been reset.                                      ║\n"
        "║  You play with: %c                                              ║\n"
        "║  First turn: %s                                                \n"
        "╚═══════════════════════════════════════════════════════════════╝\n\n",
        your_symbol, first_player);
    send(client->socket, msg, strlen(msg), 0);
    
    char grid_msg[BUFFER_SIZE];
    format_grid(game, grid_msg, sizeof(grid_msg));
    send(client->socket, grid_msg, strlen(grid_msg), 0);
    
    snprintf(msg, sizeof(msg),
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║                    REMATCH STARTED!                            ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  %s accepted the rematch!                                      \n"
        "║  You play with: %c                                              ║\n"
        "║  First turn: %s                                                \n"
        "╚═══════════════════════════════════════════════════════════════╝\n\n",
        client->username, opp_symbol, first_player);
    send_to_client(opponent_id, msg);
    send_to_client(opponent_id, grid_msg);
    
    char broadcast_msg[BUFFER_SIZE];
    snprintf(broadcast_msg, sizeof(broadcast_msg),
        "\n[NOTICE] Rematch started in game #%d!\n\n",
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
    
    printf("[SERVER] Client #%d connected from %s:%d\n",
           client->id,
           inet_ntoa(client->address.sin_addr),
           ntohs(client->address.sin_port));
    
    char welcome[] = 
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║           WELCOME TO CONNECT 4 SERVER!                         ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  Enter your username:                                          ║\n"
        "╚═══════════════════════════════════════════════════════════════╝\n\n"
        "Username: ";
    send(client->socket, welcome, strlen(welcome), 0);
    
    bytes_read = recv(client->socket, buffer, MAX_USERNAME - 1, 0);
    if (bytes_read <= 0) {
        printf("[SERVER] Client #%d disconnected during login\n", client->id);
        goto cleanup;
    }
    
    buffer[bytes_read] = '\0';
    char *newline = strchr(buffer, '\n');
    if (newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if (newline) *newline = '\0';
    
    strncpy(client->username, buffer, MAX_USERNAME - 1);
    client->username[MAX_USERNAME - 1] = '\0';
    
    printf("[SERVER] Client #%d registered as '%s'\n", client->id, client->username);
    
    char confirm_msg[BUFFER_SIZE];
    snprintf(confirm_msg, sizeof(confirm_msg),
        "\n[OK] Welcome %s! Type 'help' to see available commands.\n\n",
        client->username);
    send(client->socket, confirm_msg, strlen(confirm_msg), 0);
    
    char join_msg[BUFFER_SIZE];
    snprintf(join_msg, sizeof(join_msg),
        "\n[NOTICE] %s connected to the server.\n\n", client->username);
    broadcast_except(client->id, join_msg);
    
    while (server_running && (bytes_read = recv(client->socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        newline = strchr(buffer, '\r');
        if (newline) *newline = '\0';
        
        if (strlen(buffer) == 0) continue;
        
        printf("[SERVER] %s: %s\n", client->username, buffer);
        
        char cmd[64];
        char arg[64];
        int num_arg;
        
        if (sscanf(buffer, "%63s %63s", cmd, arg) < 1) continue;
        
        for (int i = 0; cmd[i]; i++) {
            if (cmd[i] >= 'A' && cmd[i] <= 'Z') {
                cmd[i] = cmd[i] + 32;
            }
        }
        
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
                send(client->socket, "\n[ERROR] Usage: join <game_id>\n\n", 33, 0);
            }
        }
        else if (strcmp(cmd, "requests") == 0) {
            handle_requests(client);
        }
        else if (strcmp(cmd, "accept") == 0) {
            if (strlen(arg) > 0 && strcmp(arg, cmd) != 0) {
                handle_accept_reject(client, arg, 1);
            } else {
                send(client->socket, "\n[ERROR] Usage: accept <username>\n\n", 36, 0);
            }
        }
        else if (strcmp(cmd, "reject") == 0) {
            if (strlen(arg) > 0 && strcmp(arg, cmd) != 0) {
                handle_accept_reject(client, arg, 0);
            } else {
                send(client->socket, "\n[ERROR] Usage: reject <username>\n\n", 36, 0);
            }
        }
        else if (strcmp(cmd, "move") == 0) {
            if (sscanf(buffer, "%*s %d", &num_arg) == 1 && num_arg >= 1 && num_arg <= 7) {
                handle_move(client, num_arg);
            } else {
                send(client->socket, "\n[ERROR] Usage: move <1-7>\n\n", 29, 0);
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
            send(client->socket, "\n[OK] Goodbye!\n\n", 17, 0);
            break;
        }
        else {
            char err_msg[BUFFER_SIZE];
            snprintf(err_msg, sizeof(err_msg),
                "\n[ERROR] Unknown command: %s. Type 'help' for help.\n\n", cmd);
            send(client->socket, err_msg, strlen(err_msg), 0);
        }
    }
    
cleanup:
    printf("[SERVER] Client '%s' (#%d) disconnected\n", client->username, client->id);
    
    if (client->current_game_id >= 0) {
        handle_leave(client);
    }
    
    if (client->username[0] != '\0') {
        char leave_msg[BUFFER_SIZE];
        snprintf(leave_msg, sizeof(leave_msg),
            "\n[NOTICE] %s disconnected.\n\n", client->username);
        broadcast_except(client->id, leave_msg);
    }
    
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
    printf("\n[SERVER] Server shutting down...\n");
    server_running = 0;
    if (server_socket != -1) {
        close(server_socket);
    }
    exit(0);
}
