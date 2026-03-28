#ifndef GAME_H
#define GAME_H

#include "common.h"

/* Board cell values */
#define CELL_EMPTY  0
#define CELL_SHIP   1
#define CELL_HIT    2
#define CELL_MISS   3

#define NUM_SHIPS   5

typedef struct {
    int board[BOARD_SIZE][BOARD_SIZE]; /* 0=empty, 1=ship, 2=hit, 3=miss */
    int ships_remaining;
} player_t;

typedef struct {
    player_t players[2];
    int current_turn;   /* 0 or 1 */
    int active;         /* 0=no game, 1=game running */
} game_t;

/*
 * Initialise a new game: sets active=1, current_turn=0, places ships for
 * both players.
 */
void init_game(game_t *game);

/*
 * Randomly place NUM_SHIPS ships on a player's board with no overlaps.
 */
void place_ships(player_t *player);

/*
 * Apply a move by `player` at (row, col) on the *opponent's* board.
 * Returns:
 *   1  – hit
 *   0  – miss
 *  -1  – invalid (out-of-range or already attacked)
 */
int process_move(game_t *game, int player, int row, int col);

/*
 * Check whether the game has a winner.
 * Returns:
 *   0  – player 0 wins
 *   1  – player 1 wins
 *  -1  – no winner yet
 */
int check_win(game_t *game);

/*
 * Flip current_turn between 0 and 1.
 */
void switch_turn(game_t *game);

#endif