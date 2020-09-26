/**
 *  @filename   :   epdpaint.cpp
 *  @brief      :   Paint tools
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     September 9 2017
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

#include "epdpaint.h"

static const char *TAG = "paint";

static uint16_t g_width;
static uint16_t g_height;
static int g_rotate;
static uint8_t *g_image;


void Paint_init(unsigned char *image, int width, int height)
{
    g_rotate = ROTATE_0;
    g_image = image;
    /* 1 byte = 8 pixels, so the width should be the multiple of 8 */
    g_width = width % 8 ? width + 8 - (width % 8) : width;
    g_height = height;
}

uint8_t *Paint_GetImage(void)
{
    return g_image;
}

/**
 *  @brief: clear the image
 */
void Paint_Clear(int colored)
{
    for (int x = 0; x < g_width; x++) {
        for (int y = 0; y < g_height; y++) {
            Paint_DrawAbsolutePixel(x, y, colored);
        }
    }
}

/**
 *  @brief: this draws a pixel by absolute coordinates.
 *          this function won't be affected by the rotate parameter.
 */
void Paint_DrawAbsolutePixel(int x, int y, int colored)
{
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
        return;
    }
    if (IF_INVERT_COLOR) {
        if (colored) {
            g_image[(x + y * g_width) / 8] |= 0x80 >> (x % 8);
        } else {
            g_image[(x + y * g_width) / 8] &= ~(0x80 >> (x % 8));
        }
    } else {
        if (colored) {
            g_image[(x + y * g_width) / 8] &= ~(0x80 >> (x % 8));
        } else {
            g_image[(x + y * g_width) / 8] |= 0x80 >> (x % 8);
        }
    }
}

int Paint_GetRotate(void)
{
    return g_rotate;
}

void Paint_SetRotate(int Rotate)
{
    if (Rotate == ROTATE_0 || Rotate == ROTATE_90 || Rotate == ROTATE_180 || Rotate == ROTATE_270) {
        ESP_LOGI(TAG, "Set image Rotate %d", Rotate);
        g_rotate = Rotate;
    } else {
        ESP_LOGE(TAG, "rotate = 0, 90, 180, 270");
    }
}

/**
 *  @brief: this draws a pixel by the coordinates
 */
void Paint_DrawPixel(int x, int y, int colored)
{
    int point_temp;
    if (g_rotate == ROTATE_0) {
        if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
            return;
        }
        Paint_DrawAbsolutePixel(x, y, colored);
    } else if (g_rotate == ROTATE_90) {
        if (x < 0 || x >= g_height || y < 0 || y >= g_width) {
            return;
        }
        point_temp = x;
        x = g_width - y;
        y = point_temp;
        Paint_DrawAbsolutePixel(x, y, colored);
    } else if (g_rotate == ROTATE_180) {
        if (x < 0 || x >= g_width || y < 0 || y >= g_height) {
            return;
        }
        x = g_width - x;
        y = g_height - y;
        Paint_DrawAbsolutePixel(x, y, colored);
    } else if (g_rotate == ROTATE_270) {
        if (x < 0 || x >= g_height || y < 0 || y >= g_width) {
            return;
        }
        point_temp = x;
        x = y;
        y = g_height - point_temp;
        Paint_DrawAbsolutePixel(x, y, colored);
    }
}

/**
 *  @brief: this draws a charactor on the frame buffer but not refresh
 */
void Paint_DrawCharAt(int x, int y, char ascii_char, sFONT *font, int colored)
{
    int i, j;
    unsigned int char_offset = (ascii_char - ' ') * font->Height * (font->Width / 8 + (font->Width % 8 ? 1 : 0));
    const unsigned char *ptr = &font->table[char_offset];

    for (j = 0; j < font->Height; j++) {
        for (i = 0; i < font->Width; i++) {
            if (*(ptr) & (0x80 >> (i % 8))) {
                Paint_DrawPixel(x + i, y + j, colored);
            } else {
                Paint_DrawPixel(x + i, y + j, !colored);
            }
            if (i % 8 == 7) {
                ptr++;
            }
        }
        if (font->Width % 8 != 0) {
            ptr++;
        }
    }
}

/**
*  @brief: this displays a string on the frame buffer but not refresh
*/
void Paint_DrawStringAt(int x, int y, const char *text, sFONT *font, int colored)
{
    const char *p_text = text;
    unsigned int counter = 0;
    int refcolumn = x;

    /* Send the string character by character on EPD */
    while (*p_text != 0) {
        /* Display one character on EPD */
        Paint_DrawCharAt(refcolumn, y, *p_text, font, colored);
        /* Decrement the column position by 16 */
        refcolumn += font->Width;
        /* Point on the next character */
        p_text++;
        counter++;
    }
}

/**
*  @brief: this displays a image on the frame buffer but not refresh
*/
void Paint_DrawImage(int x, int y, int width, int height, uint8_t **img)
{

    for (size_t j = 0; j < height; j++) {
        for (size_t i = 0; i < width; i++) {
            Paint_DrawPixel(x + i, y + j, img[j][i]);
        }
    }
}

/**
*  @brief: this draws a line on the frame buffer
*/
void Paint_DrawLine(int x0, int y0, int x1, int y1, int colored)
{
    /* Bresenham algorithm */
    int dx = x1 - x0 >= 0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 - y0 <= 0 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while ((x0 != x1) && (y0 != y1)) {
        Paint_DrawPixel(x0, y0, colored);
        if (2 * err >= dy) {
            err += dy;
            x0 += sx;
        }
        if (2 * err <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
*  @brief: this draws a horizontal line on the frame buffer
*/
void Paint_DrawHorizontalLine(int x, int y, int line_width, int colored)
{
    int i;
    for (i = x; i < x + line_width; i++) {
        Paint_DrawPixel(i, y, colored);
    }
}

/**
*  @brief: this draws a vertical line on the frame buffer
*/
void Paint_DrawVerticalLine(int x, int y, int line_height, int colored)
{
    int i;
    for (i = y; i < y + line_height; i++) {
        Paint_DrawPixel(x, i, colored);
    }
}

/**
*  @brief: this draws a rectangle
*/
void Paint_DrawRectangle(int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    Paint_DrawHorizontalLine(min_x, min_y, max_x - min_x + 1, colored);
    Paint_DrawHorizontalLine(min_x, max_y, max_x - min_x + 1, colored);
    Paint_DrawVerticalLine(min_x, min_y, max_y - min_y + 1, colored);
    Paint_DrawVerticalLine(max_x, min_y, max_y - min_y + 1, colored);
}

/**
*  @brief: this draws a filled rectangle
*/
void Paint_DrawFilledRectangle(int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    int i;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    for (i = min_x; i <= max_x; i++) {
        Paint_DrawVerticalLine(i, min_y, max_y - min_y + 1, colored);
    }
}

/**
*  @brief: this draws a circle
*/
void Paint_DrawCircle(int x, int y, int radius, int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        Paint_DrawPixel(x - x_pos, y + y_pos, colored);
        Paint_DrawPixel(x + x_pos, y + y_pos, colored);
        Paint_DrawPixel(x + x_pos, y - y_pos, colored);
        Paint_DrawPixel(x - x_pos, y - y_pos, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}

/**
*  @brief: this draws a filled circle
*/
void Paint_DrawFilledCircle(int x, int y, int radius, int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        Paint_DrawPixel(x - x_pos, y + y_pos, colored);
        Paint_DrawPixel(x + x_pos, y + y_pos, colored);
        Paint_DrawPixel(x + x_pos, y - y_pos, colored);
        Paint_DrawPixel(x - x_pos, y - y_pos, colored);
        Paint_DrawHorizontalLine(x + x_pos, y + y_pos, 2 * (-x_pos) + 1, colored);
        Paint_DrawHorizontalLine(x + x_pos, y - y_pos, 2 * (-x_pos) + 1, colored);
        e2 = err;
        if (e2 <= y_pos) {
            err += ++y_pos * 2 + 1;
            if (-x_pos == y_pos && e2 <= x_pos) {
                e2 = 0;
            }
        }
        if (e2 > x_pos) {
            err += ++x_pos * 2 + 1;
        }
    } while (x_pos <= 0);
}

/* END OF FILE */

