/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "captive_portal.h"
#include "pretty_effect.h"
#include "app_sntp.h"
#include "file_manage.h"
#include "file_server.h"
#include "decode_image.h"

#include "epd2in13.h"
#include "epdpaint.h"
#include "app_image_show.h"

static const char *TAG = "Eink";

/**
  * Due to RAM not enough in Arduino UNO, a frame buffer is not allowed.
  * In this case, a smaller image buffer is allocated and you have to 
  * update a partial display several times.
  * 1 byte = 8 pixels, therefore you have to set 8*N pixels at a time.
  */
static unsigned char image[EPD_WIDTH * EPD_HEIGHT / 8];
Paint paint(image, EPD_WIDTH, EPD_HEIGHT);
Epd epd;

void setup()
{
    // put your setup code here, to run once:
    if (epd.Init(lut_full_update) != 0)
    {
        ESP_LOGE(TAG, "e-Paper init failed");
        return;
    }

    epd.ClearFrameMemory(0xFF); // bit set = white, bit reset = black

    paint.SetRotate(ROTATE_0);
    paint.SetWidth(128); // width should be the multiple of 8
    paint.SetHeight(24);

    /* For simplicity, the arguments are explicit numerical coordinates */
    paint.Clear(COLORED);
    paint.DrawStringAt(30, 4, "Hello world!", &Font12, UNCOLORED);
    epd.SetFrameMemory(paint.GetImage(), 0, 10, paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);
    paint.DrawStringAt(30, 4, "e-Paper Demo", &Font12, COLORED);
    epd.SetFrameMemory(paint.GetImage(), 0, 30, paint.GetWidth(), paint.GetHeight());

    paint.SetWidth(64);
    paint.SetHeight(64);

    paint.Clear(UNCOLORED);
    paint.DrawRectangle(0, 0, 40, 50, COLORED);
    paint.DrawLine(0, 0, 40, 50, COLORED);
    paint.DrawLine(40, 0, 0, 50, COLORED);
    epd.SetFrameMemory(paint.GetImage(), 16, 60, paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);
    paint.DrawCircle(32, 32, 30, COLORED);
    epd.SetFrameMemory(paint.GetImage(), 72, 60, paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);
    paint.DrawFilledRectangle(0, 0, 40, 50, COLORED);
    epd.SetFrameMemory(paint.GetImage(), 16, 130, paint.GetWidth(), paint.GetHeight());

    paint.Clear(UNCOLORED);
    paint.DrawFilledCircle(32, 32, 30, COLORED);
    epd.SetFrameMemory(paint.GetImage(), 72, 130, paint.GetWidth(), paint.GetHeight());
    epd.DisplayFrame();

    vTaskDelay(pdMS_TO_TICKS(1000));

    paint.SetWidth(EPD_WIDTH); // width should be the multiple of 8
    paint.SetHeight(EPD_HEIGHT);
    paint.Clear(UNCOLORED);
    epd.SetFrameMemory(paint.GetImage(), 0, 0, EPD_WIDTH, EPD_HEIGHT);
    epd.DisplayFrame();

    epd.SetLut(lut_partial_update);
    // if (epd.Init(lut_partial_update) != 0) {
    //     ESP_LOGE(TAG, "e-Paper init failed");
    //     return;
    // }

    /**
   *  there are 2 memory areas embedded in the e-paper display
   *  and once the display is refreshed, the memory area will be auto-toggled,
   *  i.e. the next action of SetFrameMemory will set the other memory area
   *  therefore you have to set the frame memory and refresh the display twice.
   */
    // epd.SetFrameMemory(IMAGE_DATA);
    // epd.DisplayFrame();
    // epd.SetFrameMemory(IMAGE_DATA);
    // epd.DisplayFrame();

    paint.SetWidth(EPD_WIDTH); // width should be the multiple of 8
    paint.SetHeight(EPD_HEIGHT);
    
}


extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "------------ system start ------------");
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = captive_portal_start("ESP_WEB_CONFIG", NULL);
    captive_portal_wait(portMAX_DELAY);

    sntp_start();

    ESP_ERROR_CHECK(fm_init()); /* Initialize file storage */
    start_file_server();

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    setup();

    image_show_start(&paint, &epd);

    while (1)
    {

        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

