/**
 * LSO Project - Forza 4 
 * 
 * Miguel Lopes Pereira - m.lopespereira@studenti.unina.it
 * Oriol Poblet Roca - o.pobletroca@studenti.unina.it
 */

#include "../server.h"

// ==============================
// CONNECT 4 GAME LOGIC
// ==============================

/**
 * Initialize the grid
 */
void init_grid(Game *game) {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            game->grid[r][c] = EMPTY;
        }
    }
}

/**
 * Format the grid to string
 */
void format_grid(Game *game, char *buffer, size_t size) {
    char *ptr = buffer;
    int remaining = size;
    int written;
    
    written = snprintf(ptr, remaining, "\n  1 2 3 4 5 6 7\n");
    ptr += written; remaining -= written;
    written = snprintf(ptr, remaining, " +---------------+\n");
    ptr += written; remaining -= written;
    
    for (int r = 0; r < GRID_ROWS; r++) {
        written = snprintf(ptr, remaining, " | ");
        ptr += written; remaining -= written;
        for (int c = 0; c < GRID_COLS; c++) {
            written = snprintf(ptr, remaining, "%c ", game->grid[r][c]);
            ptr += written; remaining -= written;
        }
        written = snprintf(ptr, remaining, "|\n");
        ptr += written; remaining -= written;
    }
    written = snprintf(ptr, remaining, " +---------------+\n");
}

/**
 * Drop a piece in a column
 * Returns the row where piece is dropped
 */
int drop_piece(Game *game, int col, char piece) {
    if (col < 0 || col >= GRID_COLS) return -1;
    
    for (int r = GRID_ROWS - 1; r >= 0; r--) {
        if (game->grid[r][col] == EMPTY) {
            game->grid[r][col] = piece;
            return r;
        }
    }
    return -1;
}

/**
 * Check for a win in a direction
 */
int check_direction(Game *game, int row, int col, int dr, int dc, char piece) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        int r = row + i * dr;
        int c = col + i * dc;
        if (r < 0 || r >= GRID_ROWS || c < 0 || c >= GRID_COLS) break;
        if (game->grid[r][c] == piece) count++;
        else break;
    }
    return count >= 4;
}

/**
 * Check if a player has won
 */
int check_winner(Game *game, char piece) {
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (check_direction(game, r, c, 0, 1, piece)) return 1;
            if (check_direction(game, r, c, 1, 0, piece)) return 1;
            if (check_direction(game, r, c, 1, 1, piece)) return 1;
            if (check_direction(game, r, c, 1, -1, piece)) return 1;
        }
    }
    return 0;
}

/**
 * Check if the grid is full (in draw case)
 */
int is_grid_full(Game *game) {
    for (int c = 0; c < GRID_COLS; c++) {
        if (game->grid[0][c] == EMPTY) return 0;
    }
    return 1;
}

