#include <stdlib.h>
#include <time.h>
#include "game.h"

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static int rand_seeded = 0;

static void seed_rand(void) {
    if (!rand_seeded) {
        srand((unsigned int)time(NULL));
        rand_seeded = 1;
    }
}

/* ------------------------------------------------------------------ */
/* Public functions                                                     */
/* ------------------------------------------------------------------ */

void place_ships(player_t *player) {
    int r, c, i;
    seed_rand();

    /* Zero the board */
    for (r = 0; r < BOARD_SIZE; r++)
        for (c = 0; c < BOARD_SIZE; c++)
            player->board[r][c] = CELL_EMPTY;

    player->ships_remaining = NUM_SHIPS;

    /* Place NUM_SHIPS single-cell ships, no overlaps */
    int placed = 0;
    while (placed < NUM_SHIPS) {
        r = rand() % BOARD_SIZE;
        c = rand() % BOARD_SIZE;
        if (player->board[r][c] == CELL_EMPTY) {
            player->board[r][c] = CELL_SHIP;
            placed++;
        }
    }
    (void)i; /* suppress unused warning */
}

void init_game(game_t *game) {
    game->active       = 1;
    game->current_turn = 0;
    place_ships(&game->players[0]);
    place_ships(&game->players[1]);
}

int process_move(game_t *game, int player, int row, int col) {
    /* The attacking player fires at the *opponent's* board */
    int opponent = 1 - player;
    player_t *target = &game->players[opponent];

    /* Range check */
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE)
        return -1;

    /* Already attacked this cell */
    if (target->board[row][col] == CELL_HIT ||
        target->board[row][col] == CELL_MISS)
        return -1;

    if (target->board[row][col] == CELL_SHIP) {
        target->board[row][col] = CELL_HIT;
        target->ships_remaining--;
        return 1; /* hit */
    }

    /* CELL_EMPTY */
    target->board[row][col] = CELL_MISS;
    return 0; /* miss */
}

int check_win(game_t *game) {
    if (game->players[0].ships_remaining == 0)
        return 1; /* player 1 wins */
    if (game->players[1].ships_remaining == 0)
        return 0; /* player 0 wins */
    return -1;    /* no winner yet */
}

void switch_turn(game_t *game) {
    game->current_turn = 1 - game->current_turn;
}