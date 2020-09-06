/*
   This code generates an effect that should pass the 'fancy graphics' qualification
   as set in the comment in the spi_master code.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <math.h>
#include "esp_log.h"
#include "pretty_effect.h"
#include "sdkconfig.h"

static const char *TAG = "pretty";

static const uint8_t BayerPattern[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42,},
    {48, 16, 56, 24, 50, 18, 58, 26,},
    {12, 44, 4, 36, 14, 46, 6, 38,},
    {60, 28, 52, 20, 62, 30, 54, 22,},
    {3, 35, 11, 43, 1, 33, 9, 41,},
    {51, 19, 59, 27, 49, 17, 57, 25,},
    {15, 47, 7, 39, 13, 45, 5, 37,},
    {63, 31, 55, 23, 61, 29, 53, 21}
};

esp_err_t pretty_effect_init(void)
{
    // return decode_image(&pixels);
    return ESP_OK;
}

esp_err_t pretty_print_img(uint8_t **img, int width, int height)
{
    const char temp2char[17] = "@MNHQ&#UJ*x7^i;.";
    for (size_t j = 0; j < height; j++)
    {
        for (size_t i = 0; i < width; i++)
        {
            printf("%c", temp2char[img[j][i] >> 4]);
        }
        printf("\n");
    }
    return ESP_OK;
}

esp_err_t pretty_process(Jpegdata_t *jpg_data, pretty_method_t method)
{
    ESP_LOGD(TAG, "pretty process method: %d", method);
    
    if (PRETTY_METHOD_BINARIZATION == method)
    {
        for (size_t j = 0; j < jpg_data->outH; j++)
        {
            for (size_t i = 0; i < jpg_data->outW; i++)
            {
                pixel_data_t *pixel = &jpg_data->outData[j][i];
                *pixel = *pixel > 128 ? 255 : 0;
            }
        }
    }
    else if (PRETTY_METHOD_DITHERING == method)
    {
        for (size_t j = 0; j < jpg_data->outH; j++)
        {
            for (size_t i = 0; i < jpg_data->outW; i++)
            {
                pixel_data_t *pixel = &jpg_data->outData[j][i];
                if ((*pixel >> 2) > BayerPattern[j & 7][i & 7])
                {
                    *pixel = 1;
                }
                else
                {
                    *pixel = 0;
                }
            }
        }
    }
    else if (PRETTY_METHOD_DITHERING_Floyd == method)
    {
        for (size_t j = 0; j < jpg_data->outH; j++)
        {
            for (size_t i = 0; i < jpg_data->outW; i++)
            {
                pixel_data_t *pixel = &jpg_data->outData[j][i];
                pixel_data_t newPixel = *pixel > 128 ? 255 : 0;
                int err = *pixel - newPixel;
                *pixel = newPixel;
                if (i == jpg_data->outW - 1 || j == jpg_data->outH - 1)
                    continue;
                if (i == 0)
                {
                    jpg_data->outData[j][i + 1] += err * 7 / 16;
                    jpg_data->outData[j + 1][i] += err * 7 / 16;
                    jpg_data->outData[j + 1][i + 1] += err * 2 / 16;
                }
                else
                {
                    jpg_data->outData[j][i + 1] += err * 7 / 16;
                    jpg_data->outData[j + 1][i] += err * 5 / 16;
                    jpg_data->outData[j + 1][i - 1] += err * 3 / 16;
                    jpg_data->outData[j + 1][i + 1] += err * 1 / 16;
                }
            }
        }
    }
    return ESP_OK;
}