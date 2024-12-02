/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "app_epaper.h"
#include "esp_ths_font.h"
#include "epdpaint.h"

static const char *TAG = "app_epaper";

static void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(CONFIG_EPAPER_PIN_NUM_DC, dc); // TODO: replace CONFIG_EPAPER_PIN_NUM_DC with epaper_panel_t->config.dc_pin
}

/* Send a command to the epaper. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static esp_err_t epaper_write_command(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active)
{
    esp_err_t ret;
    spi_transaction_t t = {0};
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = 8;                   //Command is 8 bits
    t.tx_buffer = &cmd;             //The data is the cmd itself
    t.user = (void*)0;              //D/C needs to be set to 0
    if (keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;   //Keep CS active after data transfer
    }
    ret = spi_device_polling_transmit(spi, &t); //Transmit!
    return ret;
}

/* Send data to the epaper. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
static esp_err_t epaper_write_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret = ESP_OK;
    spi_transaction_t t;
    if (len == 0) {
        return ret;    //no need to send anything
    }
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length = len * 8;             //Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;             //Data
    t.user = (void*)1;              //D/C needs to be set to 1
    ret = spi_device_polling_transmit(spi, &t); //Transmit!
    return ret;
}

#include "GDEY029T94/Display_EPD_W21.c"

static uint32_t epaper_pow(uint16_t m, uint16_t n)
{
    uint32_t result = 1;
    while (n--) {
        result *= m;
    }
    return result;
}


void epaper_print_number(epd_handle_t epd_handle, uint16_t x, uint16_t y, uint32_t num, uint16_t len, const sFONT *font, uint16_t color)
{
    uint8_t t, temp, m = 0;

    for (t = 0; t < len; t++) {
        temp = (num / epaper_pow(10, len - t - 1)) % 10;
        if (temp == 0) {
            Paint_DrawCharAt(epd_handle->paint, x + (font->Width) * t, y, '0', font, color);
        } else {
            Paint_DrawCharAt(epd_handle->paint, x + (font->Width) * t, y, temp + '0', font, color);
        }
    }
}

void epaper_display_picture(epd_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, const uint8_t bmp[], uint16_t color)
{
    uint16_t j = 0;
    uint16_t i, n, temp, m;
    uint16_t x0, y0;
    x += 1, y += 1, x0 = x, y0 = y;
    y_size = y_size / 8 + ((y_size % 8) ? 1 : 0);
    for (n = 0; n < y_size; n++) {
        for (i = 0; i < x_size; i++) {
            temp = bmp[j];
            j++;
            for (m = 0; m < 8; m++) {
                if (temp & 0x01) {
                    Paint_DrawPixel(epaper_handle->paint, x, y, !color);
                } else {
                    Paint_DrawPixel(epaper_handle->paint, x, y, color);
                }
                temp >>= 1;
                y++;
            }
            x++;
            if ((x - x0) == x_size) {
                x = x0;
                y0 = y0 + 8;
            }
            y = y0;
        }
    }
}


esp_err_t epaper_refresh_partial_start(epd_handle_t epd_handle, uint8_t *data)
{
    esp_err_t ret = ESP_OK;
    epaper_panel_t *panel = (epaper_panel_t *) epd_handle;
    EPD_HW_Init(epd_handle); //E-paper initialization
    EPD_SetRAMValue_BaseMap(epd_handle, data); //Please do not delete the background color function, otherwise it will cause an unstable display during partial refresh.
    return ret;
}

esp_err_t epaper_refresh_partial_end(epd_handle_t epd_handle)
{
    esp_err_t ret = ESP_OK;
    EPD_DeepSleep(epd_handle);//Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.
    return ret;
}

esp_err_t epaper_refresh(epd_handle_t epd_handle, epaper_refresh_mode_t mode)
{
    esp_err_t ret = ESP_OK;
    epaper_panel_t *panel = (epaper_panel_t *) epd_handle;

    // mode = EPD_REFRESH_MODE_FULL;
    switch (mode)
    {
    case EPD_REFRESH_MODE_FULL:
        /************Full display(2s)*******************/
        EPD_HW_Init(epd_handle); //Full screen refresh initialization.
        EPD_WhiteScreen_ALL(epd_handle, panel->paint->fbuffer); //To Display one image using full screen refresh.
        EPD_DeepSleep(epd_handle); //Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.

        break;
    case EPD_REFRESH_MODE_FULL_FAST:
        /************Fast refresh mode(1.5s)*******************/
        EPD_HW_Init_Fast(epd_handle); //Fast refresh initialization.
        EPD_WhiteScreen_ALL_Fast(epd_handle, panel->paint->fbuffer); //To display one image using fast refresh.
        EPD_DeepSleep(epd_handle); //Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.

        break;
    case EPD_REFRESH_MODE_FULL_FAST2:
        /************Fast refresh mode(1s)*******************/
        /************Onder versions do not support this feature*******************/
        EPD_HW_Init_Fast2(epd_handle); //Fast refresh initialization.
        EPD_WhiteScreen_ALL_Fast2(epd_handle, panel->paint->fbuffer); //To display one image using fast refresh.
        EPD_DeepSleep(epd_handle); //Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.

        break;
    case EPD_REFRESH_MODE_PART_AS_FULL:
        // EPD_HW_Init(epd_handle); //E-paper initialization
        // EPD_SetRAMValue_BaseMap(epd_handle, gImage_p1); //Please do not delete the background color function, otherwise it will cause an unstable display during partial refresh.
        // EPD_Dis_PartAll(epd_handle, gImage_p1); //Image 1
        // EPD_Dis_PartAll(epd_handle, gImage_p2); //Image 2
        // EPD_Dis_PartAll(epd_handle, gImage_p3); //Image 3
        // EPD_Dis_PartAll(epd_handle, gImage_p4); //Image 4
        // EPD_Dis_PartAll(epd_handle, gImage_p5); //Image 5
        // EPD_DeepSleep(epd_handle);//Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.
        break;
    case EPD_REFRESH_MODE_PART:
        // EPD_HW_Init(epd_handle); //Electronic paper initialization.
        // EPD_SetRAMValue_BaseMap(epd_handle, gImage_basemap); //Please do not delete the background color function, otherwise it will cause unstable display during partial refresh.
        // for(int i=0; i<6; i++)
        //     EPD_Dis_Part_Time(epd_handle, 32,92+32*0,Num[i],         //x-A,y-A,DATA-A
        //                       32,92+32*1,Num[0],         //x-B,y-B,DATA-B
        //                       32,92+32*2,gImage_numdot, //x-C,y-C,DATA-C
        //                       32,92+32*3,Num[0],        //x-D,y-D,DATA-D
        //                       32,92+32*4,Num[1],32,64); //x-E,y-E,DATA-E,Resolution 32*64

        // EPD_DeepSleep(epd_handle);//Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.
        break;

    default:
        break;
    }

    return ret;
}

/*
epaper_config_t _config={0};
_config.spi_host_id = EPAPER_BUS_HOST;
_config.buscfg.miso_io_num = -1;
_config.buscfg.miso_io_num = -1;
_config.buscfg.mosi_io_num = CONFIG_EPAPER_PIN_NUM_MOSI;
_config.buscfg.sclk_io_num = CONFIG_EPAPER_PIN_NUM_SCLK;
_config.buscfg.quadwp_io_num = -1;
_config.buscfg.quadhd_io_num = -1;
_config.devcfg.clock_speed_hz = 26 * 1000 * 1000;          //Clock out at 10 MHz
_config.devcfg.spics_io_num = CONFIG_EPAPER_PIN_NUM_CS;    //CS pin
*/
epd_handle_t epaper_create(epaper_config_t *_config)
{
    esp_err_t ret;
    epaper_panel_t *panel = (epaper_panel_t *) calloc(1, sizeof(epaper_panel_t));
    if (panel == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for epaper_panel_t.");
        return NULL;
    }
    ESP_LOGI(TAG, "Create epaper panel.");
    // epaper_panel->busy_state = xSemaphoreCreateBinary();
    (*_config).buscfg.max_transfer_sz = EPD_ARRAY_SIZE + 8;
    (*_config).devcfg.mode = 0;                                   //SPI mode 0
    (*_config).devcfg.queue_size = 7;                             //We want to be able to queue 7 transactions at a time
    (*_config).devcfg.pre_cb = lcd_spi_pre_transfer_callback;     //Specify pre-transfer callback to handle D/C line
    panel->config = *_config; // Copy the configuraton
    /* Initialize the SPI bus */
    ret = spi_bus_initialize(_config->spi_host_id, &panel->config.buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    /* Attach the epaper to the SPI bus */
    ret = spi_bus_add_device(_config->spi_host_id, &panel->config.devcfg, &panel->spi_handle);
    ESP_ERROR_CHECK(ret);

    /* Initialize non-SPI GPIOs */
    gpio_config_t io_conf;
    io_conf.pin_bit_mask = ((1ULL << panel->config.dc_pin) | (1ULL << panel->config.rst_pin));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = true;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << panel->config.busy_pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    panel->paint = Paint_create(EPD_WIDTH, EPD_HEIGHT);
    if(panel->paint == NULL) {
        ESP_LOGE(TAG, "Paint create failed");
        free(panel);
        return NULL;
    }
    Paint_Clear(panel->paint, 0);
    // Paint_DrawLine(panel->paint, 0, 0, 40, 20, 1);
    epaper_refresh(panel, EPD_REFRESH_MODE_FULL);

    ESP_ERROR_CHECK(ret);
    return (epd_handle_t)panel;
}

esp_err_t epaper_delete(epd_handle_t *epd_handle)
{
    esp_err_t ret = ESP_OK;
    epaper_panel_t *panel = (epaper_panel_t *)(*epd_handle);
    if (*epd_handle == NULL) {
        return ESP_OK;
    }

    Paint_delete(panel->paint);
    ret = spi_bus_remove_device(panel->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove SPI device");
        return ret;
    }
    free(panel);
    *epd_handle = NULL;
    return ESP_OK;
}
