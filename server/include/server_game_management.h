/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_GAME_MANAGEMENT_H
#define SERVER_GAME_MANAGEMENT_H

// Full definition in server.h
struct Game;

int create_game(int creator_id);
struct Game* get_game_by_id(int game_id);
int add_join_request(int game_id, int requester_id);
int process_join_request(int game_id, int requester_id, int accept);
int make_move(int game_id, int player_id, int column);
void cleanup_game(int game_id);
void reset_game_for_rematch(int game_id);

#endif 

