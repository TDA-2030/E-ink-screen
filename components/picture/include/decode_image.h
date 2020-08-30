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


typedef uint8_t pixel_data_t;

//Data that is passed from the decoder function to the infunc/outfunc functions.
typedef struct
{
    FILE *fd;
    pixel_data_t **outData; //Array of IMAGE_H pointers to arrays of IMAGE_W 16-bit pixel values
    uint16_t outW;           //Width of the resulting file
    uint16_t outH;           //Height of the resulting file
} Jpegdata_t;


/**
 * @brief Decode the jpeg ``image.jpg`` embedded into the program file into pixel data.
 *
 * @param pixels A pointer to a pointer for an array of rows, which themselves are an array of pixels.
 *        Effectively, you can get the pixel data by doing ``decode_image(&myPixels); pixelval=myPixels[ypos][xpos];``
 * @return - ESP_ERR_NOT_SUPPORTED if image is malformed or a progressive jpeg file
 *         - ESP_ERR_NO_MEM if out of memory
 *         - ESP_OK on succesful decode
 */
esp_err_t decode_image(const char *filepath, Jpegdata_t *pixels);


esp_err_t decode_image_free(Jpegdata_t *pixels);

#ifdef __cplusplus 
}
#endif