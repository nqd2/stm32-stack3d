#ifndef STACK_GAME_H
#define STACK_GAME_H

#include "Graphics/graphics_3d.h"
#include <stdint.h>

#define STACK_MAX_VISIBLE_BLOCKS 20
#define STACK_MAX_FALLING_BLOCKS 4
#define STACK_MAX_RENDER_OBJECTS (STACK_MAX_VISIBLE_BLOCKS + STACK_MAX_FALLING_BLOCKS + 1)

typedef enum {
    STACK_PHASE_TITLE,
    STACK_PHASE_PLAYING,
    STACK_PHASE_GAME_OVER
} StackGamePhase_t;

typedef struct {
    Vector3_t position;
    Vector3_t size;
    uint16_t color;
} StackBlock_t;

typedef struct {
    StackBlock_t block;
    Vector3_t velocity;
    uint8_t active;
} StackFallingBlock_t;

typedef struct {
    StackGamePhase_t phase;
    StackBlock_t blocks[STACK_MAX_VISIBLE_BLOCKS];
    int block_count;
    StackBlock_t active_block;
    StackFallingBlock_t falling[STACK_MAX_FALLING_BLOCKS];
    uint32_t score;
    uint32_t best_score;
    float camera_x;
    float camera_y;
    float camera_z;
    float move_direction;
    uint8_t move_axis;
} StackGame_t;

void StackGame_Init(StackGame_t *game);
void StackGame_HandlePress(StackGame_t *game);
void StackGame_Update(StackGame_t *game, float delta_seconds);
int StackGame_BuildRenderObjects(const StackGame_t *game, const Mesh_t *cube, GFX3D_RenderObject_t *objects, int capacity);
Matrix4_t StackGame_GetView(const StackGame_t *game);
void StackGame_Render(StackGame_t *game, Mesh_t *cube_mesh);

#endif
