/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#include "../server.h"

// =============================
// GAME MANAGEMENT
// =============================

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
    pthread_mutex_unlock(&games_mutex);
    init_grid(game);
    pthread_mutex_init(&game->game_mutex, NULL);
    
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
 * Process a join request
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
            if (accept) {
                req->processed = 1; 
            } else {
                req->processed = -1; 
            }
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
 * Make a move
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
 * Clean a finished game
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

