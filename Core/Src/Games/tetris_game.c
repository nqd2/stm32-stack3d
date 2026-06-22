#include "Games/tetris_game.h"
#include "Graphics/graphics_2d.h"
#include "Graphics/graphics_3d.h"
#include "Display/fonts.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define COLOR_BACKGROUND  0x0862  // #0b0c10
#define COLOR_BORDER      0x786F  // Neon border (magenta/purple)
#define COLOR_GRID_LINE   0x18C3  // Dark grid line
#define COLOR_WHITE       0xFFFF
#define COLOR_DARKGREY    0x7BEF
#define COLOR_YELLOW      0xFFE0
#define COLOR_ACCENT_1    0x07E0  // Cyan/Green accent

// Tetris board geometry
#define BOARD_OFFSET_X    110
#define BOARD_OFFSET_Y    20
#define BLOCK_SIZE        10
#define BOARD_COLS        10
#define BOARD_ROWS        20

// Tetromino colors matching standard types
static const uint16_t tetris_colors[] = {
    0x0000, // 0: Empty
    0x07FF, // 1: Cyan (I)
    0xFFE0, // 2: Yellow (O)
    0x801F, // 3: Purple (T)
    0x07E0, // 4: Green (S)
    0xF800, // 5: Red (Z)
    0x001F, // 6: Blue (J)
    0xFD20  // 7: Orange (L)
};

// Tetromino shapes relative to pivot block
static const TetrisPoint_t tetrominoes[7][4][4] = {
    // 0: I-piece (Cyan)
    {
        {{-1, 0}, {0, 0}, {1, 0}, {2, 0}},
        {{0, -1}, {0, 0}, {0, 1}, {0, 2}},
        {{-1, 0}, {0, 0}, {1, 0}, {2, 0}},
        {{0, -1}, {0, 0}, {0, 1}, {0, 2}}
    },
    // 1: O-piece (Yellow)
    {
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}}
    },
    // 2: T-piece (Purple)
    {
        {{-1, 0}, {0, 0}, {1, 0}, {0, 1}},
        {{0, -1}, {0, 0}, {0, 1}, {-1, 0}},
        {{-1, 0}, {0, 0}, {1, 0}, {0, -1}},
        {{0, -1}, {0, 0}, {0, 1}, {1, 0}}
    },
    // 3: S-piece (Green)
    {
        {{0, 0}, {1, 0}, {-1, 1}, {0, 1}},
        {{0, -1}, {0, 0}, {1, 0}, {1, 1}},
        {{0, 0}, {1, 0}, {-1, 1}, {0, 1}},
        {{0, -1}, {0, 0}, {1, 0}, {1, 1}}
    },
    // 4: Z-piece (Red)
    {
        {{-1, 0}, {0, 0}, {0, 1}, {1, 1}},
        {{1, -1}, {1, 0}, {0, 0}, {0, 1}},
        {{-1, 0}, {0, 0}, {0, 1}, {1, 1}},
        {{1, -1}, {1, 0}, {0, 0}, {0, 1}}
    },
    // 5: J-piece (Blue)
    {
        {{-1, 0}, {0, 0}, {1, 0}, {-1, 1}},
        {{0, -1}, {0, 0}, {0, 1}, {1, 1}},
        {{-1, 0}, {0, 0}, {1, 0}, {1, -1}},
        {{0, -1}, {0, 0}, {0, 1}, {-1, -1}}
    },
    // 6: L-piece (Orange)
    {
        {{-1, 0}, {0, 0}, {1, 0}, {1, 1}},
        {{0, -1}, {0, 0}, {0, 1}, {-1, 1}},
        {{-1, 0}, {0, 0}, {1, 0}, {-1, -1}},
        {{0, -1}, {0, 0}, {0, 1}, {1, -1}}
    }
};

static uint8_t CheckCollision(const TetrisGame_t *game, int8_t cx, int8_t cy, int8_t rot)
{
    for (int i = 0; i < 4; i++) {
        int8_t px = cx + tetrominoes[game->current_piece_type][rot][i].x;
        int8_t py = cy + tetrominoes[game->current_piece_type][rot][i].y;

        // Check horizontal boundary
        if (px < 0 || px >= BOARD_COLS) {
            return 1;
        }
        // Check vertical bottom boundary
        if (py >= BOARD_ROWS) {
            return 1;
        }
        // Check collision with existing blocks
        if (py >= 0 && game->grid[py][px] != 0) {
            return 1;
        }
    }
    return 0;
}

static float GetDropInterval(uint32_t level)
{
    if (level == 0) return 0.80f;
    if (level == 1) return 0.72f;
    if (level == 2) return 0.63f;
    if (level == 3) return 0.55f;
    if (level == 4) return 0.47f;
    if (level == 5) return 0.38f;
    if (level == 6) return 0.30f;
    if (level == 7) return 0.22f;
    if (level == 8) return 0.13f;
    if (level == 9) return 0.10f;
    return 0.08f;
}

static void SpawnPiece(TetrisGame_t *game)
{
    game->current_piece_type = game->next_piece_type;
    game->next_piece_type = rand() % 7;
    game->current_piece_rotation = 0;
    game->current_x = 4;
    game->current_y = 0;

    // Check game over
    if (CheckCollision(game, game->current_x, game->current_y, game->current_piece_rotation)) {
        game->phase = TETRIS_PHASE_GAME_OVER;
    }
}

static void LockPiece(TetrisGame_t *game)
{
    for (int i = 0; i < 4; i++) {
        int8_t px = game->current_x + tetrominoes[game->current_piece_type][game->current_piece_rotation][i].x;
        int8_t py = game->current_y + tetrominoes[game->current_piece_type][game->current_piece_rotation][i].y;

        if (py >= 0 && py < BOARD_ROWS && px >= 0 && px < BOARD_COLS) {
            game->grid[py][px] = tetris_colors[game->current_piece_type + 1];
        }
    }

    // Line clearing logic
    uint32_t consecutive_clears = 0;
    for (int y = BOARD_ROWS - 1; y >= 0; y--) {
        uint8_t full = 1;
        for (int x = 0; x < BOARD_COLS; x++) {
            if (game->grid[y][x] == 0) {
                full = 0;
                break;
            }
        }

        if (full) {
            consecutive_clears++;
            // Shift rows down
            for (int sy = y; sy > 0; sy--) {
                for (int sx = 0; sx < BOARD_COLS; sx++) {
                    game->grid[sy][sx] = game->grid[sy - 1][sx];
                }
            }
            // Clear top row
            for (int sx = 0; sx < BOARD_COLS; sx++) {
                game->grid[0][sx] = 0;
            }
            // Repeat for same row index y because rows shifted down
            y++;
        }
    }

    if (consecutive_clears > 0) {
        game->lines_cleared += consecutive_clears;
        
        // Classic Nintendo Tetris scoring system
        uint32_t line_scores[] = {0, 40, 100, 300, 1200};
        uint32_t base_score = (consecutive_clears <= 4) ? line_scores[consecutive_clears] : 1200;
        game->score += base_score * (game->level + 1);

        // Advance level every 10 lines
        uint32_t next_level = game->lines_cleared / 10;
        if (next_level > game->level) {
            game->level = next_level;
            game->drop_interval = GetDropInterval(game->level);
        }
    }

    SpawnPiece(game);
}

void TetrisGame_Init(TetrisGame_t *game)
{
    memset(game, 0, sizeof(*game));
    game->phase = TETRIS_PHASE_PLAYING;
    game->next_piece_type = rand() % 7;
    game->drop_interval = GetDropInterval(0);
    SpawnPiece(game);
}

void TetrisGame_Update(TetrisGame_t *game, float delta_seconds)
{
    if (game->phase != TETRIS_PHASE_PLAYING) return;

    game->drop_timer += delta_seconds;
    if (game->drop_timer >= game->drop_interval) {
        game->drop_timer = 0;
        if (!CheckCollision(game, game->current_x, game->current_y + 1, game->current_piece_rotation)) {
            game->current_y++;
        } else {
            LockPiece(game);
        }
    }
}

void TetrisGame_HandleAction(TetrisGame_t *game, InputAction_t action)
{
    if (game->phase != TETRIS_PHASE_PLAYING) return;

    switch (action) {
        case INPUT_ACTION_MOVE_LEFT:
            if (!CheckCollision(game, game->current_x - 1, game->current_y, game->current_piece_rotation)) {
                game->current_x--;
            }
            break;
        case INPUT_ACTION_MOVE_RIGHT:
            if (!CheckCollision(game, game->current_x + 1, game->current_y, game->current_piece_rotation)) {
                game->current_x++;
            }
            break;
        case INPUT_ACTION_MOVE_UP:
            {
                int8_t next_rot = (game->current_piece_rotation + 1) % 4;
                if (!CheckCollision(game, game->current_x, game->current_y, next_rot)) {
                    game->current_piece_rotation = next_rot;
                } else if (!CheckCollision(game, game->current_x - 1, game->current_y, next_rot)) {
                    game->current_x--;
                    game->current_piece_rotation = next_rot;
                } else if (!CheckCollision(game, game->current_x + 1, game->current_y, next_rot)) {
                    game->current_x++;
                    game->current_piece_rotation = next_rot;
                }
            }
            break;
        case INPUT_ACTION_MOVE_DOWN:
            if (!CheckCollision(game, game->current_x, game->current_y + 1, game->current_piece_rotation)) {
                game->current_y++;
                game->drop_timer = 0;
            }
            break;
        case INPUT_ACTION_SELECT:
            while (!CheckCollision(game, game->current_x, game->current_y + 1, game->current_piece_rotation)) {
                game->current_y++;
            }
            LockPiece(game);
            break;
        default:
            break;
    }
}

// Local helper to draw cursor inside the chunk buffer
static void DrawChunkCursor(uint16_t *buffer, int y_start, int chunk_height, int16_t cx, int16_t cy, uint16_t color)
{
    // Draw 3x3 square at center
    for (int16_t dy = -1; dy <= 1; dy++) {
        int16_t sy = cy + dy;
        if (sy < y_start || sy >= y_start + chunk_height) continue;
        for (int16_t dx = -1; dx <= 1; dx++) {
            int16_t sx = cx + dx;
            if (sx < 0 || sx >= GFX_WIDTH) continue;
            buffer[(sy - y_start) * GFX_WIDTH + sx] = color;
        }
    }
    
    // Draw circle of radius 6
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

void TetrisGame_Render(TetrisGame_t *game, int16_t cursor_x, int16_t cursor_y, uint8_t show_cursor)
{
    char text[32];

    for (int chunk = 0; chunk < (GFX_HEIGHT / CHUNK_HEIGHT); chunk++) {
        int y_start = chunk * CHUNK_HEIGHT;
        int y_end = y_start + CHUNK_HEIGHT - 1;

        // Clear chunk color buffer
        for (int i = 0; i < GFX_WIDTH * CHUNK_HEIGHT; i++) {
            chunk_color_buffer[i] = COLOR_BACKGROUND;
        }

        // Draw Tetris Board Frame
        // Left border, right border, bottom border
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 
                            BOARD_OFFSET_X - 2, BOARD_OFFSET_Y, 2, 200, COLOR_BORDER);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 
                            BOARD_OFFSET_X + (BOARD_COLS * BLOCK_SIZE), BOARD_OFFSET_Y, 2, 200, COLOR_BORDER);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 
                            BOARD_OFFSET_X - 2, BOARD_OFFSET_Y + (BOARD_ROWS * BLOCK_SIZE), (BOARD_COLS * BLOCK_SIZE) + 4, 2, COLOR_BORDER);

        // Draw Board Background Grid Lines
        for (int c = 1; c < BOARD_COLS; c++) {
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                                BOARD_OFFSET_X + c * BLOCK_SIZE, BOARD_OFFSET_Y, 1, BOARD_ROWS * BLOCK_SIZE, COLOR_GRID_LINE);
        }
        for (int r = 1; r < BOARD_ROWS; r++) {
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                                BOARD_OFFSET_X, BOARD_OFFSET_Y + r * BLOCK_SIZE, BOARD_COLS * BLOCK_SIZE, 1, COLOR_GRID_LINE);
        }

        // Draw locked blocks in grid
        for (int r = 0; r < BOARD_ROWS; r++) {
            for (int c = 0; c < BOARD_COLS; c++) {
                uint16_t color = game->grid[r][c];
                if (color != 0) {
                    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                                        BOARD_OFFSET_X + c * BLOCK_SIZE + 1,
                                        BOARD_OFFSET_Y + r * BLOCK_SIZE + 1,
                                        BLOCK_SIZE - 1, BLOCK_SIZE - 1, color);
                }
            }
        }

        // Draw falling active piece
        if (game->phase == TETRIS_PHASE_PLAYING) {
            uint16_t color = tetris_colors[game->current_piece_type + 1];
            for (int i = 0; i < 4; i++) {
                int8_t px = game->current_x + tetrominoes[game->current_piece_type][game->current_piece_rotation][i].x;
                int8_t py = game->current_y + tetrominoes[game->current_piece_type][game->current_piece_rotation][i].y;

                if (py >= 0 && py < BOARD_ROWS && px >= 0 && px < BOARD_COLS) {
                    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                                        BOARD_OFFSET_X + px * BLOCK_SIZE + 1,
                                        BOARD_OFFSET_Y + py * BLOCK_SIZE + 1,
                                        BLOCK_SIZE - 1, BLOCK_SIZE - 1, color);
                }
            }
        }

        // Draw Left Panel Info (Title, Score, Lines, Level)
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "TETRIS", FONT4, 15, 30, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 15, 52, 75, 2, COLOR_BORDER);

        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "SCORE", FONT1, 15, 70, COLOR_DARKGREY, COLOR_BACKGROUND);
        snprintf(text, sizeof(text), "%06lu", (unsigned long)game->score);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, text, FONT2, 15, 84, COLOR_YELLOW, COLOR_BACKGROUND);

        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "LINES", FONT1, 15, 110, COLOR_DARKGREY, COLOR_BACKGROUND);
        snprintf(text, sizeof(text), "%03lu", (unsigned long)game->lines_cleared);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, text, FONT2, 15, 124, COLOR_WHITE, COLOR_BACKGROUND);

        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "LEVEL", FONT1, 15, 150, COLOR_DARKGREY, COLOR_BACKGROUND);
        snprintf(text, sizeof(text), "%02lu", (unsigned long)game->level);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, text, FONT2, 15, 164, COLOR_ACCENT_1, COLOR_BACKGROUND);

        // Draw Right Panel Info (Next Piece preview box)
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "NEXT", FONT2, 235, 30, COLOR_WHITE, COLOR_BACKGROUND);
        
        // Draw Next Box border
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 230, 48, 52, 52, COLOR_BORDER);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 232, 50, 48, 48, COLOR_BACKGROUND);

        // Draw Next piece inside the box
        uint16_t next_color = tetris_colors[game->next_piece_type + 1];
        // Center the preview inside the box (center coordinate X: 256, Y: 74)
        for (int i = 0; i < 4; i++) {
            int8_t dx = tetrominoes[game->next_piece_type][0][i].x;
            int8_t dy = tetrominoes[game->next_piece_type][0][i].y;
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                                256 + dx * BLOCK_SIZE - 5,
                                74 + dy * BLOCK_SIZE - 5,
                                BLOCK_SIZE - 1, BLOCK_SIZE - 1, next_color);
        }

        // Draw Controls Help at bottom right
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "CONTROLS", FONT1, 230, 120, COLOR_DARKGREY, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "L: Left", FONT1, 230, 134, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "R: Right", FONT1, 230, 146, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "U: Rotate", FONT1, 230, 158, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "D: Soft D", FONT1, 230, 170, COLOR_WHITE, COLOR_BACKGROUND);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "1: Hard D", FONT1, 230, 182, COLOR_WHITE, COLOR_BACKGROUND);

        // Game Over Overlay
        if (game->phase == TETRIS_PHASE_GAME_OVER) {
            // Draw dark background box (X: 40 to 280, Y: 50 to 190)
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 40, 50, 240, 140, 0x0841); // Dark grey/black panel
            
            // Draw box borders
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 40, 50, 240, 1, COLOR_WHITE);
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 40, 189, 240, 1, COLOR_WHITE);
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 40, 50, 1, 140, COLOR_WHITE);
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 279, 50, 1, 140, COLOR_WHITE);

            GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "GAME OVER", FONT4, 90, 65, 0xF800, 0x0841);
            
            snprintf(text, sizeof(text), "SCORE: %lu", (unsigned long)game->score);
            GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, text, FONT2, 105, 95, COLOR_WHITE, 0x0841);

            // Draw two buttons: RESTART and MENU
            // Restart button: X: 55 to 155, Y: 130 to 160
            // Menu button: X: 165 to 265, Y: 130 to 160
            uint16_t restart_btn_color = (cursor_x >= 55 && cursor_x <= 155 && cursor_y >= 130 && cursor_y <= 160) ? COLOR_ACCENT_1 : COLOR_DARKGREY;
            uint16_t menu_btn_color = (cursor_x >= 165 && cursor_x <= 265 && cursor_y >= 130 && cursor_y <= 160) ? COLOR_ACCENT_1 : COLOR_DARKGREY;

            // Draw button background outlines
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 55, 130, 100, 30, restart_btn_color);
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 57, 132, 96, 26, 0x0841);
            GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "RESTART", FONT2, 73, 138, restart_btn_color, 0x0841);

            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 165, 130, 100, 30, menu_btn_color);
            GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT, 167, 132, 96, 26, 0x0841);
            GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT, "MENU", FONT2, 198, 138, menu_btn_color, 0x0841);
        }

        // Draw cursor if active on this screen
        if (show_cursor) {
            DrawChunkCursor(chunk_color_buffer, y_start, CHUNK_HEIGHT, cursor_x, cursor_y, COLOR_YELLOW);
        }

        // Send chunk buffer to display
        ILI9341_SendPixelBuffer(0, y_start, GFX_WIDTH, CHUNK_HEIGHT, chunk_color_buffer);
    }
}
