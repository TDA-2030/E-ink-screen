/**
 *  @filename   :   epdif.h
 *  @brief      :   Header file of epdif.cpp providing EPD interface functions
 *                  Users have to implement all the functions in epdif.cpp
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
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

#ifndef EPDIF_H
#define EPDIF_H

#ifdef __cplusplus 
extern "C" {
#endif

#include "esp_log.h"

// Pin definition
#define RST_PIN         CONFIG_EDPIO_RST_PIN
#define DC_PIN          CONFIG_EDPIO_DC_PIN
#define CS_PIN          CONFIG_EDPIO_CS_PIN
#define BUSY_PIN        CONFIG_EDPIO_BUSY_PIN
#define MOSI_PIN        CONFIG_EDPIO_MOSI_PIN
#define SCLK_PIN        CONFIG_EDPIO_SCLK_PIN

#define LOW  0
#define HIGH 1


int  IfInit(void);
int  IfDeinit(void);
void DigitalWrite(int pin, int value); 
int  DigitalRead(int pin);
void DelayMs(unsigned int delaytime);
void SpiTransfer(unsigned char data);

#ifdef __cplusplus 
}
#endif

#endif
