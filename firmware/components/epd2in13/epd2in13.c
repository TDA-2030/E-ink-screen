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
#include "sdkconfig.h"

static const char *TAG = "edp";

#define LCD_EPAPER_CHECK(a, str)  if(!(a)) {                               \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
                                                                   \
    }
    
#if 1//CONFIG_EPD_SCREEN_2IN13

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
static uint8_t is_init_iface = 0;
static uint8_t g_mode = EPD_2IN13_FULL;

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

static void Epd_Set_LUT(uint8_t Mode)
{
    g_mode = Mode;
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
}

int Epd_Init(uint8_t Mode) {
    ESP_LOGI(TAG, "e-Paper init ");
    /* this calls the peripheral hardware interface, see epdif */
    if (0 == is_init_iface) {
        if (IfInit() != 0) {
            ESP_LOGE(TAG, "ifinit failed");
            return -1;
        }
        is_init_iface = 1;
    }

    /* EPD hardware init start */
    DigitalWrite(RST_PIN, HIGH);
    DelayMs(100);
    DigitalWrite(RST_PIN, LOW);                //module reset    
    DelayMs(100);
    DigitalWrite(RST_PIN, HIGH);
    DelayMs(100);

    // WaitUntilIdle();
    // SendCommand(0x12); // soft reset
    // WaitUntilIdle();

    SendCommand(0x03); //Gate Driving voltage Control
    SendData(0x19);

    SendCommand(0x04); //Source Driving voltage Control
    SendData(0x3c);
    SendData(0xA8);
    SendData(0x2e);

    SendCommand(DRIVER_OUTPUT_CONTROL);
    SendData((EPD_2IN13_HEIGHT - 1) & 0xFF);
    SendData(((EPD_2IN13_HEIGHT - 1) >> 8) & 0xFF);
    SendData(0x00);                     // GD = 0; SM = 0; TB = 0;

    // SendCommand(BOOSTER_SOFT_START_CONTROL);
    // SendData(0xD7);
    // SendData(0xD6);
    // SendData(0x9D);
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
    Epd_Set_LUT(Mode);

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
 *  @brief: clear the frame memory with the specified color.
 *          this won't update the display.
 */
void Epd_ClearFrameMemory(unsigned char color) {
    
}

void Epd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{

    LCD_EPAPER_CHECK((0 == (x0 % 8)), "x0 should be a multiple of 8");
    LCD_EPAPER_CHECK((0 == ((x1 + 1) % 8)), "x1+1 should be a multiple of 8");

    x0 >>= 3;
    x1 = ((x1 + 1) >> 3) - 1;
    ESP_LOGI(TAG, "window:[%d, %d, %d, %d]", x0, y0, x1, y1);

    SendCommand(SET_RAM_X_ADDRESS_START_END_POSITION);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData(x0 & 0xFF);ESP_LOGI(TAG, "0x44, 0x%x, 0x%x", (x0 ) & 0xFF, (x1 ) & 0xFF);
    SendData(x1 & 0xFF);
    SendCommand(SET_RAM_Y_ADDRESS_START_END_POSITION);
    SendData(y0 & 0xFF);
    SendData((y0 >> 8) & 0xFF);
    SendData(y1 & 0xFF);
    SendData((y1 >> 8) & 0xFF);
    ESP_LOGI(TAG, "0x45, 0x%x, 0x%x, 0x%x, 0x%x", y0 & 0xFF, (y0 >> 8) & 0xFF, y1 & 0xFF, (y1 >> 8) & 0xFF);

    SendCommand(SET_RAM_X_ADDRESS_COUNTER);
    /* x point must be the multiple of 8 or the last 3 bits will be ignored */
    SendData(x0 & 0xFF);
    SendCommand(SET_RAM_Y_ADDRESS_COUNTER);
    SendData(y0 & 0xFF);
    SendData((y0 >> 8) & 0xFF);
    ESP_LOGI(TAG, "0x4E, 0x%x", (x0 ) & 0xFF);
    ESP_LOGI(TAG, "0x4F, 0x%x, 0x%x", y0 & 0xFF, (y0 >> 8) & 0xFF);
    
    SendCommand(WRITE_RAM);
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

void Epd_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    uint8_t *p = (uint8_t *)bitmap;
    
    // Epd_set_window(x, y, x + w - 1, y + h - 1);
    // uint32_t len = w / 8 * h;
    // ESP_LOGI(TAG, "len = %d\n", len);
    // for (uint32_t i = 0; i < len; i++) {
    //     SendData(p[i]);
    // }
    int x_end = x + w - 1;
    int y_end = y + h - 1;
    Epd_SetMemoryArea(x, y, x_end, y_end);
    /* set the frame memory line by line */
    for (int j = y; j <= y_end; j++) {
        Epd_SetMemoryPointer(x, j);
        SendCommand(WRITE_RAM);
        for (int i = x / 8; i <= x_end / 8; i++) {
            SendData(p[(i - x / 8) + (j - y) * (w / 8)]);
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
void Epd_DisplayFrame(void)
{
    if (EPD_2IN13_FULL == g_mode) {
        SendCommand(0x22); //Display Update Control
        SendData(0xC7);
        SendCommand(0x20);  //Activate Display Update Sequence

    } else {
        SendCommand(0x22); //Display Update Control
        SendData(0x0C);
        SendCommand(0x20); //Activate Display Update Sequence
    }
    WaitUntilIdle();
}



/**
 *  @brief: After this command is transmitted, the chip would enter the 
 *          deep-sleep mode to save power. 
 *          The deep sleep mode would return to standby by hardware reset. 
 *          You can use Epd_Init() to awaken
 */
void Epd_DeepSleep(void)
{
    SendCommand(0x10); //enter deep sleep
    SendData(0x01);
    DelayMs(100);
}


#endif

/* END OF FILE */


