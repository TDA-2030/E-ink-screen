/**
 *  @filename   :   epd2in13.cpp
 *  @brief      :   Implements for e-paper library
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

#include <stdlib.h>
#include "epd2in13.h"

static const char *TAG = "edp";


// EPD2IN13 commands
#define DRIVER_OUTPUT_CONTROL                       0x01
#define BOOSTER_SOFT_START_CONTROL                  0x0C
#define GATE_SCAN_START_POSITION                    0x0F
#define DEEP_SLEEP_MODE                             0x10
#define DATA_ENTRY_MODE_SETTING                     0x11
#define SW_RESET                                    0x12
#define TEMPERATURE_SENSOR_CONTROL                  0x1A
#define MASTER_ACTIVATION                           0x20
#define DISPLAY_UPDATE_CONTROL_1                    0x21
#define DISPLAY_UPDATE_CONTROL_2                    0x22
#define WRITE_RAM                                   0x24
#define WRITE_VCOM_REGISTER                         0x2C
#define WRITE_LUT_REGISTER                          0x32
#define SET_DUMMY_LINE_PERIOD                       0x3A
#define SET_GATE_TIME                               0x3B
#define BORDER_WAVEFORM_CONTROL                     0x3C
#define SET_RAM_X_ADDRESS_START_END_POSITION        0x44
#define SET_RAM_Y_ADDRESS_START_END_POSITION        0x45
#define SET_RAM_X_ADDRESS_COUNTER                   0x4E
#define SET_RAM_Y_ADDRESS_COUNTER                   0x4F
#define TERMINATE_FRAME_READ_WRITE                  0xFF

static const unsigned char EPD_2IN13_lut_full_update[] = {
    0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char EPD_2IN13_lut_partial_update[] = {
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint16_t g_width = EPD_WIDTH;
static uint16_t g_height = EPD_HEIGHT;


static void SendCommand(unsigned char command) {
    DigitalWrite(DC_PIN, LOW);
    SpiTransfer(command);
}

static void SendData(unsigned char data) {
    DigitalWrite(DC_PIN, HIGH);
    SpiTransfer(data);
}

static void WaitUntilIdle(void) {
    uint16_t timeout=50;
    while(DigitalRead(BUSY_PIN) == HIGH) {      //LOW: idle, HIGH: busy
        DelayMs(100);
        if(--timeout == 0){
            ESP_LOGW(TAG, "Wait epaper idle timeout");
            break;
        }
    }
}

int Epd_Init(uint8_t Mode) {
    ESP_LOGI(TAG, "e-Paper init ");
    /* this calls the peripheral hardware interface, see epdif */
    if (IfInit() != 0) {
        ESP_LOGE(TAG, "ifinit failed");
        return -1;
    }
    /* EPD hardware init start */
    DigitalWrite(RST_PIN, HIGH);
    DelayMs(200);
    DigitalWrite(RST_PIN, LOW);                //module reset    
    DelayMs(200);
    DigitalWrite(RST_PIN, HIGH);
    DelayMs(200);

    SendCommand(DRIVER_OUTPUT_CONTROL);
    SendData((EPD_2IN13_HEIGHT - 1) & 0xFF);
    SendData(((EPD_2IN13_HEIGHT - 1) >> 8) & 0xFF);
    SendData(0x00);                     // GD = 0; SM = 0; TB = 0;
    SendCommand(BOOSTER_SOFT_START_CONTROL);
    SendData(0xD7);
    SendData(0xD6);
    SendData(0x9D);
    SendCommand(WRITE_VCOM_REGISTER);
    SendData(0xA8);                     // VCOM 7C
    SendCommand(SET_DUMMY_LINE_PERIOD);
    SendData(0x1A);                     // 4 dummy lines per gate
    SendCommand(SET_GATE_TIME);
    SendData(0x08);                     // 2us per line

    SendCommand(0X3C);	// BORDER_WAVEFORM_CONTROL
    SendData(0x63);
    SendCommand(DATA_ENTRY_MODE_SETTING);
    SendData(0x03);                     // X increment; Y increment

    // //set the look-up table register
    SendCommand(0x32);
    if(Mode == EPD_2IN13_FULL) {
        for (int i = 0; i < 30; i++) {
            SendData(EPD_2IN13_lut_full_update[i]);
        }
    } else if(Mode == EPD_2IN13_PART) {
        for (int i = 0; i < 30; i++) {
            SendData(EPD_2IN13_lut_partial_update[i]);
        }
    }

    return 0;
}

int Epd_Deinit(void) {
    ESP_LOGI(TAG, "e-Paper deinit ");
    if (IfDeinit() != 0) {
        ESP_LOGE(TAG, "ifdeinit failed");
        return -1;
    }
    
    return 0;
}

/**
 *  @brief: private function to specify the memory area for data R/W
 */
static void Epd_SetMemoryArea(int x_start, int y_start, int x_end, int y_end) {
    SendCommand(SET_RAM_X_ADDRESS_START_END_POSITION);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData((x_start >> 3) & 0xFF);
    SendData((x_end >> 3) & 0xFF);
    SendCommand(SET_RAM_Y_ADDRESS_START_END_POSITION);
    SendData(y_start & 0xFF);
    SendData((y_start >> 8) & 0xFF);
    SendData(y_end & 0xFF);
    SendData((y_end >> 8) & 0xFF);
}

/**
 *  @brief: private function to specify the start point for data R/W
 */
static void Epd_SetMemoryPointer(int x, int y) {
    SendCommand(SET_RAM_X_ADDRESS_COUNTER);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData((x >> 3) & 0xFF);
    SendCommand(SET_RAM_Y_ADDRESS_COUNTER);
    SendData(y & 0xFF);
    SendData((y >> 8) & 0xFF);
    WaitUntilIdle();
}

/**
 *  @brief: put an image buffer to the frame memory.
 *          this won't update the display.
 */
void Epd_SetFrameMemory_Area(
    const unsigned char* image_buffer,
    int x,
    int y,
    int image_width,
    int image_height
) {
    int x_end;
    int y_end;

    if (
        image_buffer == NULL ||
        x < 0 || image_width < 0 ||
        y < 0 || image_height < 0
    ) {
        return;
    }
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    x &= 0xF8;
    image_width &= 0xF8;
    if (x + image_width >= g_width) {
        x_end = g_width - 1;
    } else {
        x_end = x + image_width - 1;
    }
    if (y + image_height >= g_height) {
        y_end = g_height - 1;
    } else {
        y_end = y + image_height - 1;
    }
    Epd_SetMemoryArea(x, y, x_end, y_end);
    /* set the frame memory line by line */
    for (int j = y; j <= y_end; j++) {
        Epd_SetMemoryPointer(x, j);
        SendCommand(WRITE_RAM);
        for (int i = x / 8; i <= x_end / 8; i++) {
            SendData(image_buffer[(i - x / 8) + (j - y) * (image_width / 8)]);
        }
    }
}

/**
 *  @brief: put an image buffer to the frame memory.
 *          this won't update the display.
 *
 *          Question: When do you use this function instead of 
 *          void SetFrameMemory(
 *              const unsigned char* image_buffer,
 *              int x,
 *              int y,
 *              int image_width,
 *              int image_height
 *          );
 *          Answer: SetFrameMemory with parameters only reads image data
 *          from the RAM but not from the flash in AVR chips (for AVR chips,
 *          you have to use the function pgm_read_byte to read buffers 
 *          from the flash).
 */
void Epd_SetFrameMemory(const unsigned char* image_buffer) {
    Epd_SetMemoryArea(0, 0, g_width - 1, g_height - 1);
    /* set the frame memory line by line */
    for (int j = 0; j < g_height; j++) {
        Epd_SetMemoryPointer(0, j);
        SendCommand(WRITE_RAM);
        for (int i = 0; i < g_width / 8; i++) {
            SendData(*(&image_buffer[i + j * (g_width / 8)]));
        }
    }
}

/**
 *  @brief: clear the frame memory with the specified color.
 *          this won't update the display.
 */
void Epd_ClearFrameMemory(unsigned char color) {
    Epd_SetMemoryArea(0, 0, g_width - 1, g_height - 1);
    /* set the frame memory line by line */
    for (int j = 0; j < g_height; j++) {
        Epd_SetMemoryPointer(0, j);
        SendCommand(WRITE_RAM);
        for (int i = 0; i < g_width / 8; i++) {
            SendData(color);
        }
    }
}

/**
 *  @brief: update the display
 *          there are 2 memory areas embedded in the e-paper display
 *          but once this function is called,
 *          the the next action of SetFrameMemory or ClearFrame will 
 *          set the other memory area.
 */
void Epd_DisplayFrame(void) {
    SendCommand(DISPLAY_UPDATE_CONTROL_2);
    SendData(0xC4);
    SendCommand(MASTER_ACTIVATION);
    SendCommand(TERMINATE_FRAME_READ_WRITE);
    WaitUntilIdle();
}



/**
 *  @brief: After this command is transmitted, the chip would enter the 
 *          deep-sleep mode to save power. 
 *          The deep sleep mode would return to standby by hardware reset. 
 *          You can use Epd_Init() to awaken
 */
void Epd_Sleep() {
    SendCommand(DEEP_SLEEP_MODE);
    WaitUntilIdle();
}




/* END OF FILE */


