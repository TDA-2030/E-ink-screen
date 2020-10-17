
#include <stdlib.h>
#include "epd2in13.h"

static const char *TAG = "edp";

#define LCD_EPAPER_CHECK(a, str)  if(!(a)) {                               \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
                                                                   \
    }


#if CONFIG_EPD_SCREEN_2IN13_GOOD_DISPLAY

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

static const unsigned char LUT_DATA[] = {
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,       //LUT0: BB:     VS 0 ~7
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,       //LUT1: BW:     VS 0 ~7
    0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00,       //LUT2: WB:     VS 0 ~7
    0x10, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00,       //LUT3: WW:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       //LUT4: VCOM:   VS 0 ~7

    0x03, 0x03, 0x00, 0x00, 0x02,                   // TP0 A~D RP0
    0x09, 0x09, 0x00, 0x00, 0x02,                   // TP1 A~D RP1
    0x03, 0x03, 0x00, 0x00, 0x02,                   // TP2 A~D RP2
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP3 A~D RP3
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP4 A~D RP4
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP5 A~D RP5
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP6 A~D RP6

    0x15, 0x41, 0xA8, 0x32, 0x30, 0x0A,
};
static const unsigned char LUT_DATA_part[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       //LUT0: BB:     VS 0 ~7
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       //LUT1: BW:     VS 0 ~7
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       //LUT2: WB:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       //LUT3: WW:     VS 0 ~7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       //LUT4: VCOM:   VS 0 ~7

    0x0A, 0x00, 0x00, 0x00, 0x00,                   // TP0 A~D RP0
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP1 A~D RP1
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP2 A~D RP2
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP3 A~D RP3
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP4 A~D RP4
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP5 A~D RP5
    0x00, 0x00, 0x00, 0x00, 0x00,                   // TP6 A~D RP6

    0x15, 0x41, 0xA8, 0x32, 0x30, 0x0A,
};

static uint16_t g_width = EPD_WIDTH;
static uint16_t g_height = EPD_HEIGHT;
static uint8_t is_init_iface = 0;
static uint8_t g_mode = EPD_2IN13_FULL;

static void SendCommand(unsigned char command)
{
    DigitalWrite(DC_PIN, LOW);
    SpiTransfer(command);
}

static void SendData(unsigned char data)
{
    DigitalWrite(DC_PIN, HIGH);
    SpiTransfer(data);
}

static void WaitUntilIdle(void)
{
    uint16_t timeout = 50;
    while (DigitalRead(BUSY_PIN) == HIGH) {     //LOW: idle, HIGH: busy
        DelayMs(100);
        if (--timeout == 0) {
            ESP_LOGW(TAG, "Wait epaper idle timeout");
            break;
        }
    }
}

static void Epd_Set_LUT(uint8_t Mode)
{
    g_mode = Mode;
    unsigned char count;
    SendCommand(0x32);
    if (Mode == EPD_2IN13_FULL) {
        for (count = 0; count < 70; count++) {
            SendData(LUT_DATA[count]);
        }
    } else {
        for (count = 0; count < 70; count++) {
            SendData(LUT_DATA_part[count]);
        }
    }
}

int Epd_Init(uint8_t Mode)
{
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
    DelayMs(50);
    DigitalWrite(RST_PIN, LOW);                //module reset
    DelayMs(50);
    DigitalWrite(RST_PIN, HIGH);
    DelayMs(50);

    if (EPD_2IN13_FULL == Mode) {

        WaitUntilIdle();
        SendCommand(0x12); // soft reset
        WaitUntilIdle();

        SendCommand(0x74); //set analog block control
        SendData(0x54);
        SendCommand(0x7E); //set digital block control
        SendData(0x3B);

        SendCommand(0x01); //Driver output control
        SendData(0xF9);
        SendData(0x00);
        SendData(0x00);

        SendCommand(0x11); //data entry mode
        SendData(0x03);

        SendCommand(0x44); //set Ram-X address start/end position
        SendData(0x00);
        SendData(0x0F);    //0x0C-->(15+1)*8=128

        SendCommand(0x45); //set Ram-Y address start/end position
        SendData(0x00);
        SendData(0x00);
        SendData(0xF9);   //0xF9-->(249+1)=250
        SendData(0x00);
        

        SendCommand(0x3C); //BorderWavefrom
        SendData(0x03);

        SendCommand(0x2C);     //VCOM Voltage
        SendData(0x55);    //

        SendCommand(0x03); //
        SendData(LUT_DATA[70]);

        SendCommand(0x04); //
        SendData(LUT_DATA[71]);
        SendData(LUT_DATA[72]);
        SendData(LUT_DATA[73]);


        SendCommand(0x3A);     //Dummy Line
        SendData(LUT_DATA[74]);
        SendCommand(0x3B);     //Gate time
        SendData(LUT_DATA[75]);

        Epd_Set_LUT(EPD_2IN13_FULL); //LUT

        SendCommand(0x4E);   // set RAM x address count to 0;
        SendData(0x00);
        SendCommand(0x4F);   // set RAM y address count to 0X127;
        SendData(0x00);
        SendData(0x00);
        WaitUntilIdle();
    } else {
        SendCommand(0x2C);     //VCOM Voltage
        SendData(0x26);

        WaitUntilIdle();
        Epd_Set_LUT(EPD_2IN13_PART);
        SendCommand(0x37);
        SendData(0x00);
        SendData(0x00);
        SendData(0x00);
        SendData(0x00);
        SendData(0x40);
        SendData(0x00);
        SendData(0x00);

        SendCommand(0x22); //Display Update Control
        SendData(0xC0);
        SendCommand(0x20);  //Activate Display Update Sequence
        WaitUntilIdle();

        SendCommand(0x3C); //BorderWavefrom
        SendData(0x01);
    }

    return 0;
}

int Epd_Deinit(void)
{
    ESP_LOGI(TAG, "e-Paper deinit ");
    if (IfDeinit() != 0) {
        ESP_LOGE(TAG, "ifdeinit failed");
        return -1;
    }

    return 0;
}

void Epd_ClearFrameMemory(uint8_t color)
{
    unsigned int i;
    SendCommand(0x24);   //write RAM for black(0)/white (1)
    for (i = 0; i < (250 * 128 / 8); i++) {
        SendData(color);
    }
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


void Epd_draw_bitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    uint8_t *p = (uint8_t *)bitmap;
    
    Epd_set_window(x, y, x + w - 1, y + h - 1);
    // SendCommand(WRITE_RAM);
    uint32_t len = w / 8 * h;
    ESP_LOGI(TAG, "len = %d\n", len);
    for (uint32_t i = 0; i < len; i++) {
        SendData(p[i]);
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


