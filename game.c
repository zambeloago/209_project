#include "game.h"

void init_grid(char grid[GRID_SIZE][GRID_SIZE]) {
    // TODO: fill every cell with '.'
}

int is_valid_cell(int row, int col) {
    // TODO: return 1 if row and col are both in [0, GRID_SIZE)
    return 0;
}

int can_place_ship(char grid[GRID_SIZE][GRID_SIZE], int row, int col, char dir, int len) {
    // TODO: for each cell the ship occupies, check is_valid_cell() and grid[r][c] == '.'
    // TODO: return 1 if all clear, 0 otherwise
    return 0;
}

void place_ship(char grid[GRID_SIZE][GRID_SIZE], int row, int col, char dir, int len) {
    // TODO: for each cell the ship occupies, set grid[r][c] = 'S'
}

int apply_shot(Game *game, int target_idx, int row, int col) {
    // TODO: if cell is 'S': mark 'H', decrement ships_remaining, return 'H'
    // TODO: if cell is '.': mark 'M', return 'M'
    // TODO: otherwise return 0 (already shot)
    return 0;
}

int is_ship_sunk(char grid[GRID_SIZE][GRID_SIZE], int row, int col) {
    // TODO: expand left+right from col to find full horizontal ship extent
    // TODO: if extent > 1 cell: check all are 'H', return result
    // TODO: else expand up+down from row, check all are 'H', return result
    return 0;
}

int check_win(Game *game, int target_idx) {
    // TODO: return 1 if target's ships_remaining == 0
    return 0;
}

void reset_game(Game *game) {
    // TODO: for both players: init both grids, reset ships_placed/ships_remaining/ready/wants_rematch
    // TODO: game->state = PLACING
}
