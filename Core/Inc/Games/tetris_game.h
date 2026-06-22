#ifndef TETRIS_GAME_H
#define TETRIS_GAME_H

#include <stdint.h>

typedef enum {
    TETRIS_PHASE_PLAYING,
    TETRIS_PHASE_GAME_OVER
} TetrisGamePhase_t;

typedef struct {
    int8_t x;
    int8_t y;
} TetrisPoint_t;

typedef struct {
    TetrisGamePhase_t phase;
    uint16_t grid[20][10];  // 0 if empty, otherwise RGB565 color
    int8_t current_piece_type;
    int8_t current_piece_rotation;
    int8_t current_x;
    int8_t current_y;
    int8_t next_piece_type;
    uint32_t score;
    uint32_t lines_cleared;
    uint32_t level;
    float drop_timer;
    float drop_interval;
    uint8_t selected_button; // 0 for RESTART, 1 for MAIN MENU (on Game Over screen)
} TetrisGame_t;

#include "Communication/input_manager.h"

void TetrisGame_Init(TetrisGame_t *game);
void TetrisGame_Update(TetrisGame_t *game, float delta_seconds);
void TetrisGame_HandleAction(TetrisGame_t *game, InputAction_t action);
void TetrisGame_Render(TetrisGame_t *game, int16_t cursor_x, int16_t cursor_y, uint8_t show_cursor);

#endif // TETRIS_GAME_H
