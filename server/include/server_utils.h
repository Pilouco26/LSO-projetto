/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

// Full definition in server.h
struct Client;

void send_to_client(int client_id, const char *message);
void broadcast_except(int exclude_id, const char *message);
void broadcast_all(const char *message);
struct Client* get_client_by_id(int client_id);
const char* get_username(int client_id);

#endif

