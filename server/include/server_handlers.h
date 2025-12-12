/**
 * LSO Project - Forza 4 (Connect 4) Multi-Client Server
 * 
 * Command handlers, client handler, and signal handler
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

// This header should be included after server.h where Client is defined
// Forward declaration for Client type
struct Client;

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

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

// ============================================================================
// CLIENT AND SIGNAL HANDLERS
// ============================================================================

void *handle_client(void *arg);
void handle_signal(int sig);

#endif // SERVER_HANDLERS_H

