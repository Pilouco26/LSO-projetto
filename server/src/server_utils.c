/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#include "../server.h"

// ===============================
// UTILITY FUNCTIONS
// ===============================

/**
 * Send a message to a client
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

