/**
  ******************************************************************************
  * @file    graphics_3d.h
  * @brief   3D Graphics engine with flat shading and Z-buffer for STM32F429
  * @author  Nguyen Quy Duc - Hanoi University of Science and Technology
  ******************************************************************************
  */

#ifndef GRAPHICS_3D_H
#define GRAPHICS_3D_H

#include "stm32f4xx_hal.h"
#include "Display/ILI9341_STM32_Driver.h"
#include "Graphics/graphics_2d.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define CHUNK_HEIGHT 120
#define GFX3D_MAX_VERTICES 512

typedef struct {
    float x, y, z;
} Vector3_t;

typedef struct {
    float m[4][4];
} Matrix4_t;

typedef struct {
    float w, x, y, z;
} Quaternion_t;

typedef struct {
    int v[3];
    uint16_t color;
    uint8_t wire_edges;
} Face_t;

typedef struct {
    Vector3_t *vertices;
    int vertex_count;
    Face_t *faces;
    int face_count;
} Mesh_t;

typedef struct {
    const Mesh_t *mesh;
    Matrix4_t world;
    uint16_t color;
} GFX3D_RenderObject_t;

typedef void (*GFX3D_ChunkOverlay_t)(uint16_t *buffer, int y_start,
                                     int chunk_height, void *context);

typedef struct {
    uint16_t background_color;
    uint8_t show_wireframe;
    uint8_t show_axes;
    GFX3D_ChunkOverlay_t overlay;
    void *overlay_context;
} GFX3D_RenderOptions_t;

Matrix4_t M4_Identity(void);
Matrix4_t M4_Scaling(float sx, float sy, float sz);
Matrix4_t M4_Translation(float tx, float ty, float tz);
Matrix4_t M4_RotationX(float angle_rad);
Matrix4_t M4_RotationY(float angle_rad);
Matrix4_t M4_RotationZ(float angle_rad);
Matrix4_t M4_Perspective(float fov_deg, float aspect, float znear, float zfar);
Matrix4_t M4_LookAt(Vector3_t eye, Vector3_t target, Vector3_t up);
Matrix4_t M4_Multiply(Matrix4_t a, Matrix4_t b);
Vector3_t M4_MultiplyVector(Matrix4_t m, Vector3_t v);

Quaternion_t Q_Identity(void);
Quaternion_t Q_Normalize(Quaternion_t q);
Quaternion_t Q_Multiply(Quaternion_t a, Quaternion_t b);
Quaternion_t Q_FromAxisAngle(Vector3_t axis, float angle_rad);
Quaternion_t Q_IntegrateAngularVelocity(Quaternion_t orientation, Vector3_t angular_velocity, float delta_seconds);
Matrix4_t Q_ToMatrix(Quaternion_t q);

Vector3_t V3_Add(Vector3_t a, Vector3_t b);
Vector3_t V3_Sub(Vector3_t a, Vector3_t b);
float V3_Dot(Vector3_t a, Vector3_t b);
Vector3_t V3_Cross(Vector3_t a, Vector3_t b);
Vector3_t V3_Normalize(Vector3_t v);
float V3_Length(Vector3_t v);

Mesh_t GFX3D_CreateCube(float size, uint16_t color);
Mesh_t GFX3D_CreatePyramid(float width, float height, uint16_t color);
Mesh_t GFX3D_CreateSphere(float radius, int longitude_segments, int latitude_segments, uint16_t color);
Mesh_t GFX3D_CreateTorus(float outer_radius, float inner_radius, int radial_segments, int tubular_segments, uint16_t color);
void GFX3D_FreeMesh(Mesh_t *mesh);

void GFX3D_FillChunkRect(uint16_t *buffer, int y_start, int chunk_height,
                         int x, int y, int width, int height, uint16_t color);
void GFX3D_DrawChunkText(uint16_t *buffer, int y_start, int chunk_height,
                         const char *text, const uint8_t font[], int x, int y,
                         uint16_t color, uint16_t background_color);
void GFX3D_RenderObjects(const GFX3D_RenderObject_t *objects, int object_count, Matrix4_t view, Matrix4_t proj, GFX3D_RenderOptions_t options);
void GFX3D_RenderScene(Mesh_t *mesh, Matrix4_t world, Matrix4_t view, Matrix4_t proj, uint16_t bgcolor, uint8_t show_wireframe);

void ILI9341_SendPixelBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buffer);
extern uint16_t chunk_color_buffer[GFX_WIDTH * CHUNK_HEIGHT];

#endif /* GRAPHICS_3D_H */
