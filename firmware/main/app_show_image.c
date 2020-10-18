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

#include "pretty_effect.h"
#include "file_manage.h"
#include "file_server.h"
#include "decode_image.h"

#include "epd2in13.h"
#include "epdpaint.h"

static const char *TAG = "image show";

static bool g_is_stop = 0;


static void image_show_task(void *args)
{
    esp_err_t ret;
    char **file_list;
    uint16_t f_l=0;
    fm_file_table_create(&file_list, &f_l);
    for (size_t i = 0; i < f_l; i++)
    {
        ESP_LOGI(TAG, "have file [%d:%p, %s]", i, file_list[i], file_list[i]);
    }

    int list_index = 0;
    fs_info_t *info;
    fm_get_info(&info);

    while (1)
    {
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

        Jpegdata_t jpg_data;
        char file_path[64];
        sprintf(file_path, "%s/%s", info->base_path, file_list[list_index]);
        ret = decode_image(file_path, &jpg_data);
        ESP_LOGI(TAG, "%s: h=%d, w=%d", file_list[list_index], jpg_data.outH, jpg_data.outW);
        if (++list_index >= f_l){
            list_index = 0;
        }
        if (ESP_OK != ret)
        {
            continue;
        }
        uint16_t w = jpg_data.outW > 250 ? 250 : jpg_data.outW;
        uint16_t h = jpg_data.outH > 128 ? 128 : jpg_data.outH;

        // // pretty_print_img((uint8_t **)jpg_data.outData, (int)w, (int)h);
        pretty_process(&jpg_data, PRETTY_METHOD_DITHERING);
        
        ESP_LOGI(TAG, "stary to display");
        Paint_SetRotate(ROTATE_90);
        Paint_DrawImage(0, 0, (int)w, (int)h, (uint8_t **)jpg_data.outData);

        Epd_Init(EPD_2IN13_FULL);
        Epd_draw_bitmap(0, 0, EPD_WIDTH, EPD_HEIGHT, Paint_GetImage());
        Epd_DisplayFrame();
        Epd_DeepSleep();
        
        ESP_LOGI(TAG, "display ok");

        decode_image_free(&jpg_data);
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

        vTaskDelay(pdMS_TO_TICKS(30*1000));

        if(g_is_stop){
            break;
        }
    }
    fm_file_table_free(&file_list, f_l);
    vTaskDelete(NULL);
    
}

esp_err_t image_show_start(void)
{

    xTaskCreate(image_show_task, "image_task", 4096, NULL, 6, NULL);
    return ESP_OK;
}

esp_err_t image_show_stop(void)
{
    g_is_stop = 1;
    return ESP_OK;
}

