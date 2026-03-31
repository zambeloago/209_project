#ifndef GAME_H
#define GAME_H

#include "common.h"

typedef enum { WAITING, PLACING, P1_TURN, P2_TURN, GAME_OVER } GameState;

typedef struct {
    int  fd;
    char name[NAME_LEN];
    // own_grid: '.' water, 'S' ship (unhit), 'X' ship (hit), 'M' miss
    char own_grid[GRID_SIZE][GRID_SIZE];
    // Parallel owner map: 0 = no ship, 1..N = ship id (unchanged when cell becomes 'X')
    int ship_id[GRID_SIZE][GRID_SIZE];
    char shot_grid[GRID_SIZE][GRID_SIZE];  // '.' unknown  'H' your hit  'M' your miss
    int  ships_placed;
    int  ships_remaining;
    int  ready;
    int  wants_rematch;
    char buf[BUF_SIZE];   // partial-read buffer
    int  buf_len;
} Player;

typedef struct {
    GameState state;
    Player    p[2];
} Game;

// grid
void init_grid(char grid[GRID_SIZE][GRID_SIZE]);
void init_ship_id(int ship_id[GRID_SIZE][GRID_SIZE]);
int  is_valid_cell(int row, int col);

// placement (updates own_grid and ship_id together)
int  can_place_ship(char grid[GRID_SIZE][GRID_SIZE], int ship_id[GRID_SIZE][GRID_SIZE],
                    int row, int col, char dir, int len);
void place_ship    (char grid[GRID_SIZE][GRID_SIZE], int ship_id[GRID_SIZE][GRID_SIZE],
                    int row, int col, char dir, int len);

// shots
int  apply_shot   (Game *game, int target_idx, int row, int col); // 'H', 'M', or 0
int  is_ship_sunk (char grid[GRID_SIZE][GRID_SIZE], int ship_id[GRID_SIZE][GRID_SIZE],
                   int row, int col);
int  check_win    (Game *game, int target_idx);

// state
void reset_game(Game *game);

#endif
