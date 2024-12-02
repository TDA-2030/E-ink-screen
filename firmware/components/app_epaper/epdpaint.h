/**
 *  @filename   :   epdpaint.h
 *  @brief      :   Header file for epdpaint.cpp
 *  @author     :   Yehui from Waveshare
 *  
 *  Copyright (C) Waveshare     July 28 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef EPDPAINT_H
#define EPDPAINT_H

#include <stdint.h>
#include "font/fonts.h"


#ifdef __cplusplus 
extern "C" {
#endif

// Display orientation
#define ROTATE_0            0
#define ROTATE_90           1
#define ROTATE_180          2
#define ROTATE_270          3

// Color inverse. 1 or 0 = set or reset a bit if set a colored pixel
#define IF_INVERT_COLOR     0



typedef struct {
    uint8_t *fbuffer;
    uint32_t fbuffer_size;
    uint16_t width;
    uint16_t height;
    uint16_t width_memory;
    uint16_t height_memory;
    uint16_t color;
    uint16_t rotate;
    uint16_t width_byte;
    uint16_t height_byte;
} epd_paint_t;

epd_paint_t *Paint_create(int width, int height);
void Paint_delete(epd_paint_t *_paint);
void Paint_Clear(const epd_paint_t *paint, int colored);
void Paint_DrawAbsolutePixel(const epd_paint_t *paint, uint16_t x, uint16_t y, int colored);
void Paint_DrawPixel(const epd_paint_t *paint, int x, int y, int colored);
void Paint_DrawCharAt(const epd_paint_t *paint, int x, int y, char ascii_char, const sFONT* font, int colored);
void Paint_DrawStringAt(const epd_paint_t *paint, int x, int y, const char* text, const sFONT* font, int colored);
void Paint_DrawLine(const epd_paint_t *paint, int x0, int y0, int x1, int y1, int colored);
void Paint_DrawHorizontalLine(const epd_paint_t *paint, int x, int y, int width, int colored);
void Paint_DrawVerticalLine(const epd_paint_t *paint, int x, int y, int height, int colored);
void Paint_DrawRectangle(const epd_paint_t *paint, int x0, int y0, int x1, int y1, int colored);
void Paint_DrawFilledRectangle(const epd_paint_t *paint, int x0, int y0, int x1, int y1, int colored);
void Paint_DrawCircle(const epd_paint_t *paint, int x, int y, int radius, int colored);
void Paint_DrawFilledCircle(const epd_paint_t *paint, int x, int y, int radius, int colored);
void Paint_DrawImage(const epd_paint_t *paint, int x, int y, int width, int height, uint8_t **img);

int Paint_gbk_char(const epd_paint_t *paint, uint16_t x, uint16_t y, const char *gbk, const sFONT* font, uint8_t mode);
int Paint_text_str(const epd_paint_t *paint, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *string, const sFONT* font, uint8_t mode);

#ifdef __cplusplus 
}
#endif

#endif

/* END OF FILE */

