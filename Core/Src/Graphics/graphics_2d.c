/**
  ******************************************************************************
  * @file    graphics_2d.c
  * @brief   2D Graphics primitives
  ******************************************************************************
  */

#include "Graphics/graphics_2d.h"

static uint8_t gfx_zoom = 1;

Point_t Oxy_to_LCD(OxyPoint_t p)
{
  Point_t lcd;
  lcd.x = (int32_t)roundf(p.x * gfx_zoom + (GFX_WIDTH  / 2.0f));
  lcd.y = (int32_t)roundf(-p.y * gfx_zoom + (GFX_HEIGHT / 2.0f));
  return lcd;
}

void GFX_DrawPoint(Point_t p, uint16_t color)
{
  if (p.x < 0 || p.x >= GFX_WIDTH || p.y < 0 || p.y >= GFX_HEIGHT)
    return;
  ILI9341_DrawPixel((uint16_t)p.x, (uint16_t)p.y, color);
}

/* ---- Cohen-Sutherland line clipping ---- */

#define CS_INSIDE 0
#define CS_LEFT   1
#define CS_RIGHT  2
#define CS_BOTTOM 4
#define CS_TOP    8

static int CS_OutCode(int x, int y)
{
  int code = CS_INSIDE;
  if (x < 0)           code |= CS_LEFT;
  else if (x >= GFX_WIDTH)  code |= CS_RIGHT;
  if (y < 0)           code |= CS_TOP;
  else if (y >= GFX_HEIGHT) code |= CS_BOTTOM;
  return code;
}

static int CS_Clip(int *x0, int *y0, int *x1, int *y1)
{
  int oc0 = CS_OutCode(*x0, *y0);
  int oc1 = CS_OutCode(*x1, *y1);

  for (;;)
  {
    if (!(oc0 | oc1)) return 1;
    if (oc0 & oc1) return 0;

    int oc = oc0 ? oc0 : oc1;
    int x, y;

    if (oc & CS_TOP)         { x = *x0 + (int)((long)(*x1 - *x0) * (0 - *y0)               / (*y1 - *y0)); y = 0; }
    else if (oc & CS_BOTTOM) { x = *x0 + (int)((long)(*x1 - *x0) * (GFX_HEIGHT - 1 - *y0)   / (*y1 - *y0)); y = GFX_HEIGHT - 1; }
    else if (oc & CS_RIGHT)  { y = *y0 + (int)((long)(*y1 - *y0) * (GFX_WIDTH  - 1 - *x0)   / (*x1 - *x0)); x = GFX_WIDTH  - 1; }
    else                     { y = *y0 + (int)((long)(*y1 - *y0) * (0 - *x0)                 / (*x1 - *x0)); x = 0; }

    if (oc == oc0) { *x0 = x; *y0 = y; oc0 = CS_OutCode(x, y); }
    else           { *x1 = x; *y1 = y; oc1 = CS_OutCode(x, y); }
  }
}

void GFX_DrawLine(Point_t p1, Point_t p2, uint16_t color)
{
  int x0 = p1.x, y0 = p1.y, x1 = p2.x, y1 = p2.y;

  if (!CS_Clip(&x0, &y0, &x1, &y1)) return;

  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx - dy;

  for (;;)
  {
    ILI9341_DrawPixel((uint16_t)x0, (uint16_t)y0, color);

    if (x0 == x1 && y0 == y1) break;

    int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x0 += sx; }
    if (e2 <  dx) { err += dx; y0 += sy; }
  }
}

/* ---- Xiaolin Wu anti-aliased line ---- */

static uint16_t BlendRGB565(uint16_t fg, uint16_t bg, uint8_t alpha)
{
  uint32_t inv = 255 - alpha;

  uint32_t r = ((fg >> 11) * alpha + (bg >> 11) * inv) / 255;
  uint32_t g = (((fg >> 5) & 0x3F) * alpha + ((bg >> 5) & 0x3F) * inv) / 255;
  uint32_t b = ((fg & 0x1F) * alpha + (bg & 0x1F) * inv) / 255;

  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void PlotAA(int x, int y, uint16_t fg, uint16_t bg, uint8_t alpha)
{
  if (x < 0 || x >= GFX_WIDTH || y < 0 || y >= GFX_HEIGHT) return;
  ILI9341_DrawPixel((uint16_t)x, (uint16_t)y, BlendRGB565(fg, bg, alpha));
}

static float fpart(float x)  { return x - floorf(x); }
static float rfpart(float x) { return 1.0f - fpart(x); }

void GFX_DrawLineAA(Point_t p1, Point_t p2, uint16_t color, uint16_t bgcolor)
{
  float x0 = (float)p1.x, y0 = (float)p1.y;
  float x1 = (float)p2.x, y1 = (float)p2.y;

  int steep = fabsf(y1 - y0) > fabsf(x1 - x0);

  if (steep) { float t; t=x0; x0=y0; y0=t; t=x1; x1=y1; y1=t; }
  if (x0 > x1) { float t; t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }

  float dx = x1 - x0;
  float dy = y1 - y0;
  float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

  int xpxl1 = (int)roundf(x0);
  float yend = y0 + gradient * (xpxl1 - x0);
  uint8_t a1 = (uint8_t)(rfpart(yend) * 255);
  int ypxl1 = (int)floorf(yend);
  if (steep) { PlotAA(ypxl1, xpxl1, color, bgcolor, a1); PlotAA(ypxl1+1, xpxl1, color, bgcolor, 255-a1); }
  else       { PlotAA(xpxl1, ypxl1, color, bgcolor, a1); PlotAA(xpxl1, ypxl1+1, color, bgcolor, 255-a1); }
  float intery = yend + gradient;

  int xpxl2 = (int)roundf(x1);
  yend = y1 + gradient * (xpxl2 - x1);
  a1 = (uint8_t)(rfpart(yend) * 255);
  int ypxl2 = (int)floorf(yend);
  if (steep) { PlotAA(ypxl2, xpxl2, color, bgcolor, a1); PlotAA(ypxl2+1, xpxl2, color, bgcolor, 255-a1); }
  else       { PlotAA(xpxl2, ypxl2, color, bgcolor, a1); PlotAA(xpxl2, ypxl2+1, color, bgcolor, 255-a1); }

  for (int x = xpxl1 + 1; x < xpxl2; x++)
  {
    uint8_t a = (uint8_t)(rfpart(intery) * 255);
    int iy = (int)floorf(intery);
    if (steep) { PlotAA(iy, x, color, bgcolor, a); PlotAA(iy+1, x, color, bgcolor, 255-a); }
    else       { PlotAA(x, iy, color, bgcolor, a); PlotAA(x, iy+1, color, bgcolor, 255-a); }
    intery += gradient;
  }
}

void GFX_DrawRectangle(Point_t p1, Point_t p2, uint16_t color)
{
  Point_t tl = {(p1.x < p2.x) ? p1.x : p2.x, (p1.y < p2.y) ? p1.y : p2.y};
  Point_t br = {(p1.x > p2.x) ? p1.x : p2.x, (p1.y > p2.y) ? p1.y : p2.y};
  Point_t tr = {br.x, tl.y};
  Point_t bl = {tl.x, br.y};

  GFX_DrawLine(tl, tr, color);
  GFX_DrawLine(tr, br, color);
  GFX_DrawLine(br, bl, color);
  GFX_DrawLine(bl, tl, color);
}

void GFX_DrawCircle(Point_t center, uint16_t radius, uint16_t color)
{
  int32_t cx = center.x;
  int32_t cy = center.y;

  if (cx + radius < 0 || cx - radius >= GFX_WIDTH ||
      cy + radius < 0 || cy - radius >= GFX_HEIGHT)
    return;

  int32_t x = radius - 1;
  int32_t y = 0;
  int32_t dx = 1;
  int32_t dy = 1;
  int32_t err = dx - ((int32_t)radius << 1);

  while (x >= y)
  {
    int32_t pts[8][2] = {
      {cx + x, cy + y}, {cx + y, cy + x},
      {cx - y, cy + x}, {cx - x, cy + y},
      {cx - x, cy - y}, {cx - y, cy - x},
      {cx + y, cy - x}, {cx + x, cy - y}
    };
    for (int i = 0; i < 8; i++)
    {
      int32_t px = pts[i][0];
      int32_t py = pts[i][1];
      if (px >= 0 && px < GFX_WIDTH && py >= 0 && py < GFX_HEIGHT)
        ILI9341_DrawPixel((uint16_t)px, (uint16_t)py, color);
    }

    if (err <= 0)
    {
      y++;
      err += dy;
      dy += 2;
    }
    if (err > 0)
    {
      x--;
      dx += 2;
      err += (-(int32_t)(radius << 1)) + dx;
    }
  }
}

void GFX_DrawAxis(uint16_t color)
{
  ILI9341_DrawHLine(0, GFX_HEIGHT / 2, GFX_WIDTH,  color);
  ILI9341_DrawVLine(GFX_WIDTH / 2, 0,  GFX_HEIGHT, color);
}

static int SameOutsideSide(Point_t a, Point_t b)
{
  if (a.y < 0           && b.y < 0)           return 1;
  if (a.y >= GFX_HEIGHT && b.y >= GFX_HEIGHT) return 1;
  if (a.x < 0           && b.x < 0)           return 1;
  if (a.x >= GFX_WIDTH  && b.x >= GFX_WIDTH)  return 1;
  return 0;
}

void GFX_DrawFunction(float (*f)(float), uint16_t color)
{
  int half_w = GFX_WIDTH / 2;
  float step = 1.0f / gfx_zoom;
  float x_start = (float)(-half_w) / gfx_zoom;
  float x_end   = (float)(half_w)  / gfx_zoom;

  float y_prev = f(x_start);

  for (float xf = x_start; xf < x_end; xf += step)
  {
    float xf_next = xf + step;
    float y_next = f(xf_next);

    if (!isnan(y_prev) && !isnan(y_next))
    {
      Point_t a = Oxy_to_LCD((OxyPoint_t){xf,     y_prev});
      Point_t b = Oxy_to_LCD((OxyPoint_t){xf_next, y_next});

      if (!SameOutsideSide(a, b))
        GFX_DrawLine(a, b, color);
    }

    y_prev = y_next;
  }
}

void GFX_DrawFunctionAA(float (*f)(float), uint16_t color, uint16_t bgcolor)
{
  int half_w = GFX_WIDTH / 2;
  float step = 1.0f / gfx_zoom;
  float x_start = (float)(-half_w) / gfx_zoom;
  float x_end   = (float)(half_w)  / gfx_zoom;

  float y_prev = f(x_start);

  for (float xf = x_start; xf < x_end; xf += step)
  {
    float xf_next = xf + step;
    float y_next = f(xf_next);

    if (!isnan(y_prev) && !isnan(y_next))
    {
      Point_t a = Oxy_to_LCD((OxyPoint_t){xf,     y_prev});
      Point_t b = Oxy_to_LCD((OxyPoint_t){xf_next, y_next});

      if (!SameOutsideSide(a, b))
        GFX_DrawLineAA(a, b, color, bgcolor);
    }

    y_prev = y_next;
  }
}

void GFX_FillBetweenFunctions(float (*f_top)(float), float (*f_bottom)(float), uint16_t color)
{
  int half_w = GFX_WIDTH / 2;
  float step = 1.0f / gfx_zoom;
  float x_start = (float)(-half_w) / gfx_zoom;
  float x_end   = (float)(half_w)  / gfx_zoom;

  for (float xf = x_start; xf < x_end; xf += step)
  {
    float yt = f_top(xf);
    float yb = f_bottom(xf);

    if (!isnan(yt) && !isnan(yb))
    {
      Point_t pt = Oxy_to_LCD((OxyPoint_t){xf, yt});
      Point_t pb = Oxy_to_LCD((OxyPoint_t){xf, yb});

      int32_t y_min = (pt.y < pb.y) ? pt.y : pb.y;
      int32_t y_max = (pt.y > pb.y) ? pt.y : pb.y;

      if (pt.x >= 0 && pt.x < GFX_WIDTH)
      {
        if (y_min < 0) y_min = 0;
        if (y_max >= GFX_HEIGHT) y_max = GFX_HEIGHT - 1;
        
        ILI9341_DrawVLine((uint16_t)pt.x, (uint16_t)y_min, (uint16_t)(y_max - y_min + 1), color);
      }
    }
  }
}

void GFX_SetZoom(uint8_t zoom)
{
  if (zoom < 1) zoom = 1;
  gfx_zoom = zoom;
}

uint8_t GFX_GetZoom(void)
{
  return gfx_zoom;
}

void GFX_DrawCursor(int16_t x, int16_t y, uint16_t color)
{
  GFX_DrawCircle((Point_t){x, y}, 6, color);
  for (int16_t dy = -1; dy <= 1; dy++)
  {
    for (int16_t dx = -1; dx <= 1; dx++)
    {
      GFX_DrawPoint((Point_t){x + dx, y + dy}, color);
    }
  }
}
