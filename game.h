#ifndef GAME_H
#define GAME_H

#include "common.h"

typedef enum { WAITING, PLACING, P1_TURN, P2_TURN, GAME_OVER } GameState;

typedef struct {
    int  fd;
    char name[NAME_LEN];
    char own_grid[GRID_SIZE][GRID_SIZE];   // '.' water  'S' ship  'H' hit  'M' miss
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
int  is_valid_cell(int row, int col);

// placement
int  can_place_ship(char grid[GRID_SIZE][GRID_SIZE], int row, int col, char dir, int len);
void place_ship    (char grid[GRID_SIZE][GRID_SIZE], int row, int col, char dir, int len);

// shots
int  apply_shot   (Game *game, int target_idx, int row, int col); // 'H', 'M', or 0
int  is_ship_sunk (char grid[GRID_SIZE][GRID_SIZE], int row, int col);
int  check_win    (Game *game, int target_idx);

// state
void reset_game(Game *game);

#endif