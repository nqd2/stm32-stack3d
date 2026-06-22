/**
  ******************************************************************************
  * @file    graphics_2d.h
  * @brief   2D Graphics primitives for ILI9341 on STM32F429-DISC1
  * @author  Nguyen Quy Duc - Hanoi University of Science and Technology
  ******************************************************************************
  */

#ifndef GRAPHICS_2D_H
#define GRAPHICS_2D_H

#include "stm32f4xx_hal.h"
#include "Display/ILI9341_STM32_Driver.h"
#include "Display/ILI9341_GFX.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

/* ---- Screen configuration ---- */
#define SCREEN_ROTATION SCREEN_HORIZONTAL_1

#if (SCREEN_ROTATION == SCREEN_VERTICAL_1 || SCREEN_ROTATION == SCREEN_VERTICAL_2)
  #define GFX_WIDTH  240
  #define GFX_HEIGHT 320
#else
  #define GFX_WIDTH  320
  #define GFX_HEIGHT 240
#endif

/* ---- Types ---- */

typedef struct {
  int32_t x;
  int32_t y;
} Point_t;

typedef struct {
  float x;
  float y;
} OxyPoint_t;

Point_t Oxy_to_LCD(OxyPoint_t p);

void GFX_DrawPoint(Point_t p, uint16_t color);
void GFX_DrawLine(Point_t p1, Point_t p2, uint16_t color);
void GFX_DrawLineAA(Point_t p1, Point_t p2, uint16_t color, uint16_t bgcolor);
void GFX_DrawRectangle(Point_t p1, Point_t p2, uint16_t color);
void GFX_DrawCircle(Point_t center, uint16_t radius, uint16_t color);
void GFX_DrawAxis(uint16_t color);
void GFX_DrawFunction(float (*f)(float), uint16_t color);
void GFX_DrawFunctionAA(float (*f)(float), uint16_t color, uint16_t bgcolor);
void GFX_FillBetweenFunctions(float (*f_top)(float), float (*f_bottom)(float), uint16_t color);
void GFX_DrawCursor(int16_t x, int16_t y, uint16_t color);

void GFX_SetZoom(uint8_t zoom);
uint8_t GFX_GetZoom(void);

#endif /* GRAPHICS_2D_H */
