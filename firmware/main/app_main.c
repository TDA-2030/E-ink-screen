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
#include "board.h"
#include "app_show_image.h"
#include "show_text.h"
#include "mp3_player.h"

static const char *TAG = "Eink";

/**
  * Due to RAM not enough in Arduino UNO, a frame buffer is not allowed.
  * In this case, a smaller image buffer is allocated and you have to 
  * update a partial display several times.
  * 1 byte = 8 pixels, therefore you have to set 8*N pixels at a time.
  */
static unsigned char image[EPD_WIDTH * EPD_HEIGHT / 8];



void app_main(void)
{
    ESP_LOGI(TAG, "------------ system start ------------");
    board_init();

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    Epd_Init(EPD_2IN13_PART);
    Paint_init(image, EPD_WIDTH, EPD_HEIGHT);
    Paint_SetRotate(ROTATE_90);
    Paint_Clear(UNCOLORED);
    Paint_DrawStringAt(0, 16, "start", &Font16, COLORED);
    
    bool is_configured;
    char str_buf[128]={0};
    captive_portal_start("ESP_WEB_CONFIG", NULL, &is_configured);
    wifi_config_t wifi_config;
    esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);

    if (is_configured) {
        sprintf(str_buf, "SSID:%s, PASSWORD:%s", wifi_config.sta.ssid, wifi_config.sta.password);
        Paint_DrawStringAt(0, 16, str_buf, &Font12, COLORED);
    } else {
        sprintf(str_buf, "WiFi not configured, started a configuration webpage");
        Paint_DrawStringAt(0, 32, str_buf, &Font12, COLORED);
    }
    Epd_SetFrameMemory_Area(Paint_GetImage(), 0, 0, EPD_WIDTH, EPD_HEIGHT);
    Epd_DisplayFrame();
    captive_portal_wait(portMAX_DELAY);

    Paint_Clear(UNCOLORED);
    Epd_Set_LUT(EPD_2IN13_FULL);
    Epd_SetFrameMemory_Area(Paint_GetImage(), 0, 0, EPD_WIDTH, EPD_HEIGHT);
    Epd_DisplayFrame();
    

    sntp_start();

    ESP_ERROR_CHECK(fm_init()); /* Initialize file storage */
    start_file_server();
    
    board_pa_ctrl(true);
    mp3_player_start("/spiffs/01.mp3");
    // mp3_player_wait(portMAX_DELAY);
    // board_pa_ctrl(false);

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    

    app_show_text_init();

    app_show_text_str(0, 40, 200, 200, "现在的时间是:", 16, 1);
    // image_show_start(&paint, &epd);

    while (1)
    {

        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);

        app_show_text_str(0, 55, 200, 100, strftime_buf, 16, 1);
        Epd_SetFrameMemory_Area(Paint_GetImage(), 0, 0, EPD_WIDTH, EPD_HEIGHT);
        Epd_DisplayFrame();

        vTaskDelay(pdMS_TO_TICKS(6000));
    }
}

