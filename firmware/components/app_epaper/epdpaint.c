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
#include <stdio.h>
#include <string.h>
#include "epdpaint.h"
#include "cc936.h"
#include "esp_log.h"

static const char *TAG = "epdpaint";

/**
 *
 * Reference from https://blog.csdn.net/bladeandmaster88/article/details/54837338
 */
static int UnicodeToUtf8(char *pInput, char *pOutput)
{
    int len = 0; //记录转换后的Utf8字符串的字节数
    while (*pInput) {
        //处理一个unicode字符
        char low = *pInput;//取出unicode字符的低8位
        pInput++;
        char high = *pInput;//取出unicode字符的高8位
        unsigned  wchar = (high << 8) + low; //高8位和低8位组成一个unicode字符,加法运算级别高

        if (wchar <= 0x7F ) { //英文字符
            pOutput[len] = (char)wchar;  //取wchar的低8位
            len++;
        } else if (wchar >= 0x80 && wchar <= 0x7FF) { //可以转换成双字节pOutput字符
            pOutput[len] = 0xc0 | ((wchar >> 6) & 0x1f); //取出unicode编码低6位后的5位，填充到110yyyyy 10zzzzzz 的yyyyy中
            len++;
            pOutput[len] = 0x80 | (wchar & 0x3f);  //取出unicode编码的低6位，填充到110yyyyy 10zzzzzz 的zzzzzz中
            len++;
        } else if (wchar >= 0x800 && wchar < 0xFFFF) { //可以转换成3个字节的pOutput字符
            pOutput[len] = 0xe0 | ((wchar >> 12) & 0x0f); //高四位填入1110xxxx 10yyyyyy 10zzzzzz中的xxxx
            len++;
            pOutput[len] = 0x80 | ((wchar >> 6) & 0x3f);  //中间6位填入1110xxxx 10yyyyyy 10zzzzzz中的yyyyyy
            len++;
            pOutput[len] = 0x80 | (wchar & 0x3f);  //低6位填入1110xxxx 10yyyyyy 10zzzzzz中的zzzzzz
            len++;
        }

        else { //对于其他字节数的unicode字符不进行处理
            return -1;
        }
        pInput ++;//处理下一个unicode字符
    }
    //utf8字符串后面，有个\0
    pOutput [len] = 0;
    return len;
}
/*************************************************************************************************
* 将UTF8编码转换成Unicode（UCS-2LE）编码  高地址存低位字节
* 参数：
*    char* pInput     输入字符串
*    char*pOutput   输出字符串
* 返回值：转换后的Unicode字符串的字节数，如果出错则返回-1
**************************************************************************************************/
//utf8转unicode
static char *Utf8ToUnicode(char *pInput, char *pOutput)
{
    int outputSize = 0; //记录转换后的Unicode字符串的字节数
    char *p = pOutput;

    if (*pInput > 0x00 && *pInput <= 0x7F) { //处理单字节UTF8字符（英文字母、数字）
        *pOutput = *pInput;
        pOutput++;
        *pOutput = 0; //小端法表示，在高地址填补0
    } else if (((*pInput) & 0xE0) == 0xC0) { //处理双字节UTF8字符
        char high = *pInput;
        pInput++;
        char low = *pInput;
        if ((low & 0xC0) != 0x80) { //检查是否为合法的UTF8字符表示
            ESP_LOGE(TAG, "utf-8 encounter illegal character");
            return ++pInput;
        }

        *pOutput = (high << 6) + (low & 0x3F);
        pOutput++;
        *pOutput = (high >> 2) & 0x07;
    } else if (((*pInput) & 0xF0) == 0xE0) { //处理三字节UTF8字符
        char high = *pInput;
        pInput++;
        char middle = *pInput;
        pInput++;
        char low = *pInput;
        if (((middle & 0xC0) != 0x80) || ((low & 0xC0) != 0x80)) {
            ESP_LOGE(TAG, "utf-8 encounter illegal character");
            return ++pInput;
        }
        *pOutput = (middle << 6) + (low & 0x3F);//取出middle的低两位与low的低6位，组合成unicode字符的低8位
        pOutput++;
        *pOutput = (high << 4) + ((middle >> 2) & 0x0F); //取出high的低四位与middle的中间四位，组合成unicode字符的高8位
    } else { //对于其他字节数的UTF8字符不进行处理
        ESP_LOGE(TAG, "Unsupport utf-8 character [%x]", *pInput);
        return ++pInput;
    }
    outputSize += 2;

    // 字节顺序翻转
    for (size_t i = 0; i < outputSize; i += 2) {
        char t = p[i];
        p[i] = p[i + 1];
        p[i + 1] = t;
    }

    return ++pInput;
}


epd_paint_t *Paint_create(int width, int height)
{
    epd_paint_t *p = (epd_paint_t *)malloc(sizeof(epd_paint_t));
    p->width_memory = width;
    p->height_memory = height;
    p->color = 0;
    p->width_byte = (width % 8 == 0) ? (width / 8) : (width / 8 + 1);
    p->height_byte = height;
    p->rotate = ROTATE_270;
    p->fbuffer_size = p->width_byte * p->height_byte;
    if (p->rotate == ROTATE_0 || p->rotate == ROTATE_180) {
        p->width = width;
        p->height = height;
    } else {
        p->width = height;
        p->height = width;
    }

    p->fbuffer = (uint8_t *) calloc(p->fbuffer_size, sizeof(uint8_t));
    if (p->fbuffer == NULL) {
        ESP_LOGE(TAG, "%s calloc failed", __func__);
        free(p);
        return NULL;
    }
    return p;
}

void Paint_delete(epd_paint_t *_paint)
{
    if (NULL == _paint) {
        return;
    }

    if (_paint->fbuffer != NULL) {
        free(_paint->fbuffer);
        _paint = NULL;
    }
    free(_paint);
}

/**
 *  @brief: clear the image
 */
void Paint_Clear(const epd_paint_t *paint, int colored)
{
    for (int x = 0; x < paint->width_memory; x++) {
        for (int y = 0; y < paint->height_memory; y++) {
            Paint_DrawAbsolutePixel(paint, x, y, colored);
        }
    }
}

/**
 *  @brief: this draws a pixel by absolute coordinates.
 *          this function won't be affected by the rotate parameter.
 */
void Paint_DrawAbsolutePixel(const epd_paint_t *paint, uint16_t x, uint16_t y, int colored)
{
    int _width = paint->width_memory;
    int _height = paint->height_memory;
    x = _width - 1 - x; // mirror x axis

    uint32_t index = (x + y * _width) / 8;
    if (index > paint->fbuffer_size) {
        // if ( x >= _width || y >= _height) {
        ESP_LOGE(TAG, "Cannot draw point at (%d,%d) index:%u", x, y, index);
        return;
    }
    colored = !colored;
    if (colored) {
        paint->fbuffer[index] |= 0x80 >> (x % 8);
    } else {
        paint->fbuffer[index] &= ~(0x80 >> (x % 8));
    }

}

/**
 *  @brief: this draws a pixel by the coordinates
 */
void Paint_DrawPixel(const epd_paint_t *paint, int x, int y, int colored)
{
    int point_temp;
    int _width = paint->width_memory;
    int _height = paint->height_memory;

    switch (paint->rotate) {
    case ROTATE_0:
        // printf("x y %d %d\n", x, y);
        Paint_DrawAbsolutePixel(paint, x, y, colored);
        break;
    case ROTATE_90:
        point_temp = x;
        x = _width - 1 - y;
        y = point_temp;
        Paint_DrawAbsolutePixel(paint, x, y, colored);
        break;
    case ROTATE_180:
        x = _width - 1 - x;
        y = _height - 1 - y;
        Paint_DrawAbsolutePixel(paint, x, y, colored);
        break;
    case ROTATE_270:
        point_temp = x;
        x = y;
        y = _height - 1 - point_temp;
        Paint_DrawAbsolutePixel(paint, x, y, colored);
        break;
    default:
        // Handle unexpected rotation value
        break;
    }
}

/**
 *  @brief: this draws a charactor on the frame buffer but not refresh
 */
void Paint_DrawCharAt(const epd_paint_t *paint, int x, int y, char ascii_char, const sFONT *font, int colored)
{
    int i, j;
    uint8_t const *ptr;
    uint16_t font_len;
    font->get_char_mask(&ascii_char, &ptr, &font_len);

    for (j = 0; j < font->Height; j++) {
        for (i = 0; i < font->Width; i++) {
            if (*(ptr) & (0x80 >> (i % 8))) {
                Paint_DrawPixel(paint, x + i, y + j, !colored);
            } else {
                Paint_DrawPixel(paint, x + i, y + j, colored);
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
void Paint_DrawStringAt(const epd_paint_t *paint, int x, int y, const char *text, const sFONT *font, int colored)
{
    const char *p_text = text;
    unsigned int counter = 0;
    int refcolumn = x;

    /* Send the string character by character on EPD */
    while (*p_text != 0) {
        /* Display one character on EPD */
        Paint_DrawCharAt(paint, refcolumn, y, *p_text, font, colored);
        /* Decrement the column position by 16 */
        refcolumn += font->Width;
        /* Point on the next character */
        p_text++;
        counter++;
    }
}

//显示一个指定大小的汉字
//x,y :汉字的坐标
//font:汉字GBK码
//size:字体大小
//mode:0,正常显示,1,叠加显示
int Paint_gbk_char(const epd_paint_t *paint, uint16_t x, uint16_t y, const char *gbk, const sFONT *font, uint8_t mode)
{
    int ret;
    uint8_t temp, t, t1;
    uint16_t x0 = x, font_len;
    uint8_t *dzk;

    ret = font->get_char_mask(gbk, &dzk, &font_len); //得到相应大小的点阵数据
    if (0 != ret) {
        return ret;
    }

    for (t = 0; t < font_len; t++) {
        temp = dzk[t]; //得到点阵数据
        for (t1 = 0; t1 < 8; t1++) {
            if (temp & 0x80) {
                Paint_DrawPixel(paint, x, y, 1);
            } else if (mode == 0) {
                Paint_DrawPixel(paint, x, y, 0);
            }
            temp <<= 1;
            x++;
            if ((x - x0) == font->Width) {
                x = x0;
                y++;
                break;
            }
        }
    }

    return 0;
}

//在指定位置开始显示一个字符串
//支持自动换行
//(x,y):起始坐标
//width,height:区域
//str  :字符串
//size :字体大小
//mode:0,非叠加方式;1,叠加方式
int Paint_text_str(const epd_paint_t *paint, uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *string, const sFONT *font, uint8_t mode)
{
    char *str = (char *)string;

    char gbk[2];
    char unicode[2];
    uint16_t size = font->Width;

    uint16_t x0 = x;
    uint16_t y0 = y;
    uint8_t bHz = 0; //字符或者中文

    while (*str != 0) { //数据未结束
        str = Utf8ToUnicode(str, unicode);
        font_unicode2gbk(unicode, gbk);

        if (gbk[0] > 0x80) { //中文
            bHz = 1;
            if (x > (x0 + width - size)) { //换行
                y += size;
                x = x0;
            }
            if (y > (y0 + height - size)) {
                break;    //越界返回
            }
            Paint_gbk_char(paint, x, y, gbk, font, mode);
            x += size; //下一个汉字偏移
        } else { //字符
            if (x > (x0 + width - size / 2)) { //换行
                y += size;
                x = x0;
            }
            if (y > (y0 + height - size)) {
                break;    //越界返回
            }
            if (gbk[1] == 13) { //换行符号
                y += size;
                x = x0;
            } else {

                Paint_DrawCharAt(paint, (int)x, (int)y, gbk[1], &Font16, 0);
                printf("%c", gbk[1]);
            }
            x += size / 2; //字符,为全字的一半
        }
    }
    return 0;
}


/**
*  @brief: this displays a image on the frame buffer but not refresh
*/
void Paint_DrawImage(const epd_paint_t *paint, int x, int y, int width, int height, uint8_t **img)
{

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            Paint_DrawPixel(paint, x + i, y + j, img[j][i]);
        }
    }
}

/**
*  @brief: this draws a line on the frame buffer
*/
void Paint_DrawLine(const epd_paint_t *paint, int x0, int y0, int x1, int y1, int colored)
{
    /* Bresenham algorithm */
    int dx = x1 - x0 >= 0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 - y0 <= 0 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while ((x0 != x1) && (y0 != y1)) {
        Paint_DrawPixel(paint, x0, y0, colored);
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
void Paint_DrawHorizontalLine(const epd_paint_t *paint, int x, int y, int line_width, int colored)
{
    int i;
    for (i = x; i < x + line_width; i++) {
        Paint_DrawPixel(paint, i, y, colored);
    }
}

/**
*  @brief: this draws a vertical line on the frame buffer
*/
void Paint_DrawVerticalLine(const epd_paint_t *paint, int x, int y, int line_height, int colored)
{
    int i;
    for (i = y; i < y + line_height; i++) {
        Paint_DrawPixel(paint, x, i, colored);
    }
}

/**
*  @brief: this draws a rectangle
*/
void Paint_DrawRectangle(const epd_paint_t *paint, int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    Paint_DrawHorizontalLine(paint, min_x, min_y, max_x - min_x + 1, colored);
    Paint_DrawHorizontalLine(paint, min_x, max_y, max_x - min_x + 1, colored);
    Paint_DrawVerticalLine(paint, min_x, min_y, max_y - min_y + 1, colored);
    Paint_DrawVerticalLine(paint, max_x, min_y, max_y - min_y + 1, colored);
}

/**
*  @brief: this draws a filled rectangle
*/
void Paint_DrawFilledRectangle(const epd_paint_t *paint, int x0, int y0, int x1, int y1, int colored)
{
    int min_x, min_y, max_x, max_y;
    int i;
    min_x = x1 > x0 ? x0 : x1;
    max_x = x1 > x0 ? x1 : x0;
    min_y = y1 > y0 ? y0 : y1;
    max_y = y1 > y0 ? y1 : y0;

    for (i = min_x; i <= max_x; i++) {
        Paint_DrawVerticalLine(paint, i, min_y, max_y - min_y + 1, colored);
    }
}

/**
*  @brief: this draws a circle
*/
void Paint_DrawCircle(const epd_paint_t *paint, int x, int y, int radius, int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        Paint_DrawPixel(paint, x - x_pos, y + y_pos, colored);
        Paint_DrawPixel(paint, x + x_pos, y + y_pos, colored);
        Paint_DrawPixel(paint, x + x_pos, y - y_pos, colored);
        Paint_DrawPixel(paint, x - x_pos, y - y_pos, colored);
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
void Paint_DrawFilledCircle(const epd_paint_t *paint, int x, int y, int radius, int colored)
{
    /* Bresenham algorithm */
    int x_pos = -radius;
    int y_pos = 0;
    int err = 2 - 2 * radius;
    int e2;

    do {
        Paint_DrawPixel(paint, x - x_pos, y + y_pos, colored);
        Paint_DrawPixel(paint, x + x_pos, y + y_pos, colored);
        Paint_DrawPixel(paint, x + x_pos, y - y_pos, colored);
        Paint_DrawPixel(paint, x - x_pos, y - y_pos, colored);
        Paint_DrawHorizontalLine(paint, x + x_pos, y + y_pos, 2 * (-x_pos) + 1, colored);
        Paint_DrawHorizontalLine(paint, x + x_pos, y - y_pos, 2 * (-x_pos) + 1, colored);
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

