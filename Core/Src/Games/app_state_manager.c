#include "Games/app_state_manager.h"
#include "Display/fonts.h"
#include "Graphics/graphics_3d.h"
#include "Games/game_ui_theme.h"
#include "Games/stack_game.h"
#include "Games/tetris_game.h"
#include <stdint.h>

#define CURSOR_STEP       15
#define CURSOR_START_X    160
#define CURSOR_START_Y    120

typedef enum {
    STATE_MAIN_MENU,
    STATE_STACK3D,
    STATE_TETRIS
} AppState_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t visible;
} CursorState_t;

typedef struct {
    int16_t min_x;
    int16_t min_y;
    int16_t max_x;
    int16_t max_y;
} Hitbox_t;

typedef struct {
    AppState_t state;
    const char *title;
    const char *subtitle;
    Hitbox_t card_hitbox;
    Hitbox_t restart_hitbox;
    Hitbox_t menu_hitbox;
    int16_t title_x;
    int16_t subtitle_x;
    void (*start)(void);
    void (*restart)(void);
    void (*update)(float delta_seconds);
    void (*render)(const CursorState_t *cursor);
    void (*handle_play_action)(InputAction_t action);
    uint8_t (*is_game_over)(void);
} GameScreen_t;

static AppState_t app_state = STATE_MAIN_MENU;
static CursorState_t cursor = {CURSOR_START_X, CURSOR_START_Y, 1U};
static Mesh_t cube_mesh;
static StackGame_t stack_game;
static TetrisGame_t tetris_game;

static void stack_start(void);
static void stack_restart(void);
static void stack_update(float delta_seconds);
static void stack_render(const CursorState_t *cursor_state);
static void stack_handle_play_action(InputAction_t action);
static uint8_t stack_is_game_over(void);

static void tetris_start(void);
static void tetris_restart(void);
static void tetris_update(float delta_seconds);
static void tetris_render(const CursorState_t *cursor_state);
static void tetris_handle_play_action(InputAction_t action);
static uint8_t tetris_is_game_over(void);

static const GameScreen_t game_screens[] = {
    {
        STATE_STACK3D,
        "STACK 3D",
        "3D Physics",
        {30, 90, 140, 170},
        {60, 135, 150, 160},
        {170, 135, 260, 160},
        50,
        55,
        stack_start,
        stack_restart,
        stack_update,
        stack_render,
        stack_handle_play_action,
        stack_is_game_over
    },
    {
        STATE_TETRIS,
        "TETRIS",
        "Classic 2D",
        {180, 90, 290, 170},
        {55, 130, 155, 160},
        {165, 130, 265, 160},
        208,
        203,
        tetris_start,
        tetris_restart,
        tetris_update,
        tetris_render,
        tetris_handle_play_action,
        tetris_is_game_over
    }
};

static const uint8_t game_screen_count =
    (uint8_t)(sizeof(game_screens) / sizeof(game_screens[0]));
static const GameScreen_t *active_screen = NULL;

static uint8_t hitbox_contains(const Hitbox_t *hitbox, int16_t x, int16_t y)
{
    return (x >= hitbox->min_x && x <= hitbox->max_x &&
            y >= hitbox->min_y && y <= hitbox->max_y);
}

static void move_cursor(InputAction_t action)
{
    if (action == INPUT_ACTION_MOVE_UP) {
        cursor.y = (cursor.y >= CURSOR_STEP) ? (cursor.y - CURSOR_STEP) : 0;
    } else if (action == INPUT_ACTION_MOVE_DOWN) {
        cursor.y = (cursor.y <= GFX_HEIGHT - 1 - CURSOR_STEP)
            ? (cursor.y + CURSOR_STEP)
            : (GFX_HEIGHT - 1);
    } else if (action == INPUT_ACTION_MOVE_LEFT) {
        cursor.x = (cursor.x >= CURSOR_STEP) ? (cursor.x - CURSOR_STEP) : 0;
    } else if (action == INPUT_ACTION_MOVE_RIGHT) {
        cursor.x = (cursor.x <= GFX_WIDTH - 1 - CURSOR_STEP)
            ? (cursor.x + CURSOR_STEP)
            : (GFX_WIDTH - 1);
    }
}

static uint8_t is_cursor_action(InputAction_t action)
{
    return (action == INPUT_ACTION_MOVE_UP ||
            action == INPUT_ACTION_MOVE_DOWN ||
            action == INPUT_ACTION_MOVE_LEFT ||
            action == INPUT_ACTION_MOVE_RIGHT);
}

static void reset_cursor(void)
{
    cursor.x = CURSOR_START_X;
    cursor.y = CURSOR_START_Y;
    cursor.visible = 1U;
}

static void start_screen(const GameScreen_t *screen)
{
    active_screen = screen;
    app_state = screen->state;
    cursor.visible = 0U;
    screen->start();
}

static void restart_active_screen(void)
{
    if (active_screen == NULL) return;

    cursor.visible = 0U;
    active_screen->restart();
}

static void return_to_main_menu(void)
{
    app_state = STATE_MAIN_MENU;
    active_screen = NULL;
    cursor.visible = 1U;
}

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

static void draw_menu_background(int y_start)
{
    for (int i = 0; i < GFX_WIDTH * CHUNK_HEIGHT; i++) {
        chunk_color_buffer[i] = GAME_UI_BG;
    }

    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        0, 168, GFX_WIDTH, 2, GAME_UI_GRID);
    for (int16_t y = 176; y < GFX_HEIGHT; y += 14) {
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            0, y, GFX_WIDTH, 1, GAME_UI_GRID_DIM);
    }
    for (int16_t x = 20; x < GFX_WIDTH; x += 40) {
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            x, 170, 1, GFX_HEIGHT - 170, GAME_UI_GRID_DIM);
    }

    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        0, 0, GFX_WIDTH, 18, GAME_UI_PANEL_INNER);
    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        0, 18, GFX_WIDTH, 1, GAME_UI_GRID);
}

static void render_menu_card(const GameScreen_t *screen, int y_start)
{
    const Hitbox_t *card = &screen->card_hitbox;
    int16_t width = card->max_x - card->min_x;
    int16_t height = card->max_y - card->min_y;
    uint8_t selected = hitbox_contains(card, cursor.x, cursor.y);
    uint16_t border_color = selected ? GAME_UI_NEON_GREEN : GAME_UI_GRID;
    uint16_t text_color = selected ? GAME_UI_YELLOW : GAME_UI_WHITE;
    uint16_t detail_color = selected ? GAME_UI_NEON_CYAN : GAME_UI_MUTED;

    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        card->min_x, card->min_y, width, height, border_color);
    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        card->min_x + 2, card->min_y + 2,
                        width - 4, height - 4, GAME_UI_PANEL);
    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        card->min_x + 5, card->min_y + 5,
                        width - 10, height - 10, GAME_UI_PANEL_INNER);
    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        card->min_x + 8, card->min_y + 14,
                        18, 18, selected ? GAME_UI_NEON_GREEN : GAME_UI_GRID_DIM);
    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        card->min_x + 12, card->min_y + 18,
                        10, 10, GAME_UI_BG);
    GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        screen->title, FONT2, screen->title_x, 112,
                        text_color, GAME_UI_PANEL_INNER);
    GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        screen->subtitle, FONT1, screen->subtitle_x, 136,
                        detail_color, GAME_UI_PANEL_INNER);
    GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                        card->min_x + 12, card->max_y - 16,
                        width - 24, 2, selected ? GAME_UI_YELLOW : GAME_UI_GRID_DIM);
}

static void Render_MainMenu(void)
{
    for (int chunk = 0; chunk < (GFX_HEIGHT / CHUNK_HEIGHT); chunk++) {
        int y_start = chunk * CHUNK_HEIGHT;

        draw_menu_background(y_start);

        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            "STM32 RETRO PLAY", FONT4, 28, 26,
                            GAME_UI_WHITE, GAME_UI_BG);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            28, 50, 264, 2, GAME_UI_NEON_MAGENTA);
        GFX3D_FillChunkRect(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            70, 56, 180, 1, GAME_UI_NEON_CYAN);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            "SELECT A CABINET", FONT1, 88, 62,
                            GAME_UI_YELLOW, GAME_UI_BG);

        for (uint8_t i = 0; i < game_screen_count; i++) {
            render_menu_card(&game_screens[i], y_start);
        }

        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            "WASD MOVE", FONT1, 70, 202,
                            GAME_UI_MUTED, GAME_UI_BG);
        GFX3D_DrawChunkText(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            "1 START", FONT1, 190, 202,
                            GAME_UI_WHITE, GAME_UI_BG);

        if (cursor.visible) {
            DrawChunkCursor(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            cursor.x, cursor.y, GAME_UI_YELLOW);
        }

        ILI9341_SendPixelBuffer(0, y_start, GFX_WIDTH, CHUNK_HEIGHT, chunk_color_buffer);
    }
}

static void stack_start(void)
{
    StackGame_Init(&stack_game);
    StackGame_HandlePress(&stack_game);
}

static void stack_restart(void)
{
    StackGame_Init(&stack_game);
    StackGame_HandlePress(&stack_game);
}

static void stack_update(float delta_seconds)
{
    StackGame_Update(&stack_game, delta_seconds);
}

static void stack_render(const CursorState_t *cursor_state)
{
    StackGame_Render(&stack_game, &cube_mesh,
                     cursor_state->x, cursor_state->y, cursor_state->visible);
}

static void stack_handle_play_action(InputAction_t action)
{
    if (action == INPUT_ACTION_SELECT &&
        (stack_game.phase == STACK_PHASE_TITLE ||
         stack_game.phase == STACK_PHASE_PLAYING)) {
        StackGame_HandlePress(&stack_game);
    }
}

static uint8_t stack_is_game_over(void)
{
    return (stack_game.phase == STACK_PHASE_GAME_OVER);
}

static void tetris_start(void)
{
    TetrisGame_Init(&tetris_game);
}

static void tetris_restart(void)
{
    TetrisGame_Init(&tetris_game);
}

static void tetris_update(float delta_seconds)
{
    TetrisGame_Update(&tetris_game, delta_seconds);
}

static void tetris_render(const CursorState_t *cursor_state)
{
    TetrisGame_Render(&tetris_game,
                      cursor_state->x, cursor_state->y, cursor_state->visible);
}

static void tetris_handle_play_action(InputAction_t action)
{
    TetrisGame_HandleAction(&tetris_game, action);
}

static uint8_t tetris_is_game_over(void)
{
    return (tetris_game.phase == TETRIS_PHASE_GAME_OVER);
}

void AppStateManager_Init(void)
{
    app_state = STATE_MAIN_MENU;
    active_screen = NULL;
    reset_cursor();
    cube_mesh = GFX3D_CreateCube(1.0f, GAME_UI_WHITE);
    StackGame_Init(&stack_game);
}

void AppStateManager_Update(float delta_seconds)
{
    if (app_state != STATE_MAIN_MENU && active_screen != NULL) {
        active_screen->update(delta_seconds);
    }
}

void AppStateManager_Render(void)
{
    if (app_state == STATE_MAIN_MENU) {
        Render_MainMenu();
    } else if (active_screen != NULL) {
        active_screen->render(&cursor);
    }
}

void AppStateManager_HandleAction(InputAction_t action)
{
    if (action == INPUT_ACTION_NONE) return;

    if (app_state == STATE_MAIN_MENU) {
        if (is_cursor_action(action)) {
            move_cursor(action);
        } else if (action == INPUT_ACTION_SELECT) {
            for (uint8_t i = 0; i < game_screen_count; i++) {
                if (hitbox_contains(&game_screens[i].card_hitbox,
                                    cursor.x, cursor.y)) {
                    start_screen(&game_screens[i]);
                    break;
                }
            }
        }
        return;
    }

    if (active_screen == NULL) return;

    if (active_screen->is_game_over()) {
        cursor.visible = 1U;
        if (is_cursor_action(action)) {
            move_cursor(action);
        } else if (action == INPUT_ACTION_SELECT) {
            if (hitbox_contains(&active_screen->restart_hitbox,
                                cursor.x, cursor.y)) {
                restart_active_screen();
            } else if (hitbox_contains(&active_screen->menu_hitbox,
                                       cursor.x, cursor.y)) {
                return_to_main_menu();
            }
        }
        return;
    }

    active_screen->handle_play_action(action);
}
