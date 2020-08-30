/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#ifdef __cplusplus 
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "decode_image.h"

typedef enum
{
    PRETTY_METHOD_BINARIZATION,
    PRETTY_METHOD_DITHERING_Floyd,
    PRETTY_METHOD_DITHERING,
}pretty_method_t;

/**
 * @brief Calculate the effect for a bunch of lines.
 *
 * @param dest Destination for the pixels. Assumed to be LINECT * 320 16-bit pixel values.
 * @param line Starting line of the chunk of lines.
 * @param frame Current frame, used for animation
 * @param linect Amount of lines to calculate
 */
esp_err_t pretty_process(Jpegdata_t *jpg_data, pretty_method_t method);


/**
 * @brief Initialize the effect
 *
 * @return ESP_OK on success, an error from the jpeg decoder otherwise.
 */
esp_err_t pretty_effect_init(void);

#ifdef __cplusplus 
}
#endif