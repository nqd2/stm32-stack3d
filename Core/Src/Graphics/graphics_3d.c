/**
  ******************************************************************************
  * @file    graphics_3d.c
  * @brief   3D Graphics engine implementation for STM32F429
  ******************************************************************************
  */

#include "Graphics/graphics_3d.h"

#define NUM_CHUNKS (GFX_HEIGHT / CHUNK_HEIGHT)
#define FACE_EDGE_01 0x01u
#define FACE_EDGE_12 0x02u
#define FACE_EDGE_20 0x04u
#define FACE_ALL_EDGES (FACE_EDGE_01 | FACE_EDGE_12 | FACE_EDGE_20)
#define RGB565_TO_WIRE_ORDER(color) \
    (uint16_t)((((uint16_t)(color) & 0x00FFu) << 8) | \
               (((uint16_t)(color) & 0xFF00u) >> 8))

_Static_assert(RGB565_TO_WIRE_ORDER(0xF800u) == 0x00F8u,
               "RGB565 wire order must send the high byte first");

uint16_t chunk_color_buffer[GFX_WIDTH * CHUNK_HEIGHT];
static uint16_t chunk_z_buffer[GFX_WIDTH * CHUNK_HEIGHT];

typedef struct {
    float x;
    float y;
    float z;
} RasterVertex_t;

static RasterVertex_t projected_vertices[GFX3D_MAX_VERTICES];

static RasterVertex_t ProjectVertex(Matrix4_t mvp, Vector3_t v)
{
    float x = v.x * mvp.m[0][0] + v.y * mvp.m[0][1] + v.z * mvp.m[0][2] + mvp.m[0][3];
    float y = v.x * mvp.m[1][0] + v.y * mvp.m[1][1] + v.z * mvp.m[1][2] + mvp.m[1][3];
    float w = v.x * mvp.m[3][0] + v.y * mvp.m[3][1] + v.z * mvp.m[3][2] + mvp.m[3][3];

    if (w != 0.0f) {
        x /= w;
        y /= w;
    }

    return (RasterVertex_t){
        (x + 1.0f) * 0.5f * GFX_WIDTH,
        (1.0f - y) * 0.5f * GFX_HEIGHT,
        (w != 0.0f) ? (1.0f / w) : 0.0f
    };
}

void ILI9341_SendPixelBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *buffer)
{
    ILI9341_SetAddress(x, y, x + w - 1, y + h - 1);

    uint32_t pixel_count = (uint32_t)w * h;
    for (uint32_t i = 0; i < pixel_count; i++) {
        buffer[i] = RGB565_TO_WIRE_ORDER(buffer[i]);
    }

    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

    uint32_t total_bytes = (uint32_t)w * h * 2;
    uint8_t *ptr = (uint8_t *)buffer;

    while (total_bytes > 0)
    {
        uint16_t chunk_size = (total_bytes > 60000) ? 60000 : (uint16_t)total_bytes;

        // Wait for SPI to be ready (blocking wait to avoid collisions with DMA)
        while (HAL_SPI_GetState(HSPI_INSTANCE) != HAL_SPI_STATE_READY);

        HAL_SPI_Transmit(HSPI_INSTANCE, ptr, chunk_size, 100);

        ptr += chunk_size;
        total_bytes -= chunk_size;
    }

    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

static uint16_t ScaleColorRGB565(uint16_t color, float intensity)
{
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    uint32_t r = (color >> 11) & 0x1F;
    uint32_t g = (color >> 5) & 0x3F;
    uint32_t b = color & 0x1F;

    r = (uint32_t)(r * intensity);
    g = (uint32_t)(g * intensity);
    b = (uint32_t)(b * intensity);

    return (uint16_t)((r << 11) | (g << 5) | b);
}

Vector3_t V3_Add(Vector3_t a, Vector3_t b) { return (Vector3_t){a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3_t V3_Sub(Vector3_t a, Vector3_t b) { return (Vector3_t){a.x - b.x, a.y - b.y, a.z - b.z}; }
float V3_Dot(Vector3_t a, Vector3_t b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float V3_Length(Vector3_t v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }

Vector3_t V3_Cross(Vector3_t a, Vector3_t b)
{
    return (Vector3_t){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vector3_t V3_Normalize(Vector3_t v)
{
    float len = V3_Length(v);
    if (len == 0.0f) return (Vector3_t){0.0f, 0.0f, 0.0f};
    return (Vector3_t){v.x / len, v.y / len, v.z / len};
}

Matrix4_t M4_Identity(void)
{
    Matrix4_t m = {0};
    m.m[0][0] = 1.0f; m.m[1][1] = 1.0f; m.m[2][2] = 1.0f; m.m[3][3] = 1.0f;
    return m;
}

Matrix4_t M4_Scaling(float sx, float sy, float sz)
{
    Matrix4_t m = M4_Identity();
    m.m[0][0] = sx; m.m[1][1] = sy; m.m[2][2] = sz;
    return m;
}

Matrix4_t M4_Translation(float tx, float ty, float tz)
{
    Matrix4_t m = M4_Identity();
    m.m[0][3] = tx; m.m[1][3] = ty; m.m[2][3] = tz;
    return m;
}

Matrix4_t M4_RotationX(float angle_rad)
{
    Matrix4_t m = M4_Identity();
    float c = cosf(angle_rad), s = sinf(angle_rad);
    m.m[1][1] = c;  m.m[1][2] = -s;
    m.m[2][1] = s;  m.m[2][2] = c;
    return m;
}

Matrix4_t M4_RotationY(float angle_rad)
{
    Matrix4_t m = M4_Identity();
    float c = cosf(angle_rad), s = sinf(angle_rad);
    m.m[0][0] = c;  m.m[0][2] = s;
    m.m[2][0] = -s; m.m[2][2] = c;
    return m;
}

Matrix4_t M4_RotationZ(float angle_rad)
{
    Matrix4_t m = M4_Identity();
    float c = cosf(angle_rad), s = sinf(angle_rad);
    m.m[0][0] = c;  m.m[0][1] = -s;
    m.m[1][0] = s;  m.m[1][1] = c;
    return m;
}

Matrix4_t M4_Perspective(float fov_deg, float aspect, float znear, float zfar)
{
    Matrix4_t m = {0};
    float f = 1.0f / tanf(fov_deg * 3.14159265f / 360.0f);
    m.m[0][0] = f / aspect;
    m.m[1][1] = f;
    m.m[2][2] = (zfar + znear) / (znear - zfar);
    m.m[2][3] = (2.0f * zfar * znear) / (znear - zfar);
    m.m[3][2] = -1.0f;
    return m;
}

Matrix4_t M4_LookAt(Vector3_t eye, Vector3_t target, Vector3_t up)
{
    Vector3_t forward = V3_Normalize(V3_Sub(target, eye));
    Vector3_t right = V3_Normalize(V3_Cross(forward, up));
    Vector3_t camera_up = V3_Cross(right, forward);

    Matrix4_t m = M4_Identity();
    m.m[0][0] = right.x;
    m.m[0][1] = right.y;
    m.m[0][2] = right.z;
    m.m[0][3] = -V3_Dot(right, eye);
    m.m[1][0] = camera_up.x;
    m.m[1][1] = camera_up.y;
    m.m[1][2] = camera_up.z;
    m.m[1][3] = -V3_Dot(camera_up, eye);
    m.m[2][0] = -forward.x;
    m.m[2][1] = -forward.y;
    m.m[2][2] = -forward.z;
    m.m[2][3] = V3_Dot(forward, eye);
    return m;
}

Matrix4_t M4_Multiply(Matrix4_t a, Matrix4_t b)
{
    Matrix4_t out = {0};
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out.m[r][c] = a.m[r][0] * b.m[0][c] +
                          a.m[r][1] * b.m[1][c] +
                          a.m[r][2] * b.m[2][c] +
                          a.m[r][3] * b.m[3][c];
        }
    }
    return out;
}

Vector3_t M4_MultiplyVector(Matrix4_t m, Vector3_t v)
{
    float w = v.x * m.m[3][0] + v.y * m.m[3][1] + v.z * m.m[3][2] + m.m[3][3];
    if (w == 0.0f) w = 1.0f;
    return (Vector3_t){
        (v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2] + m.m[0][3]) / w,
        (v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2] + m.m[1][3]) / w,
        (v.x * m.m[2][0] + v.y * m.m[2][1] + v.z * m.m[2][2] + m.m[2][3]) / w
    };
}

Quaternion_t Q_Identity(void)
{
    return (Quaternion_t){1.0f, 0.0f, 0.0f, 0.0f};
}

Quaternion_t Q_Normalize(Quaternion_t q)
{
    float length = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (length == 0.0f) return Q_Identity();

    float inv_length = 1.0f / length;
    return (Quaternion_t){
        q.w * inv_length,
        q.x * inv_length,
        q.y * inv_length,
        q.z * inv_length
    };
}

Quaternion_t Q_Multiply(Quaternion_t a, Quaternion_t b)
{
    return (Quaternion_t){
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w
    };
}

Quaternion_t Q_FromAxisAngle(Vector3_t axis, float angle_rad)
{
    Vector3_t normalized_axis = V3_Normalize(axis);
    float half_angle = angle_rad * 0.5f;
    float scale = sinf(half_angle);

    return (Quaternion_t){
        cosf(half_angle),
        normalized_axis.x * scale,
        normalized_axis.y * scale,
        normalized_axis.z * scale
    };
}

Quaternion_t Q_IntegrateAngularVelocity(Quaternion_t orientation, Vector3_t angular_velocity, float delta_seconds)
{
    float angular_speed = V3_Length(angular_velocity);
    if (angular_speed == 0.0f || delta_seconds <= 0.0f) {
        return orientation;
    }

    Quaternion_t delta_rotation = Q_FromAxisAngle(angular_velocity, angular_speed * delta_seconds);
    return Q_Normalize(Q_Multiply(delta_rotation, orientation));
}

Matrix4_t Q_ToMatrix(Quaternion_t q)
{
    q = Q_Normalize(q);

    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;

    Matrix4_t m = M4_Identity();
    m.m[0][0] = 1.0f - 2.0f * (yy + zz);
    m.m[0][1] = 2.0f * (xy - wz);
    m.m[0][2] = 2.0f * (xz + wy);
    m.m[1][0] = 2.0f * (xy + wz);
    m.m[1][1] = 1.0f - 2.0f * (xx + zz);
    m.m[1][2] = 2.0f * (yz - wx);
    m.m[2][0] = 2.0f * (xz - wy);
    m.m[2][1] = 2.0f * (yz + wx);
    m.m[2][2] = 1.0f - 2.0f * (xx + yy);
    return m;
}

Mesh_t GFX3D_CreateCube(float size, uint16_t color)
{
    Mesh_t mesh;
    mesh.vertex_count = 8;
    mesh.vertices = (Vector3_t *)malloc(8 * sizeof(Vector3_t));
    
    float h = size / 2.0f;
    mesh.vertices[0] = (Vector3_t){-h, -h, -h};
    mesh.vertices[1] = (Vector3_t){ h, -h, -h};
    mesh.vertices[2] = (Vector3_t){ h,  h, -h};
    mesh.vertices[3] = (Vector3_t){-h,  h, -h};
    mesh.vertices[4] = (Vector3_t){-h, -h,  h};
    mesh.vertices[5] = (Vector3_t){ h, -h,  h};
    mesh.vertices[6] = (Vector3_t){ h,  h,  h};
    mesh.vertices[7] = (Vector3_t){-h,  h,  h};

    mesh.face_count = 12;
    mesh.faces = (Face_t *)malloc(12 * sizeof(Face_t));

    mesh.faces[0] = (Face_t){{0, 1, 2}, color, FACE_EDGE_01 | FACE_EDGE_12};
    mesh.faces[1] = (Face_t){{0, 2, 3}, color, FACE_EDGE_12 | FACE_EDGE_20};
    mesh.faces[2] = (Face_t){{5, 4, 7}, color, FACE_EDGE_01 | FACE_EDGE_12};
    mesh.faces[3] = (Face_t){{5, 7, 6}, color, FACE_EDGE_12 | FACE_EDGE_20};
    mesh.faces[4] = (Face_t){{3, 2, 6}, color, FACE_EDGE_01 | FACE_EDGE_12};
    mesh.faces[5] = (Face_t){{3, 6, 7}, color, FACE_EDGE_12 | FACE_EDGE_20};
    mesh.faces[6] = (Face_t){{1, 0, 4}, color, FACE_EDGE_01 | FACE_EDGE_12};
    mesh.faces[7] = (Face_t){{1, 4, 5}, color, FACE_EDGE_12 | FACE_EDGE_20};
    mesh.faces[8] = (Face_t){{4, 0, 3}, color, FACE_EDGE_01 | FACE_EDGE_12};
    mesh.faces[9] = (Face_t){{4, 3, 7}, color, FACE_EDGE_12 | FACE_EDGE_20};
    mesh.faces[10] = (Face_t){{1, 5, 6}, color, FACE_EDGE_01 | FACE_EDGE_12};
    mesh.faces[11] = (Face_t){{1, 6, 2}, color, FACE_EDGE_12 | FACE_EDGE_20};

    return mesh;
}

Mesh_t GFX3D_CreatePyramid(float width, float height, uint16_t color)
{
    Mesh_t mesh;
    mesh.vertex_count = 5;
    mesh.vertices = (Vector3_t *)malloc(5 * sizeof(Vector3_t));

    float w = width / 2.0f;
    float h = height / 2.0f;

    mesh.vertices[0] = (Vector3_t){-w, -h, -w};
    mesh.vertices[1] = (Vector3_t){ w, -h, -w};
    mesh.vertices[2] = (Vector3_t){ w, -h,  w};
    mesh.vertices[3] = (Vector3_t){-w, -h,  w};
    mesh.vertices[4] = (Vector3_t){0.0f, h, 0.0f};

    mesh.face_count = 6;
    mesh.faces = (Face_t *)malloc(6 * sizeof(Face_t));

    mesh.faces[0] = (Face_t){{0, 3, 2}, color, FACE_EDGE_01 | FACE_EDGE_12};
    mesh.faces[1] = (Face_t){{0, 2, 1}, color, FACE_EDGE_12 | FACE_EDGE_20};
    mesh.faces[2] = (Face_t){{0, 1, 4}, color, FACE_ALL_EDGES};
    mesh.faces[3] = (Face_t){{1, 2, 4}, color, FACE_ALL_EDGES};
    mesh.faces[4] = (Face_t){{2, 3, 4}, color, FACE_ALL_EDGES};
    mesh.faces[5] = (Face_t){{3, 0, 4}, color, FACE_ALL_EDGES};

    return mesh;
}

Mesh_t GFX3D_CreateSphere(float radius, int longitude_segments, int latitude_segments, uint16_t color)
{
    Mesh_t mesh;
    mesh.vertex_count = (latitude_segments + 1) * (longitude_segments + 1);
    mesh.vertices = (Vector3_t *)malloc(mesh.vertex_count * sizeof(Vector3_t));

    int v_idx = 0;
    for (int lat = 0; lat <= latitude_segments; lat++)
    {
        float theta = -1.5707963f + 3.14159265f * (float)lat / (float)latitude_segments;
        float sin_theta = sinf(theta);
        float cos_theta = cosf(theta);

        for (int lon = 0; lon <= longitude_segments; lon++)
        {
            float phi = 2.0f * 3.14159265f * (float)lon / (float)longitude_segments;
            float sin_phi = sinf(phi);
            float cos_phi = cosf(phi);

            mesh.vertices[v_idx++] = (Vector3_t){
                radius * cos_theta * cos_phi,
                radius * sin_theta,
                radius * cos_theta * sin_phi
            };
        }
    }

    mesh.face_count = latitude_segments * longitude_segments * 2;
    mesh.faces = (Face_t *)malloc(mesh.face_count * sizeof(Face_t));

    int f_idx = 0;
    int stride = longitude_segments + 1;
    for (int lat = 0; lat < latitude_segments; lat++)
    {
        for (int lon = 0; lon < longitude_segments; lon++)
        {
            int next_lat = lat + 1;
            int next_lon = lon + 1;

            int i0 = lat * stride + lon;
            int i1 = lat * stride + next_lon;
            int i2 = next_lat * stride + lon;
            int i3 = next_lat * stride + next_lon;

            mesh.faces[f_idx++] = (Face_t){{i0, i1, i2}, color, FACE_EDGE_01 | FACE_EDGE_20};
            mesh.faces[f_idx++] = (Face_t){{i1, i3, i2}, color, FACE_EDGE_01 | FACE_EDGE_12};
        }
    }

    return mesh;
}

Mesh_t GFX3D_CreateTorus(float outer_radius, float inner_radius, int radial_segments, int tubular_segments, uint16_t color)
{
    Mesh_t mesh;
    mesh.vertex_count = (radial_segments + 1) * (tubular_segments + 1);
    mesh.vertices = (Vector3_t *)malloc(mesh.vertex_count * sizeof(Vector3_t));

    int v_idx = 0;
    for (int rad = 0; rad <= radial_segments; rad++)
    {
        float u = 2.0f * 3.14159265f * (float)rad / (float)radial_segments;
        float cos_u = cosf(u);
        float sin_u = sinf(u);

        for (int tub = 0; tub <= tubular_segments; tub++)
        {
            float v = 2.0f * 3.14159265f * (float)tub / (float)tubular_segments;
            float cos_v = cosf(v);
            float sin_v = sinf(v);

            mesh.vertices[v_idx++] = (Vector3_t){
                (outer_radius + inner_radius * cos_v) * cos_u,
                (outer_radius + inner_radius * cos_v) * sin_u,
                inner_radius * sin_v
            };
        }
    }

    mesh.face_count = radial_segments * tubular_segments * 2;
    mesh.faces = (Face_t *)malloc(mesh.face_count * sizeof(Face_t));

    int f_idx = 0;
    int stride = tubular_segments + 1;
    for (int rad = 0; rad < radial_segments; rad++)
    {
        for (int tub = 0; tub < tubular_segments; tub++)
        {
            int next_rad = rad + 1;
            int next_tub = tub + 1;

            int i0 = rad * stride + tub;
            int i1 = rad * stride + next_tub;
            int i2 = next_rad * stride + tub;
            int i3 = next_rad * stride + next_tub;

            mesh.faces[f_idx++] = (Face_t){{i0, i1, i2}, color, FACE_EDGE_01 | FACE_EDGE_20};
            mesh.faces[f_idx++] = (Face_t){{i1, i3, i2}, color, FACE_EDGE_01 | FACE_EDGE_12};
        }
    }

    return mesh;
}

void GFX3D_FreeMesh(Mesh_t *mesh)
{
    if (mesh->vertices) {
        free(mesh->vertices);
        mesh->vertices = NULL;
    }
    if (mesh->faces) {
        free(mesh->faces);
        mesh->faces = NULL;
    }
}

static void DrawScanline(int y, float x1, float z1, float x2, float z2, uint16_t color, uint16_t wireframe_color, uint16_t *color_buf, uint16_t *z_buf, int y_start, int is_edge_y)
{
    if (y < y_start || y >= y_start + CHUNK_HEIGHT) return;
    int y_local = y - y_start;

    if (x1 > x2) {
        float tmp_x = x1; x1 = x2; x2 = tmp_x;
        float tmp_z = z1; z1 = z2; z2 = tmp_z;
    }

    int sx = (int)ceilf(x1);
    int ex = (int)floorf(x2);

    if (sx < 0) sx = 0;
    if (ex >= GFX_WIDTH) ex = GFX_WIDTH - 1;

    float dx = x2 - x1;
    float step_z = (dx != 0.0f) ? (z2 - z1) / dx : 0.0f;
    float cur_z = z1 + step_z * ((float)sx - x1);

    uint16_t *row_color = &color_buf[y_local * GFX_WIDTH];
    uint16_t *row_z = &z_buf[y_local * GFX_WIDTH];

    for (int x = sx; x <= ex; x++) {
        uint16_t depth_val = (uint16_t)(cur_z * 65535.0f);
        if (depth_val > row_z[x]) {
            row_z[x] = depth_val;
            if (x == sx || x == ex || is_edge_y) {
                row_color[x] = wireframe_color;
            } else {
                row_color[x] = color;
            }
        }
        cur_z += step_z;
    }
}

static void FillFlatBottomTriangle(RasterVertex_t v0, RasterVertex_t v1, RasterVertex_t v2, uint16_t color, uint16_t wireframe_color, uint16_t *color_buf, uint16_t *z_buf, int y_start, int draw_bottom_edge)
{
    float dy = v1.y - v0.y;
    if (dy <= 0.0f) return;

    float inv_slope1 = (v1.x - v0.x) / dy;
    float inv_slope2 = (v2.x - v0.x) / dy;

    float d1_z = (v1.z - v0.z) / dy;
    float d2_z = (v2.z - v0.z) / dy;

    int start_y = (int)ceilf(v0.y);
    int end_y = (int)floorf(v1.y);

    for (int y = start_y; y <= end_y; y++) {
        float diff_y = (float)y - v0.y;
        float x1 = v0.x + diff_y * inv_slope1;
        float x2 = v0.x + diff_y * inv_slope2;
        float z1 = v0.z + diff_y * d1_z;
        float z2 = v0.z + diff_y * d2_z;

        int is_edge_y = (y == start_y) || (draw_bottom_edge && (y == end_y));
        DrawScanline(y, x1, z1, x2, z2, color, wireframe_color, color_buf, z_buf, y_start, is_edge_y);
    }
}

static void FillFlatTopTriangle(RasterVertex_t v0, RasterVertex_t v1, RasterVertex_t v2, uint16_t color, uint16_t wireframe_color, uint16_t *color_buf, uint16_t *z_buf, int y_start, int draw_top_edge)
{
    float dy = v2.y - v0.y;
    if (dy <= 0.0f) return;

    float inv_slope1 = (v2.x - v0.x) / dy;
    float inv_slope2 = (v2.x - v1.x) / (v2.y - v1.y);

    float d1_z = (v2.z - v0.z) / dy;
    float d2_z = (v2.z - v1.z) / (v2.y - v1.y);

    int start_y = (int)ceilf(v0.y);
    int end_y = (int)floorf(v2.y);

    for (int y = start_y; y <= end_y; y++) {
        float diff_y = (float)y - v0.y;
        float x1 = v0.x + diff_y * inv_slope1;
        float x2 = v1.x + ((float)y - v1.y) * inv_slope2;
        float z1 = v0.z + diff_y * d1_z;
        float z2 = v1.z + ((float)y - v1.y) * d2_z;

        int is_edge_y = (draw_top_edge && (y == start_y)) || (y == end_y);
        DrawScanline(y, x1, z1, x2, z2, color, wireframe_color, color_buf, z_buf, y_start, is_edge_y);
    }
}

static void GFX3D_RasterizeTriangle(RasterVertex_t v0, RasterVertex_t v1, RasterVertex_t v2, uint16_t color, uint16_t wireframe_color, uint16_t *color_buf, uint16_t *z_buf, int y_start)
{
    if (v0.y > v1.y) { RasterVertex_t t = v0; v0 = v1; v1 = t; }
    if (v0.y > v2.y) { RasterVertex_t t = v0; v0 = v2; v2 = t; }
    if (v1.y > v2.y) { RasterVertex_t t = v1; v1 = v2; v2 = t; }

    if (v0.y == v2.y) return;

    if (v0.y == v1.y) {
        FillFlatTopTriangle(v0, v1, v2, color, wireframe_color, color_buf, z_buf, y_start, 1);
    } else if (v1.y == v2.y) {
        FillFlatBottomTriangle(v0, v1, v2, color, wireframe_color, color_buf, z_buf, y_start, 1);
    } else {
        float t = (v1.y - v0.y) / (v2.y - v0.y);
        RasterVertex_t v3;
        v3.y = v1.y;
        v3.x = v0.x + t * (v2.x - v0.x);
        v3.z = v0.z + t * (v2.z - v0.z);

        FillFlatBottomTriangle(v0, v1, v3, color, wireframe_color, color_buf, z_buf, y_start, 0);
        FillFlatTopTriangle(v1, v3, v2, color, wireframe_color, color_buf, z_buf, y_start, 0);
    }
}

static void DrawDepthLine(RasterVertex_t v0, RasterVertex_t v1, uint16_t color, uint16_t *color_buf, uint16_t *z_buf, int y_start)
{
    float dx = v1.x - v0.x;
    float dy = v1.y - v0.y;
    int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (steps < 1) steps = 1;

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int x = (int)roundf(v0.x + dx * t);
        int y = (int)roundf(v0.y + dy * t);

        if (x < 0 || x >= GFX_WIDTH || y < y_start || y >= y_start + CHUNK_HEIGHT) {
            continue;
        }

        float depth = v0.z + (v1.z - v0.z) * t;
        uint16_t depth_val = (uint16_t)(depth * 65535.0f);
        int offset = (y - y_start) * GFX_WIDTH + x;

        if ((uint32_t)depth_val + 32u >= z_buf[offset]) {
            color_buf[offset] = color;
        }
    }
}

void GFX3D_FillChunkRect(uint16_t *buffer, int y_start, int chunk_height,
                         int x, int y, int width, int height, uint16_t color)
{
    int x0 = x < 0 ? 0 : x;
    int x1 = x + width > GFX_WIDTH ? GFX_WIDTH : x + width;
    int y0 = y < y_start ? y_start : y;
    int y1 = y + height > y_start + chunk_height ? y_start + chunk_height : y + height;

    for (int py = y0; py < y1; py++) {
        uint16_t *row = &buffer[(py - y_start) * GFX_WIDTH];
        for (int px = x0; px < x1; px++) row[px] = color;
    }
}

static void DrawChunkChar(uint16_t *buffer, int y_start, int chunk_height,
                          char ch, const uint8_t font[], int x, int y,
                          uint16_t color, uint16_t background_color)
{
    if (ch < 32 || ch > 127) return;

    uint8_t offset = font[0];
    uint8_t width = font[1];
    uint8_t height = font[2];
    uint8_t bytes_per_line = font[3];
    const uint8_t *glyph = &font[((ch - 0x20) * offset) + 4];

    GFX3D_FillChunkRect(buffer, y_start, chunk_height, x, y, width, height,
                        background_color);

    for (int py = 0; py < height; py++) {
        int screen_y = y + py;
        if (screen_y < y_start || screen_y >= y_start + chunk_height) continue;
        for (int px = 0; px < width; px++) {
            int screen_x = x + px;
            if (screen_x < 0 || screen_x >= GFX_WIDTH) continue;
            uint8_t bits = glyph[bytes_per_line * px + ((py & 0xF8) >> 3) + 1];
            if (bits & (1U << (py & 0x07))) {
                buffer[(screen_y - y_start) * GFX_WIDTH + screen_x] = color;
            }
        }
    }
}

void GFX3D_DrawChunkText(uint16_t *buffer, int y_start, int chunk_height,
                         const char *text, const uint8_t font[], int x, int y,
                         uint16_t color, uint16_t background_color)
{
    uint8_t offset = font[0];
    uint8_t font_width = font[1];

    while (*text) {
        DrawChunkChar(buffer, y_start, chunk_height, *text, font, x, y,
                      color, background_color);
        const uint8_t *glyph = &font[((*text - 0x20) * offset) + 4];
        uint8_t character_width = glyph[0];
        x += character_width + 2 < font_width ? character_width + 2 : font_width;
        text++;
    }
}

static void DrawChunkPixel(uint16_t *buffer, int y_start, int y_end, int x, int y, uint16_t color)
{
    if (x >= 0 && x < GFX_WIDTH && y >= y_start && y <= y_end) {
        buffer[(y - y_start) * GFX_WIDTH + x] = color;
    }
}

static void DrawChunkCircle(uint16_t *buffer, int y_start, int y_end, Point_t center, uint16_t radius, uint16_t color)
{
    int32_t cx = center.x;
    int32_t cy = center.y;
    int32_t x = radius - 1;
    int32_t y = 0;
    int32_t dx = 1;
    int32_t dy = 1;
    int32_t err = dx - ((int32_t)radius << 1);

    while (x >= y) {
        int32_t pts[8][2] = {
            {cx + x, cy + y}, {cx + y, cy + x},
            {cx - y, cy + x}, {cx - x, cy + y},
            {cx - x, cy - y}, {cx - y, cy - x},
            {cx + y, cy - x}, {cx + x, cy - y}
        };
        for (int i = 0; i < 8; i++) {
            DrawChunkPixel(buffer, y_start, y_end, pts[i][0], pts[i][1], color);
        }
        if (err <= 0) {
            y++;
            err += dy;
            dy += 2;
        }
        if (err > 0) {
            x--;
            dx += 2;
            err += (-(int32_t)(radius << 1)) + dx;
        }
    }
}

static void DrawChunkLine(uint16_t *buffer, int y_start, int y_end, Point_t p1, Point_t p2, uint16_t color)
{
    int x0 = p1.x, y0 = p1.y, x1 = p2.x, y1 = p2.y;
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        DrawChunkPixel(buffer, y_start, y_end, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

static void DrawChunkBackground(uint16_t *buffer, int y_start, int y_end)
{
    for (int i = 0; i < GFX_WIDTH * CHUNK_HEIGHT; i++) {
        buffer[i] = BLACK;
    }

    // 1. Draw stars (Chunk 0, y_start = 0, y_end = 119)
    DrawChunkPixel(buffer, y_start, y_end, 30, 25, WHITE);
    DrawChunkPixel(buffer, y_start, y_end, 75, 80, LIGHTGREY);
    DrawChunkPixel(buffer, y_start, y_end, 140, 15, WHITE);
    DrawChunkPixel(buffer, y_start, y_end, 180, 20, WHITE);
    DrawChunkPixel(buffer, y_start, y_end, 280, 40, LIGHTGREY);
    DrawChunkPixel(buffer, y_start, y_end, 100, 110, LIGHTGREY);

    // 2. Draw Concentric Neon Sun
    DrawChunkCircle(buffer, y_start, y_end, (Point_t){160, 130}, 45, RED);
    DrawChunkCircle(buffer, y_start, y_end, (Point_t){160, 130}, 40, ORANGE);
    DrawChunkCircle(buffer, y_start, y_end, (Point_t){160, 130}, 35, YELLOW);

    // 3. Draw Horizon line (Y = 170)
    GFX3D_FillChunkRect(buffer, y_start, CHUNK_HEIGHT, 0, 170, GFX_WIDTH, 1, PURPLE);

    // 4. Draw perspective lines
    for (int16_t x = -80; x <= 400; x += 60) {
        DrawChunkLine(buffer, y_start, y_end, (Point_t){160, 170}, (Point_t){x, 240}, PURPLE);
    }

    // 5. Draw horizontal grid lines
    GFX3D_FillChunkRect(buffer, y_start, CHUNK_HEIGHT, 0, 172, GFX_WIDTH, 1, PURPLE);
    GFX3D_FillChunkRect(buffer, y_start, CHUNK_HEIGHT, 0, 176, GFX_WIDTH, 1, PURPLE);
    GFX3D_FillChunkRect(buffer, y_start, CHUNK_HEIGHT, 0, 183, GFX_WIDTH, 1, PURPLE);
    GFX3D_FillChunkRect(buffer, y_start, CHUNK_HEIGHT, 0, 194, GFX_WIDTH, 1, PURPLE);
    GFX3D_FillChunkRect(buffer, y_start, CHUNK_HEIGHT, 0, 210, GFX_WIDTH, 1, PURPLE);
    GFX3D_FillChunkRect(buffer, y_start, CHUNK_HEIGHT, 0, 230, GFX_WIDTH, 1, PURPLE);
}

void GFX3D_RenderObjects(const GFX3D_RenderObject_t *objects, int object_count, Matrix4_t view, Matrix4_t proj, GFX3D_RenderOptions_t options)
{
    if (!objects || object_count <= 0) return;

    Vector3_t light_dir = {0.577f, -0.577f, 0.577f};
    RasterVertex_t axis_origin = {0};
    RasterVertex_t axis_x = {0};
    RasterVertex_t axis_y = {0};
    RasterVertex_t axis_z = {0};

    if (options.show_axes) {
        const float axis_length = 1.4f;
        Matrix4_t axis_mvp = M4_Multiply(proj, M4_Multiply(view, objects[0].world));
        axis_origin = ProjectVertex(axis_mvp, (Vector3_t){0.0f, 0.0f, 0.0f});
        axis_x = ProjectVertex(axis_mvp, (Vector3_t){axis_length, 0.0f, 0.0f});
        axis_y = ProjectVertex(axis_mvp, (Vector3_t){0.0f, axis_length, 0.0f});
        axis_z = ProjectVertex(axis_mvp, (Vector3_t){0.0f, 0.0f, axis_length});
    }

    for (int chunk = 0; chunk < NUM_CHUNKS; chunk++) {
        int y_start = chunk * CHUNK_HEIGHT;
        int y_end = y_start + CHUNK_HEIGHT - 1;

        for (int i = 0; i < GFX_WIDTH * CHUNK_HEIGHT; i++) {
            chunk_z_buffer[i] = 0;
        }

        if (options.background_color == 0x0841U) {
            DrawChunkBackground(chunk_color_buffer, y_start, y_end);
        } else {
            for (int i = 0; i < GFX_WIDTH * CHUNK_HEIGHT; i++) {
                chunk_color_buffer[i] = options.background_color;
            }
        }

        for (int object_index = 0; object_index < object_count; object_index++) {
            const GFX3D_RenderObject_t *object = &objects[object_index];
            const Mesh_t *mesh = object->mesh;
            if (!mesh || mesh->vertex_count <= 0 || mesh->vertex_count > GFX3D_MAX_VERTICES) continue;

            Matrix4_t world = object->world;
            Matrix4_t mvp = M4_Multiply(proj, M4_Multiply(view, world));
            for (int i = 0; i < mesh->vertex_count; i++) {
                projected_vertices[i] = ProjectVertex(mvp, mesh->vertices[i]);
            }

        for (int f = 0; f < mesh->face_count; f++) {
            Face_t face = mesh->faces[f];
            RasterVertex_t rv0 = projected_vertices[face.v[0]];
            RasterVertex_t rv1 = projected_vertices[face.v[1]];
            RasterVertex_t rv2 = projected_vertices[face.v[2]];

            float min_y = rv0.y;
            if (rv1.y < min_y) min_y = rv1.y;
            if (rv2.y < min_y) min_y = rv2.y;

            float max_y = rv0.y;
            if (rv1.y > max_y) max_y = rv1.y;
            if (rv2.y > max_y) max_y = rv2.y;

            if (max_y < (float)y_start || min_y > (float)y_end) {
                continue;
            }

            float area = (rv1.x - rv0.x) * (rv2.y - rv0.y) - (rv2.x - rv0.x) * (rv1.y - rv0.y);
            if (area <= 0.0f) {
                continue; 
            }

            Vector3_t w0 = mesh->vertices[face.v[0]];
            Vector3_t w1 = mesh->vertices[face.v[1]];
            Vector3_t w2 = mesh->vertices[face.v[2]];

            Vector3_t world_v0 = M4_MultiplyVector(world, w0);
            Vector3_t world_v1 = M4_MultiplyVector(world, w1);
            Vector3_t world_v2 = M4_MultiplyVector(world, w2);

            Vector3_t edge1 = V3_Sub(world_v1, world_v0);
            Vector3_t edge2 = V3_Sub(world_v2, world_v0);
            Vector3_t normal = V3_Normalize(V3_Cross(edge1, edge2));

            float intensity = -V3_Dot(normal, light_dir);
            if (intensity < 0.0f) intensity = 0.0f;

            float ambient = 0.55f;
            float factor = ambient + (1.0f - ambient) * intensity;

            uint16_t shaded_color = ScaleColorRGB565(object->color, factor);

            // Fill only. Selected mesh edges are overlaid below, so triangulation
            // diagonals never become wireframe lines.
            GFX3D_RasterizeTriangle(rv0, rv1, rv2, shaded_color, shaded_color, chunk_color_buffer, chunk_z_buffer, y_start);

            if (options.show_wireframe) {
                if (face.wire_edges & FACE_EDGE_01) {
                    DrawDepthLine(rv0, rv1, WHITE, chunk_color_buffer, chunk_z_buffer, y_start);
                }
                if (face.wire_edges & FACE_EDGE_12) {
                    DrawDepthLine(rv1, rv2, WHITE, chunk_color_buffer, chunk_z_buffer, y_start);
                }
                if (face.wire_edges & FACE_EDGE_20) {
                    DrawDepthLine(rv2, rv0, WHITE, chunk_color_buffer, chunk_z_buffer, y_start);
                }
            }
        }
        }

        if (options.show_axes) {
            DrawDepthLine(axis_origin, axis_x, RED, chunk_color_buffer, chunk_z_buffer, y_start);
            DrawDepthLine(axis_origin, axis_y, GREEN, chunk_color_buffer, chunk_z_buffer, y_start);
            DrawDepthLine(axis_origin, axis_z, BLUE, chunk_color_buffer, chunk_z_buffer, y_start);
        }

        if (options.overlay) {
            options.overlay(chunk_color_buffer, y_start, CHUNK_HEIGHT,
                            options.overlay_context);
        }

        ILI9341_SendPixelBuffer(0, y_start, GFX_WIDTH, CHUNK_HEIGHT, chunk_color_buffer);
    }
}

void GFX3D_RenderScene(Mesh_t *mesh, Matrix4_t world, Matrix4_t view, Matrix4_t proj, uint16_t bgcolor, uint8_t show_wireframe)
{
    GFX3D_RenderObject_t object = {mesh, world, mesh->faces[0].color};
    GFX3D_RenderOptions_t options = {bgcolor, show_wireframe, 1U, NULL, NULL};
    GFX3D_RenderObjects(&object, 1, view, proj, options);
}
