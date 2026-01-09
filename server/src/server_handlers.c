/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#include "../server.h"

// =============================
// COMMAND HANDLERS
// =============================

void handle_help(Client *client) {
    char msg[BUFFER_SIZE];
    snprintf(msg, sizeof(msg),
        "\n╔═════════════════════════════════════════════════════════════════╗\n"
        "║              CONNECT 4 - AVAILABLE COMMANDS                    ║\n"
        "╠════════════════════════════════════════════════════════════════╣\n"
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
        "╚════════════════════════════════════════════════════════════════╝\n\n");
    send(client->socket, msg, strlen(msg), 0);
}

void handle_list(Client *client) {
    char msg[BUFFER_SIZE];
    char *ptr = msg;
    int remaining = sizeof(msg);
    int written;
    
    written = snprintf(ptr, remaining,
        "\n╔═══════════════════════════════════════════════════════════════╗\n"
        "║                      GAME LIST                                ║\n"
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
                "║  Game #%-3d  |  Creator: %-12s |  Status: %-12s  ║\n",
                games[i].id, creator_name, state_str);
            ptr += written; remaining -= written;
        }
    }
    
    pthread_mutex_unlock(&games_mutex);
    
    if (!found) {
        written = snprintf(ptr, remaining,
            "║             No games available                                 ║\n");
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
            "           Use 'create' to create a game or 'join <id>' to join one.\n\n",
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
                "           Use 'leave' to leave before creating a new one.\n\n",
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
                    "           Use 'accept %s' or 'reject %s'\n\n",
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
                    pthread_mutex_lock(&game->game_mutex);
                    int old_creator = game->creator_id;
                    int old_opponent = game->opponent_id;
                    game->creator_id = game->winner_id;
                    game->opponent_id = (old_creator == game->winner_id) ? old_opponent : old_creator;
                    pthread_mutex_unlock(&game->game_mutex);
                    
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                      YOU WON! 🎉                               ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  Congratulations! You connected 4 pieces!                      ║\n"
                        "║  You are now the game creator.                                  ║\n"
                        "║  Use 'rematch' to propose a rematch to your opponent.           ║\n"
                        "╚═══════════════════════════════════════════════════════════════╝\n\n",
                        grid_msg);
                    send(client->socket, msg, strlen(msg), 0);
                    
                    snprintf(msg, sizeof(msg),
                        "%s\n"
                        "╔═══════════════════════════════════════════════════════════════╗\n"
                        "║                      YOU LOST! 😢                              ║\n"
                        "╠═══════════════════════════════════════════════════════════════╣\n"
                        "║  %s connected 4 pieces.                                        \n"
                        "║  You must leave the game.                                       ║\n"
                        "║  You can only stay if the winner proposes a rematch.            ║\n"
                        "║  Use 'leave' to exit the game.                                  ║\n"
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
    
    if (game->winner_id != -1) {
        if (client->id != game->creator_id) {
            snprintf(msg, sizeof(msg),
                "\n[ERROR] Only the winner can propose a rematch.\n"
                "           You must leave the game. Use 'leave' to exit.\n\n");
            send(client->socket, msg, strlen(msg), 0);
            return;
        }
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

// ===========================
// CLIENT HANDLER
// ===========================

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
        "║           WELCOME TO CONNECT 4 SERVER!                        ║\n"
        "╠═══════════════════════════════════════════════════════════════╣\n"
        "║  Enter your username:                                         ║\n"
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

// ===========================
// SIGNAL HANDLER
// ===========================

void handle_signal(int sig) {
    printf("\n[SERVER] Server shutting down...\n");
    server_running = 0;
    if (server_socket != -1) {
        close(server_socket);
    }
    exit(0);
}

