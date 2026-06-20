#include "stack_game.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define COLOR_BACKGROUND  0x0862  // #0b0c10
#define COLOR_ACCENT_1    0x07E0  // Cyan
#define COLOR_ACCENT_2    0xF8E0  // Pink
#define COLOR_GRID        0x786F  // Magenta/Purple neon grid
#define COLOR_WHITE       0xFFFF
#define COLOR_LIGHTGREY   0xC618
#define COLOR_DARKGREY    0x7BEF
#define COLOR_RED         0xF800
#define COLOR_ORANGE      0xFD20
#define COLOR_YELLOW      0xFFE0

#define STACK_OFFSET_X    4

#define STACK_BLOCK_HEIGHT 0.3f
#define STACK_BASE_SIZE 2.0f
#define STACK_TRAVEL_DISTANCE 2.5f
#define STACK_START_SPEED 1.2f
#define STACK_SPEED_STEP 0.08f
#define STACK_MAX_SPEED 3.0f
#define STACK_GRAVITY 4.5f

static const uint16_t block_colors[] = {
    0xF800u, 0xFD20u, 0xFFE0u, 0x07E0u, 0x07FFu, 0x001Fu, 0xF81Fu
};

static float Clamp(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float GetAxis(const Vector3_t *value, uint8_t axis)
{
    return axis == 0U ? value->x : value->z;
}

static void SetAxis(Vector3_t *value, uint8_t axis, float component)
{
    if (axis == 0U) value->x = component;
    else value->z = component;
}

static float GetSpeed(const StackGame_t *game)
{
    return fminf(STACK_START_SPEED + STACK_SPEED_STEP * (float)game->score, STACK_MAX_SPEED);
}

static const StackBlock_t *GetTopBlock(const StackGame_t *game)
{
    return &game->blocks[game->block_count - 1];
}

static void AddFallingBlock(StackGame_t *game, StackBlock_t block, Vector3_t velocity)
{
    int slot = 0;
    for (int i = 0; i < STACK_MAX_FALLING_BLOCKS; i++) {
        if (!game->falling[i].active) {
            slot = i;
            break;
        }
    }

    game->falling[slot].block = block;
    game->falling[slot].velocity = velocity;
    game->falling[slot].active = 1U;
}

static void AddSettledBlock(StackGame_t *game, StackBlock_t block)
{
    if (game->block_count == STACK_MAX_VISIBLE_BLOCKS) {
        memmove(&game->blocks[0], &game->blocks[1],
                (STACK_MAX_VISIBLE_BLOCKS - 1) * sizeof(StackBlock_t));
        game->block_count--;
    }
    game->blocks[game->block_count++] = block;
}

static void SpawnActiveBlock(StackGame_t *game)
{
    const StackBlock_t *top = GetTopBlock(game);
    game->move_axis = (uint8_t)(game->score & 1U);
    game->move_direction = 1.0f;
    game->active_block = *top;
    game->active_block.position.y = top->position.y + STACK_BLOCK_HEIGHT;
    SetAxis(&game->active_block.position, game->move_axis,
            GetAxis(&top->position, game->move_axis) - STACK_TRAVEL_DISTANCE);
    game->active_block.color = block_colors[(game->score + 1U) %
        (sizeof(block_colors) / sizeof(block_colors[0]))];
}

static void StartGame(StackGame_t *game)
{
    uint32_t best_score = game->best_score;
    memset(game, 0, sizeof(*game));
    game->best_score = best_score;
    game->phase = STACK_PHASE_PLAYING;
    game->blocks[0] = (StackBlock_t){
        {0.0f, 0.0f, 0.0f},
        {STACK_BASE_SIZE, STACK_BLOCK_HEIGHT, STACK_BASE_SIZE},
        0x39E7u
    };
    game->block_count = 1;
    SpawnActiveBlock(game);
}

static void PlaceActiveBlock(StackGame_t *game)
{
    const StackBlock_t *top = GetTopBlock(game);
    uint8_t axis = game->move_axis;
    float active_center = GetAxis(&game->active_block.position, axis);
    float active_size = GetAxis(&game->active_block.size, axis);
    float top_center = GetAxis(&top->position, axis);
    float top_size = GetAxis(&top->size, axis);
    float active_min = active_center - active_size * 0.5f;
    float active_max = active_center + active_size * 0.5f;
    float top_min = top_center - top_size * 0.5f;
    float top_max = top_center + top_size * 0.5f;
    float overlap_min = fmaxf(active_min, top_min);
    float overlap_max = fminf(active_max, top_max);
    float overlap = overlap_max - overlap_min;
    float speed = GetSpeed(game);

    if (overlap <= 0.0f) {
        Vector3_t velocity = {0.0f, 0.0f, 0.0f};
        SetAxis(&velocity, axis, game->move_direction * speed * 0.35f);
        AddFallingBlock(game, game->active_block, velocity);
        game->phase = STACK_PHASE_GAME_OVER;
        if (game->score > game->best_score) game->best_score = game->score;
        return;
    }

    StackBlock_t placed = game->active_block;
    SetAxis(&placed.position, axis, (overlap_min + overlap_max) * 0.5f);
    SetAxis(&placed.size, axis, overlap);

    float discarded_size = active_size - overlap;
    if (discarded_size > 0.001f) {
        StackBlock_t discarded = game->active_block;
        float discarded_center = active_center < top_center
            ? (active_min + overlap_min) * 0.5f
            : (overlap_max + active_max) * 0.5f;
        SetAxis(&discarded.position, axis, discarded_center);
        SetAxis(&discarded.size, axis, discarded_size);
        Vector3_t velocity = {0.0f, 0.0f, 0.0f};
        SetAxis(&velocity, axis, game->move_direction * speed * 0.35f);
        AddFallingBlock(game, discarded, velocity);
    }

    AddSettledBlock(game, placed);
    game->score++;
    if (game->score > game->best_score) game->best_score = game->score;
    SpawnActiveBlock(game);
}

void StackGame_Init(StackGame_t *game)
{
    memset(game, 0, sizeof(*game));
    game->phase = STACK_PHASE_TITLE;
}

void StackGame_HandlePress(StackGame_t *game)
{
    if (game->phase == STACK_PHASE_PLAYING) PlaceActiveBlock(game);
    else StartGame(game);
}

void StackGame_Update(StackGame_t *game, float delta_seconds)
{
    float dt = Clamp(delta_seconds, 0.0f, 0.1f);

    if (game->phase == STACK_PHASE_PLAYING) {
        const StackBlock_t *top = GetTopBlock(game);
        float center = GetAxis(&top->position, game->move_axis);
        float minimum = center - STACK_TRAVEL_DISTANCE;
        float maximum = center + STACK_TRAVEL_DISTANCE;
        float position = GetAxis(&game->active_block.position, game->move_axis);
        position += game->move_direction * GetSpeed(game) * dt;

        if (position > maximum) {
            position = maximum - (position - maximum);
            game->move_direction = -1.0f;
        } else if (position < minimum) {
            position = minimum + (minimum - position);
            game->move_direction = 1.0f;
        }
        SetAxis(&game->active_block.position, game->move_axis, position);
    }

    for (int i = 0; i < STACK_MAX_FALLING_BLOCKS; i++) {
        StackFallingBlock_t *falling = &game->falling[i];
        if (!falling->active) continue;
        falling->velocity.y -= STACK_GRAVITY * dt;
        falling->block.position.x += falling->velocity.x * dt;
        falling->block.position.y += falling->velocity.y * dt;
        falling->block.position.z += falling->velocity.z * dt;
        if (falling->block.position.y < game->camera_y - 4.0f) falling->active = 0U;
    }

    if (game->block_count > 0) {
        const StackBlock_t *top = GetTopBlock(game);
        float target_y = fmaxf(0.0f, top->position.y - 0.9f);
        float blend = fminf(1.0f, dt * 3.0f);
        game->camera_x += (top->position.x - game->camera_x) * blend;
        game->camera_y += (target_y - game->camera_y) * blend;
        game->camera_z += (top->position.z - game->camera_z) * blend;
    }
}

static Matrix4_t BlockTransform(StackBlock_t block)
{
    Matrix4_t translation = M4_Translation(block.position.x, block.position.y, block.position.z);
    Matrix4_t scaling = M4_Scaling(block.size.x, block.size.y, block.size.z);
    return M4_Multiply(translation, scaling);
}

int StackGame_BuildRenderObjects(const StackGame_t *game, const Mesh_t *cube,
                                 GFX3D_RenderObject_t *objects, int capacity)
{
    int count = 0;
    for (int i = 0; i < game->block_count && count < capacity; i++) {
        objects[count++] = (GFX3D_RenderObject_t){cube, BlockTransform(game->blocks[i]), game->blocks[i].color};
    }

    if (game->phase == STACK_PHASE_PLAYING && count < capacity) {
        objects[count++] = (GFX3D_RenderObject_t){cube, BlockTransform(game->active_block), game->active_block.color};
    }

    for (int i = 0; i < STACK_MAX_FALLING_BLOCKS && count < capacity; i++) {
        if (!game->falling[i].active) continue;
        objects[count++] = (GFX3D_RenderObject_t){cube, BlockTransform(game->falling[i].block), game->falling[i].block.color};
    }
    return count;
}

Matrix4_t StackGame_GetView(const StackGame_t *game)
{
    Vector3_t target = {game->camera_x, game->camera_y + 0.4f, game->camera_z};
    Vector3_t eye = {game->camera_x + 4.0f, game->camera_y + 3.2f, game->camera_z + 5.0f};
    return M4_LookAt(eye, target, (Vector3_t){0.0f, 1.0f, 0.0f});
}

static void DrawWireframeBlock(int16_t cx, int16_t cy, int16_t w, int16_t h, int16_t thick, uint16_t color) {
    Point_t p_top       = { cx, cy };
    Point_t p_left      = { cx - w, cy + h };
    Point_t p_right     = { cx + w, cy + h };
    Point_t p_mid       = { cx, cy + 2 * h };

    Point_t p_left_bot  = { cx - w, cy + h + thick };
    Point_t p_right_bot = { cx + w, cy + h + thick };
    Point_t p_mid_bot   = { cx, cy + 2 * h + thick };

    GFX_DrawLine(p_top, p_left, color);
    GFX_DrawLine(p_top, p_right, color);
    GFX_DrawLine(p_left, p_mid, color);
    GFX_DrawLine(p_right, p_mid, color);

    GFX_DrawLine(p_left, p_left_bot, color);
    GFX_DrawLine(p_right, p_right_bot, color);
    GFX_DrawLine(p_mid, p_mid_bot, color);

    GFX_DrawLine(p_left_bot, p_mid_bot, color);
    GFX_DrawLine(p_mid_bot, p_right_bot, color);
}

static void DrawTitleScreen(void)
{
    ILI9341_FillScreen(COLOR_BACKGROUND);

    ILI9341_DrawPixel(30, 25, COLOR_WHITE);
    ILI9341_DrawPixel(75, 80, COLOR_LIGHTGREY);
    ILI9341_DrawPixel(140, 15, COLOR_WHITE);
    ILI9341_DrawPixel(180, 20, COLOR_WHITE);
    ILI9341_DrawPixel(280, 40, COLOR_LIGHTGREY);
    ILI9341_DrawPixel(100, 110, COLOR_LIGHTGREY);

    GFX_DrawCircle((Point_t){160, 130}, 45, COLOR_RED);
    GFX_DrawCircle((Point_t){160, 130}, 40, COLOR_ORANGE);
    GFX_DrawCircle((Point_t){160, 130}, 35, COLOR_YELLOW);

    ILI9341_DrawHLine(0, 170, 320, COLOR_GRID);
    for (int16_t x = -80; x <= 400; x += 60) {
        GFX_DrawLine((Point_t){160, 170}, (Point_t){x, 240}, COLOR_GRID);
    }
    ILI9341_DrawHLine(0, 172, 320, COLOR_GRID);
    ILI9341_DrawHLine(0, 176, 320, COLOR_GRID);
    ILI9341_DrawHLine(0, 183, 320, COLOR_GRID);
    ILI9341_DrawHLine(0, 194, 320, COLOR_GRID);
    ILI9341_DrawHLine(0, 210, 320, COLOR_GRID);
    ILI9341_DrawHLine(0, 230, 320, COLOR_GRID);

    DrawWireframeBlock(230, 160, 32, 10, 10, COLOR_DARKGREY);
    DrawWireframeBlock(230, 135, 24, 8, 12, COLOR_ACCENT_1);
    DrawWireframeBlock(230 + STACK_OFFSET_X, 110, 24, 8, 12, COLOR_ACCENT_2);
    DrawWireframeBlock(226, 85, 24, 8, 12, COLOR_YELLOW);

    ILI9341_DrawText("STACKM3D", FONT4, 25, 30, COLOR_ACCENT_1, COLOR_BACKGROUND);
    ILI9341_DrawText("Stack on STM32F429", FONT1, 25, 52, COLOR_YELLOW, COLOR_BACKGROUND);
    ILI9341_DrawHLine(25, 68, 135, COLOR_ACCENT_2);

    GFX_DrawRectangle((Point_t){20, 90}, (Point_t){140, 160}, COLOR_ACCENT_1);
    GFX_DrawRectangle((Point_t){25, 95}, (Point_t){135, 155}, COLOR_ACCENT_2);

    ILI9341_DrawText(">", FONT2, 28, 105, COLOR_YELLOW, COLOR_BACKGROUND);
    ILI9341_DrawText("PLAY GAME", FONT2, 42, 105, COLOR_WHITE, COLOR_BACKGROUND);
    ILI9341_DrawText("HIGH SCORES", FONT2, 42, 135, COLOR_DARKGREY, COLOR_BACKGROUND);

    ILI9341_DrawText("PRESS THE BUTTON", FONT2, 20, 195, COLOR_YELLOW, COLOR_BACKGROUND);
    ILI9341_DrawText("TO START THE GAME", FONT1, 20, 212, COLOR_WHITE, COLOR_BACKGROUND);
}

static void DrawGameOverlay(uint16_t *buffer, int y_start,
                            int chunk_height, void *context)
{
  const StackGame_t *game = context;
  char text[32];
  GFX3D_FillChunkRect(buffer, y_start, chunk_height, 0, 0, GFX_WIDTH, 22, BLACK);
  snprintf(text, sizeof(text), "SCORE %lu", (unsigned long)game->score);
  GFX3D_DrawChunkText(buffer, y_start, chunk_height, text, FONT2, 6, 4,
                      WHITE, BLACK);
  snprintf(text, sizeof(text), "BEST %lu", (unsigned long)game->best_score);
  GFX3D_DrawChunkText(buffer, y_start, chunk_height, text, FONT2, 220, 4,
                      0xFFE0U, BLACK);

  if (game->phase != STACK_PHASE_GAME_OVER) return;

  GFX3D_FillChunkRect(buffer, y_start, chunk_height, 48, 72, 225, 97, 0x0841U);
  GFX3D_FillChunkRect(buffer, y_start, chunk_height, 48, 72, 225, 1, WHITE);
  GFX3D_FillChunkRect(buffer, y_start, chunk_height, 48, 168, 225, 1, WHITE);
  GFX3D_FillChunkRect(buffer, y_start, chunk_height, 48, 72, 1, 97, WHITE);
  GFX3D_FillChunkRect(buffer, y_start, chunk_height, 272, 72, 1, 97, WHITE);
  GFX3D_DrawChunkText(buffer, y_start, chunk_height, "GAME OVER", FONT4,
                      92, 86, 0xF800U, 0x0841U);
  snprintf(text, sizeof(text), "SCORE %lu  BEST %lu",
           (unsigned long)game->score, (unsigned long)game->best_score);
  GFX3D_DrawChunkText(buffer, y_start, chunk_height, text, FONT2, 76, 121,
                      WHITE, 0x0841U);
  GFX3D_DrawChunkText(buffer, y_start, chunk_height,
                      "PRESS BUTTON TO RESTART", FONT1, 69, 147,
                      0xFFE0U, 0x0841U);
}

void StackGame_Render(StackGame_t *game, Mesh_t *cube_mesh)
{
    static StackGamePhase_t drawn_phase = (StackGamePhase_t)UINT8_MAX;
    static Matrix4_t proj;
    static uint8_t proj_initialized = 0;

    if (!proj_initialized) {
        float aspect = (float)GFX_WIDTH / (float)GFX_HEIGHT;
        proj = M4_Perspective(50.0f, aspect, 0.1f, 50.0f);
        proj_initialized = 1;
    }

    if (game->phase == STACK_PHASE_TITLE) {
        if (drawn_phase != STACK_PHASE_TITLE) {
            DrawTitleScreen();
            drawn_phase = STACK_PHASE_TITLE;
        }
        HAL_Delay(10);
        return;
    }

    static GFX3D_RenderObject_t render_objects[STACK_MAX_RENDER_OBJECTS];
    int object_count = StackGame_BuildRenderObjects(
        game, cube_mesh, render_objects, STACK_MAX_RENDER_OBJECTS);
    Matrix4_t view = StackGame_GetView(game);
    GFX3D_RenderOptions_t options = {
        0x0841U, 0U, 0U, DrawGameOverlay, game
    };
    GFX3D_RenderObjects(render_objects, object_count, view, proj, options);
    drawn_phase = game->phase;
    HAL_Delay(10);
}
