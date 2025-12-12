/**
 * LSO Project - Forza 4 (Connect 4) Multi-Client Server
 * 
 * Connect 4 game logic functions
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#ifndef SERVER_GAME_LOGIC_H
#define SERVER_GAME_LOGIC_H

// This header should be included after server.h where Game is defined
// Forward declaration for Game type
struct Game;

// ============================================================================
// CONNECT 4 GAME LOGIC
// ============================================================================

void init_grid(Game *game);
void format_grid(Game *game, char *buffer, size_t size);
int drop_piece(Game *game, int col, char piece);
int check_direction(Game *game, int row, int col, int dr, int dc, char piece);
int check_winner(Game *game, char piece);
int is_grid_full(Game *game);

#endif // SERVER_GAME_LOGIC_H

