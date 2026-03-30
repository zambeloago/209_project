#include "game.h"
#include <ctype.h>
#include <string.h>

// own_grid: '.' empty water, 'S' ship not yet hit, 'X' ship hit, 'M' opponent miss on water.
// ship_id: 0 = no ship; 1..N = which ship owns the cell (set at placement, never cleared on hit).

static int next_ship_id = 1;

// True if any ship has been placed on this board (any non-zero ship_id).
static int grid_has_any_ship(const uint8_t ship_id[GRID_SIZE][GRID_SIZE]) {
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (ship_id[r][c] != 0) {
                return 1;
            }
        }
    }
    return 0;
}

void init_grid(char grid[GRID_SIZE][GRID_SIZE]) {
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            grid[r][c] = '.';
        }
    }
}

void init_ship_id(uint8_t ship_id[GRID_SIZE][GRID_SIZE]) {
    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            ship_id[r][c] = 0;
        }
    }
}

int is_valid_cell(int row, int col) {
    return row >= 0 && row < GRID_SIZE && col >= 0 && col < GRID_SIZE;
}

// Horizontal: same row, columns increase. Vertical: same column, rows increase.
int can_place_ship(char grid[GRID_SIZE][GRID_SIZE], uint8_t ship_id[GRID_SIZE][GRID_SIZE], int row, int col, char dir, int len) {
    char d = (char)toupper((unsigned char)dir);
    if (len < 1) {
        return 0;
    }
    if (d != 'H' && d != 'V') {
        return 0;
    }

    if (d == 'H') {
        if (col < 0 || col + len > GRID_SIZE) {
            return 0;
        }
        if (!is_valid_cell(row, col) || !is_valid_cell(row, col + len - 1)) {
            return 0;
        }
        for (int c = col; c < col + len; c++) {
            if (grid[row][c] != '.' || ship_id[row][c] != 0) {
                return 0;
            }
        }
    } else {
        if (row < 0 || row + len > GRID_SIZE) {
            return 0;
        }
        if (!is_valid_cell(row, col) || !is_valid_cell(row + len - 1, col)) {
            return 0;
        }
        for (int r = row; r < row + len; r++) {
            if (grid[r][col] != '.' || ship_id[r][col] != 0) {
                return 0;
            }
        }
    }
    return 1;
}

void place_ship(char grid[GRID_SIZE][GRID_SIZE], uint8_t ship_id[GRID_SIZE][GRID_SIZE], int row, int col, char dir, int len) {
    // Fresh board (no ships yet) -> start IDs at 1 again (e.g. second player's fleet).
    if (!grid_has_any_ship(ship_id)) {
        next_ship_id = 1;
    }

    if (next_ship_id < 1 || next_ship_id > 255) {
        return;
    }

    char d = (char)toupper((unsigned char)dir);
    int id = next_ship_id;
    next_ship_id++;

    uint8_t uid = (uint8_t)id;
    if (d == 'H') {
        for (int c = col; c < col + len; c++) {
            grid[row][c] = 'S';
            ship_id[row][c] = uid;
        }
    } else {
        for (int r = row; r < row + len; r++) {
            grid[r][col] = 'S';
            ship_id[r][col] = uid;
        }
    }
}

int apply_shot(Game *game, int target_idx, int row, int col) {
    if (game == NULL || target_idx < 0 || target_idx > 1) {
        return 0;
    }
    if (!is_valid_cell(row, col)) {
        return 0;
    }

    char (*grid)[GRID_SIZE] = game->p[target_idx].own_grid;
    char cell = grid[row][col];

    if (cell == '.') {
        grid[row][col] = 'M';
        return 'M';
    }

    if (cell == 'S') {
        grid[row][col] = 'X';
        // ship_id[row][col] unchanged
        return 'H';
    }

    // Already 'X' or 'M'
    return 0;
}

// Sunk iff every cell with this ship's id shows a hit ('X') on own_grid.
int is_ship_sunk(char grid[GRID_SIZE][GRID_SIZE], uint8_t ship_id[GRID_SIZE][GRID_SIZE], int row, int col) {
    if (!is_valid_cell(row, col)) {
        return 0;
    }

    uint8_t id = ship_id[row][col];
    if (id == 0) {
        return 0;
    }

    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (ship_id[r][c] != id) {
                continue;
            }
            if (grid[r][c] != 'X') {
                return 0;
            }
        }
    }
    return 1;
}

// Win when the defender has no unhit ship cells left.
int check_win(Game *game, int target_idx) {
    if (game == NULL || target_idx < 0 || target_idx > 1) {
        return 0;
    }

    for (int r = 0; r < GRID_SIZE; r++) {
        for (int c = 0; c < GRID_SIZE; c++) {
            if (game->p[target_idx].own_grid[r][c] == 'S') {
                return 0;
            }
        }
    }
    return 1;
}

void reset_game(Game *game) {
    if (game == NULL) {
        return;
    }

    next_ship_id = 1;

    for (int i = 0; i < 2; i++) {
        init_grid(game->p[i].own_grid);
        init_ship_id(game->p[i].ship_id);
        init_grid(game->p[i].shot_grid);
        game->p[i].ships_placed = 0;
        game->p[i].ships_remaining = 0;
        game->p[i].ready = 0;
        game->p[i].wants_rematch = 0;
        game->p[i].buf_len = 0;
        memset(game->p[i].buf, 0, sizeof(game->p[i].buf));
    }
    game->state = PLACING;
}
