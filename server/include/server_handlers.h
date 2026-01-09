/**
 * LSO Project - Forza 4 (Connect 4) Multi-Client Server
 * 
 * Command handlers, client handler, and signal handler
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

struct Client;

void handle_help(struct Client *client);
void handle_list(struct Client *client);
void handle_status(struct Client *client);
void handle_create(struct Client *client);
void handle_join(struct Client *client, int game_id);
void handle_requests(struct Client *client);
void handle_accept_reject(struct Client *client, const char *username, int accept);
void handle_move(struct Client *client, int column);
void handle_grid(struct Client *client);
void handle_leave(struct Client *client);
void handle_rematch(struct Client *client);
void *handle_client(void *arg);
void handle_signal(int sig);

#endif

