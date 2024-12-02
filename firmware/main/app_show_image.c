#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "pretty_effect.h"
#include "file_manage.h"
#include "decode_image.h"
#include "app_epaper.h"

static const char *TAG = "image show";


void image_show_task(void *args)
{

    epaper_panel_t *panel = (epaper_panel_t *)args;

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
        ESP_LOGI(TAG, "%s: h=%d, w=%d", file_path, jpg_data.outH, jpg_data.outW);
        if (++list_index >= f_l){
            epaper_refresh(panel, EPD_REFRESH_MODE_FULL);
            list_index = 0;
        }
        if (ESP_OK != ret)
        {
            ESP_LOGE(TAG, "decode_image failed");
            continue;
        }
        uint16_t w = jpg_data.outW > panel->paint->width ? panel->paint->width : jpg_data.outW;
        uint16_t h = jpg_data.outH > panel->paint->height ? panel->paint->height : jpg_data.outH;

        pretty_process(&jpg_data, PRETTY_METHOD_DITHERING);
        // pretty_print_img((uint8_t **)jpg_data.outData, (int)w, (int)h);
        
        ESP_LOGI(TAG, "stary to display");
        Paint_Clear(panel->paint, 0);
        Paint_DrawImage(panel->paint, 0, 0, (int)w, (int)h, (uint8_t **)jpg_data.outData);
        ESP_LOGI(TAG, "stary to refresh");

        epaper_refresh(panel, EPD_REFRESH_MODE_FULL_FAST2);
        
        ESP_LOGI(TAG, "display ok");

        decode_image_free(&jpg_data);
        heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);

        vTaskDelay(pdMS_TO_TICKS(10*1000));


    }
    fm_file_table_free(&file_list, f_l);
    vTaskDelete(NULL);
    
}
