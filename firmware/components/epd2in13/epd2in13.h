/**
 *  @filename   :   epd2in13.h
 *  @brief      :   Header file for e-paper display library epd2in13.cpp
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

#ifndef EPD2IN13_H
#define EPD2IN13_H

#ifdef __cplusplus 
extern "C" {
#endif

#include "epdif.h"

// Display resolution
#define EPD_2IN13_WIDTH       122
#define EPD_2IN13_HEIGHT      250

#define EPD_2IN13_FULL			0
#define EPD_2IN13_PART			1


// Display resolution
/* the resolution is 122x250 in fact */
/* however, the logical resolution is 128x250 */
#define EPD_WIDTH       128
#define EPD_HEIGHT      250

#define COLORED     0
#define UNCOLORED   1

int  Epd_Init(uint8_t Mode);
int  Epd_Deinit(void);


void Epd_DisplayFrame(void);
void Epd_DeepSleep(void);

void Epd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void Epd_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);


#ifdef __cplusplus 
}
#endif

#endif /* EPD2IN13_H */

/* END OF FILE */
