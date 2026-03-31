#ifndef GAME_H
#define GAME_H

#include "common.h"

typedef enum { WAITING, PLACING, P1_TURN, P2_TURN, GAME_OVER } GameState;

typedef struct {
    int  fd;
    char name[NAME_LEN];  // Set by LOGIN only; not used by game logic in server.c (optional nickname).
    // Grids use: '.' water, 'S' ship (unhit), 'X' hit ship, 'M' miss (same on own_grid and shot_grid).
    char own_grid[GRID_SIZE][GRID_SIZE];
    // Parallel owner map: 0 = no ship, 1..N = ship id (unchanged when cell becomes 'X')
    int ship_id[GRID_SIZE][GRID_SIZE];
    // shot_grid: '.' not yet fired, 'X' hit on opponent, 'M' miss on opponent
    char shot_grid[GRID_SIZE][GRID_SIZE];
    int  ships_placed;
    // Fleet {2,3,3,4,5}
    int  ships_left_by_len[6];
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
int  apply_shot   (Game *game, int target_idx, int row, int col); // 'X' hit, 'M' miss, 0 invalid
int  is_ship_sunk (char grid[GRID_SIZE][GRID_SIZE], int ship_id[GRID_SIZE][GRID_SIZE],
                   int row, int col);
int  check_win    (Game *game, int target_idx);

// state
void reset_game(Game *game);

#endif
