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

#if 1
#include "epd2in13.h"
#include "epdpaint.h"
#include "imagedata.h"

static const char *TAG = "Eink";

/**
  * Due to RAM not enough in Arduino UNO, a frame buffer is not allowed.
  * In this case, a smaller image buffer is allocated and you have to 
  * update a partial display several times.
  * 1 byte = 8 pixels, therefore you have to set 8*N pixels at a time.
  */
unsigned char image[EPD_WIDTH * EPD_HEIGHT / 8];
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
    epd.SetFrameMemory(IMAGE_DATA);
    epd.DisplayFrame();
    epd.SetFrameMemory(IMAGE_DATA);
    epd.DisplayFrame();

    paint.SetWidth(EPD_WIDTH); // width should be the multiple of 8
    paint.SetHeight(EPD_HEIGHT);
}

#endif

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
    wifi_config_t wifi_config;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "WiFi config success! SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    }
    else
    {
        ESP_LOGE(TAG, "WiFi config fail! please check the SSID and password, SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
    }

    sntp_start();

    ESP_ERROR_CHECK(fm_init()); /* Initialize file storage */
    start_file_server();

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    setup();

    const char list[8][32] = {
        "/spiffs/1.jpg",
        "/spiffs/2.jpg",
        "/spiffs/3.jpg",
        "/spiffs/4.jpg",
        "/spiffs/5.jpg",
        "/spiffs/6.jpg",
        "/spiffs/7.jpg",
        "/spiffs/8.jpg",
    };

    int list_index = 0;

    while (1)
    {

        static Jpegdata_t jpg_data;
        ret = decode_image(list[list_index], &jpg_data);
        ESP_LOGI(TAG, "%s: h=%d, w=%d", list[list_index], jpg_data.outH, jpg_data.outW);
        if (++list_index >= 8)
            list_index = 0;
        if (ESP_OK != ret)
        {
            continue;
        }
        if (jpg_data.outH > 250)
            jpg_data.outH = 250;
        if (jpg_data.outW > 128)
            jpg_data.outW = 128;

        static char temp2char[17] = "@MNHQ&#UJ*x7^i;.";
        static uint8_t BayerPattern[8][8] = {
            0, 32, 8, 40, 2, 34, 10, 42,
            48, 16, 56, 24, 50, 18, 58, 26,
            12, 44, 4, 36, 14, 46, 6, 38,
            60, 28, 52, 20, 62, 30, 54, 22,
            3, 35, 11, 43, 1, 33, 9, 41,
            51, 19, 59, 27, 49, 17, 57, 25,
            15, 47, 7, 39, 13, 45, 5, 37,
            63, 31, 55, 23, 61, 29, 53, 21};
#define METHOD 1
        for (size_t j = 0; j < jpg_data.outH; j++)
        {
            for (size_t i = 0; i < jpg_data.outW; i++)
            {
                // printf("%c", temp2char[jpg_data.outData[j][i] >> 4]);
                pixel_data_t *pixel = &jpg_data.outData[j][i];
                #if METHOD==1
                if ((*pixel >> 2) > BayerPattern[j & 7][i & 7])
                {
                    *pixel = 1;
                }
                else
                {
                    *pixel = 0;
                }
                #elif METHOD==2
                *pixel = *pixel > 128 ? 255 : 0;
                #elif METHOD==3
                pixel_data_t newPixel = *pixel > 128 ? 255 : 0;
                int err = *pixel - newPixel;
                *pixel = newPixel;
                if(i == jpg_data.outW-1 || j == jpg_data.outH-1) continue;
                if(i==0){
                    jpg_data.outData[j][i+1] += err*7/16;
                    jpg_data.outData[j+1][i] += err*7/16;
                    jpg_data.outData[j+1][i+1] += err*2/16;
                }else{
                    jpg_data.outData[j][i+1] += err*7/16;
                    jpg_data.outData[j+1][i] += err*5/16;
                    jpg_data.outData[j+1][i-1] += err*3/16;
                    jpg_data.outData[j+1][i+1] += err*1/16;
                }
                
                #endif
            }
            // printf("\n");
        }
        ESP_LOGI(TAG, "stary to display");
        paint.DrawImage(0, 0, (int)jpg_data.outW, (int)jpg_data.outH, (uint8_t **)jpg_data.outData);
        epd.SetFrameMemory(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight());
        epd.DisplayFrame();
        epd.SetFrameMemory(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight());
        epd.DisplayFrame();
        ESP_LOGI(TAG, "display ok");

        decode_image_free(&jpg_data);

        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


// for (let y = 0; y < height; y++) {
//     for (let x = 0; x < width; x++) {
//         const oldPixel = data[px(x, y)]
//         const newPixel = oldPixel > 125 ? 255 : 0
//         data[px(x, y)] = data[px(x, y) + 1] = data[px(x, y) + 2] = newPixel
//         const quantError = oldPixel - newPixel

//         data[px(x + 1, y    )] =
//         data[px(x + 1, y    ) + 1] =
//         data[px(x + 1, y    ) + 2] =
//         data[px(x + 1, y    )] + quantError * 7 / 16

//         data[px(x - 1, y + 1)] =
//         data[px(x - 1, y + 1) + 1] =
//         data[px(x - 1, y + 1) + 2] =
//         data[px(x - 1, y + 1)] + quantError * 3 / 16

//         data[px(x    , y + 1)] =
//         data[px(x    , y + 1) + 1] =
//         data[px(x    , y + 1) + 2] =
//         data[px(x    , y + 1)] + quantError * 5 / 16

//         data[px(x + 1, y + 1)] =
//         data[px(x + 1, y + 1) + 1] =
//         data[px(x + 1, y + 1) + 2] =
//         data[px(x + 1, y + 1)] + quantError * 1 / 16
//     }
// }