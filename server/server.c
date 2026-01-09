/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#include "server.h"

int server_socket = -1;
Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

Game games[MAX_GAMES];
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int server_running = 1;


// =========================
// GAME
// ==========================

int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int port = PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].is_connected = 0;
        clients[i].socket = -1;
        clients[i].current_game_id = -1;
    }
    
    for (int i = 0; i < MAX_GAMES; i++) {
        games[i].is_active = 0;
    }
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("[SERVER] Socket creation error");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[SERVER] setsockopt error");
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[SERVER] Binding error");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("[SERVER] Listen error");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           CONNECT 4 - MULTIPLAYER SERVER                      ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║  Port: %-5d                                                   ║\n", port);
    printf("║  Waiting for connections...                                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    while (server_running) {
        int client_socket = accept(server_socket, 
                                   (struct sockaddr *)&client_addr, 
                                   &client_len);
        
        if (client_socket < 0) {
            if (server_running) {
                perror("[SERVER] Accept error");
            }
            continue;
        }
        
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
            char *full_msg = "Server full. Try again later.\n";
            send(client_socket, full_msg, strlen(full_msg), 0);
            close(client_socket);
            continue;
        }
        
        client_count++;
        clients[slot].id = client_count;
        clients[slot].socket = client_socket;
        clients[slot].is_connected = 1;
        clients[slot].current_game_id = -1;
        clients[slot].address = client_addr;
        strcpy(clients[slot].username, "");
        pthread_mutex_unlock(&clients_mutex);

        if (pthread_create(&clients[slot].thread, NULL, handle_client, &clients[slot]) != 0) {
            perror("[SERVER] Thread creation error");
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
