/**
 * LSO Project - Forza 4 (Connect 4) Multi-Client Server
 * 
 * Utility functions for client communication and management
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_UTILS_H
#define SERVER_UTILS_H

// This header should be included after server.h where Client is defined
// Forward declaration for Client type
struct Client;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void send_to_client(int client_id, const char *message);
void broadcast_except(int exclude_id, const char *message);
void broadcast_all(const char *message);
Client* get_client_by_id(int client_id);
const char* get_username(int client_id);

#endif // SERVER_UTILS_H

