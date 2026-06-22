#include "Games/app_state_manager.h"
#include "Display/fonts.h"
#include "Display/ILI9341_STM32_Driver.h"
#include "Display/ILI9341_GFX.h"
#include "Graphics/graphics_2d.h"
#include "Graphics/graphics_3d.h"
#include "Games/stack_game.h"
#include "Games/tetris_game.h"
#include <stdio.h>
#include <stdlib.h>

#define COLOR_BACKGROUND  0x0862  
#define COLOR_ACCENT_1    0x07E0  
#define COLOR_ACCENT_2    0xF8E0  
#define COLOR_GRID        0x786F  
#define COLOR_WHITE       0xFFFF
#define COLOR_LIGHTGREY   0xC618
#define COLOR_DARKGREY    0x7BEF
#define COLOR_RED         0xF800
#define COLOR_ORANGE      0xFD20
#define COLOR_YELLOW      0xFFE0

typedef enum {
    STATE_MAIN_MENU,
    STATE_STACK3D,
    STATE_TETRIS
} AppState_t;

static AppState_t app_state = STATE_MAIN_MENU;
static Mesh_t cube_mesh;
static StackGame_t stack_game;
static TetrisGame_t tetris_game;

int16_t cursor_x = 160;
int16_t cursor_y = 120;
uint8_t show_cursor = 1;

static void DrawChunkCursor(uint16_t *buffer, int y_start, int chunk_height, int16_t cx, int16_t cy, uint16_t color)
{
    for (int16_t dy = -1; dy <= 1; dy++) {
        int16_t sy = cy + dy;
        if (sy < y_start || sy >= y_start + chunk_height) continue;
        for (int16_t dx = -1; dx <= 1; dx++) {
            int16_t sx = cx + dx;
            if (sx < 0 || sx >= GFX_WIDTH) continue;
            buffer[(sy - y_start) * GFX_WIDTH + sx] = color;
        }
    }
    
    int x = 6;
    int y = 0;
    int err = 0;
    while (x >= y) {
        int pts[8][2] = {
            {cx + x, cy + y}, {cx + y, cy + x},
            {cx - y, cy + x}, {cx - x, cy + y},
            {cx - x, cy - y}, {cx - y, cy - x},
            {cx + y, cy - x}, {cx + x, cy - y}
        };
        for (int i = 0; i < 8; i++) {
            int px = pts[i][0];
            int py = pts[i][1];
            if (px >= 0 && px < GFX_WIDTH && py >= y_start && py < y_start + chunk_height) {
                buffer[(py - y_start) * GFX_WIDTH + px] = color;
            }
        }
        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static void Render_MainMenu(void)
{
    for (int chunk = 0; chunk < (GFX_HEIGHT / CHUNK_HEIGHT); chunk++) {
        int y_start = chunk * CHUNK_HEIGHT;

        for (int i = 0; i < GFX_WIDTH * CHUNK_HEIGHT; i++) {
            chunk_color_buffer[i] = COLOR_BACKGROUND;
        }

        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "STM32 RETRO PLAY", FONT4, 28, 25, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 28, 48, 264, 2, COLOR_GRID);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "Choose a game to play", FONT1, 95, 55, COLOR_YELLOW, COLOR_BACKGROUND);

        uint16_t stack_card_color = (cursor_x >= 30 && cursor_x <= 140 && cursor_y >= 90 && cursor_y <= 170) ? COLOR_ACCENT_1 : COLOR_GRID;
        uint16_t tetris_card_color = (cursor_x >= 180 && cursor_x <= 290 && cursor_y >= 90 && cursor_y <= 170) ? COLOR_ACCENT_1 : COLOR_GRID;

        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 30, 90, 110, 80, stack_card_color);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 32, 92, 106, 76, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "STACK 3D", FONT2, 50, 120, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "3D Physics", FONT1, 55, 140, COLOR_DARKGREY, COLOR_BACKGROUND);

        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 180, 90, 110, 80, tetris_card_color);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 182, 92, 106, 76, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "TETRIS", FONT2, 208, 120, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "Classic 2D", FONT1, 203, 140, COLOR_DARKGREY, COLOR_BACKGROUND);

        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "Move cursor with Bluetooth serial (u/d/l/r)", FONT1, 35, 195, COLOR_DARKGREY, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "Press OK (1) to select", FONT1, 95, 210, COLOR_WHITE, COLOR_BACKGROUND);

        if (show_cursor) {
            DrawChunkCursor(chunk_color_buffer, y_start, CHUNK_HEIGHT, cursor_x, cursor_y, COLOR_YELLOW);
        }

        ILI9341_SendPixelBuffer(0, y_start, GFX_WIDTH, CHUNK_HEIGHT, chunk_color_buffer);
    }
}

void AppStateManager_Init(void)
{
    app_state = STATE_MAIN_MENU;
    cursor_x = 160;
    cursor_y = 120;
    show_cursor = 1;
    cube_mesh = GFX3D_CreateCube(1.0f, COLOR_WHITE);
    StackGame_Init(&stack_game);
}

void AppStateManager_Update(float delta_seconds)
{
    if (app_state == STATE_STACK3D) {
        StackGame_Update(&stack_game, delta_seconds);
    } else if (app_state == STATE_TETRIS) {
        TetrisGame_Update(&tetris_game, delta_seconds);
    }
}

void AppStateManager_Render(void)
{
    if (app_state == STATE_MAIN_MENU) {
        Render_MainMenu();
    } else if (app_state == STATE_STACK3D) {
        StackGame_Render(&stack_game, &cube_mesh);
    } else if (app_state == STATE_TETRIS) {
        TetrisGame_Render(&tetris_game, cursor_x, cursor_y, show_cursor);
    }
}

void AppStateManager_HandleAction(InputAction_t action)
{
    if (action == INPUT_ACTION_NONE) return;

    if (app_state == STATE_MAIN_MENU) {
        if (action == INPUT_ACTION_MOVE_UP) cursor_y = (cursor_y >= 15) ? (cursor_y - 15) : 0;
        else if (action == INPUT_ACTION_MOVE_DOWN) cursor_y = (cursor_y <= 240 - 1 - 15) ? (cursor_y + 15) : (240 - 1);
        else if (action == INPUT_ACTION_MOVE_LEFT) cursor_x = (cursor_x >= 15) ? (cursor_x - 15) : 0;
        else if (action == INPUT_ACTION_MOVE_RIGHT) cursor_x = (cursor_x <= 320 - 1 - 15) ? (cursor_x + 15) : (320 - 1);
        else if (action == INPUT_ACTION_SELECT) {
            if (cursor_x >= 30 && cursor_x <= 140 && cursor_y >= 90 && cursor_y <= 170) {
                StackGame_Init(&stack_game);
                StackGame_HandlePress(&stack_game);
                app_state = STATE_STACK3D;
                show_cursor = 0;
            } else if (cursor_x >= 180 && cursor_x <= 290 && cursor_y >= 90 && cursor_y <= 170) {
                TetrisGame_Init(&tetris_game);
                app_state = STATE_TETRIS;
                show_cursor = 0;
            }
        }
    } else if (app_state == STATE_STACK3D) {
        if (stack_game.phase == STACK_PHASE_TITLE) {
            if (action == INPUT_ACTION_SELECT) {
                StackGame_HandlePress(&stack_game);
                show_cursor = 0;
            }
        } else if (stack_game.phase == STACK_PHASE_PLAYING) {
            if (action == INPUT_ACTION_SELECT) {
                StackGame_HandlePress(&stack_game);
            }
        } else if (stack_game.phase == STACK_PHASE_GAME_OVER) {
            show_cursor = 1;
            if (action == INPUT_ACTION_MOVE_UP) cursor_y = (cursor_y >= 15) ? (cursor_y - 15) : 0;
            else if (action == INPUT_ACTION_MOVE_DOWN) cursor_y = (cursor_y <= 240 - 1 - 15) ? (cursor_y + 15) : (240 - 1);
            else if (action == INPUT_ACTION_MOVE_LEFT) cursor_x = (cursor_x >= 15) ? (cursor_x - 15) : 0;
            else if (action == INPUT_ACTION_MOVE_RIGHT) cursor_x = (cursor_x <= 320 - 1 - 15) ? (cursor_x + 15) : (320 - 1);
            else if (action == INPUT_ACTION_SELECT) {
                if (cursor_x >= 60 && cursor_x <= 150 && cursor_y >= 135 && cursor_y <= 160) {
                    StackGame_Init(&stack_game);
                    StackGame_HandlePress(&stack_game);
                    show_cursor = 0;
                } else if (cursor_x >= 170 && cursor_x <= 260 && cursor_y >= 135 && cursor_y <= 160) {
                    app_state = STATE_MAIN_MENU;
                    show_cursor = 1;
                }
            }
        }
    } else if (app_state == STATE_TETRIS) {
        if (tetris_game.phase == TETRIS_PHASE_PLAYING) {
            TetrisGame_HandleAction(&tetris_game, action);
        } else if (tetris_game.phase == TETRIS_PHASE_GAME_OVER) {
            show_cursor = 1;
            if (action == INPUT_ACTION_MOVE_UP) cursor_y = (cursor_y >= 15) ? (cursor_y - 15) : 0;
            else if (action == INPUT_ACTION_MOVE_DOWN) cursor_y = (cursor_y <= 240 - 1 - 15) ? (cursor_y + 15) : (240 - 1);
            else if (action == INPUT_ACTION_MOVE_LEFT) cursor_x = (cursor_x >= 15) ? (cursor_x - 15) : 0;
            else if (action == INPUT_ACTION_MOVE_RIGHT) cursor_x = (cursor_x <= 320 - 1 - 15) ? (cursor_x + 15) : (320 - 1);
            else if (action == INPUT_ACTION_SELECT) {
                if (cursor_x >= 55 && cursor_x <= 155 && cursor_y >= 130 && cursor_y <= 160) {
                    TetrisGame_Init(&tetris_game);
                    show_cursor = 0;
                } else if (cursor_x >= 165 && cursor_x <= 265 && cursor_y >= 130 && cursor_y <= 160) {
                    app_state = STATE_MAIN_MENU;
                    show_cursor = 1;
                }
            }
        }
    }
}
