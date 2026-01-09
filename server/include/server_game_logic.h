/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_GAME_LOGIC_H
#define SERVER_GAME_LOGIC_H

// Full definition in server.h
struct Game;

void init_grid(struct Game *game);
void format_grid(struct Game *game, char *buffer, size_t size);
int drop_piece(struct Game *game, int col, char piece);
int check_direction(struct Game *game, int row, int col, int dr, int dc, char piece);
int check_winner(struct Game *game, char piece);
int is_grid_full(struct Game *game);

#endif 

