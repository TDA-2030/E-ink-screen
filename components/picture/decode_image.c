/* SPI Master example: jpeg decoder.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
The image used for the effect on the LCD in the SPI master example is stored in flash
as a jpeg file. This file contains the decode_image routine, which uses the tiny JPEG
decoder library to decode this JPEG into a format that can be sent to the display.

Keep in mind that the decoder library cannot handle progressive files (will give
``Image decoder: jd_prepare failed (8)`` as an error) so make sure to save in the correct
format if you want to use a different image file.
*/
#include "esp_log.h"
#include <string.h>
#include "esp_vfs.h"

#include "decode_image.h"
#include "tjpgd.h"

const char *TAG = "ImageDec";

//Data that is passed from the decoder function to the infunc/outfunc functions.
typedef struct
{
    FILE *fd;
    uint16_t **outData; //Array of IMAGE_H pointers to arrays of IMAGE_W 16-bit pixel values
    int outW;           //Width of the resulting file
    int outH;           //Height of the resulting file
} JpegDev;

//Input function for jpeg decoder. Just returns bytes from the inData field of the JpegDev structure.
static uint16_t infunc(JDEC *decoder, uint8_t *buf, uint16_t len)
{
    //Read bytes from input file
    JpegDev *jd = (JpegDev *)decoder->device;
    size_t chunksize = 0;
    if (buf != NULL)
    {
        chunksize = fread(buf, 1, len, jd->fd);
    }
    return (uint16_t)chunksize;
}

//Output function. Re-encodes the RGB888 data from the decoder as big-endian RGB565 and
//stores it in the outData array of the JpegDev structure.
static uint16_t outfunc(JDEC *decoder, void *bitmap, JRECT *rect)
{
    JpegDev *jd = (JpegDev *)decoder->device;
    uint8_t *in = (uint8_t *)bitmap;
    for (int y = rect->top; y <= rect->bottom; y++)
    {
        for (int x = rect->left; x <= rect->right; x++)
        {
            //We need to convert the 3 bytes in `in` to a rgb565 value.
            uint16_t v = 0;
            v |= ((in[0] >> 3) << 11);
            v |= ((in[1] >> 2) << 5);
            v |= ((in[2] >> 3) << 0);
            //The LCD wants the 16-bit value in big-endian, so swap bytes
            v = (v >> 8) | (v << 8);
            jd->outData[y][x] = v;
            in += 3;
        }
    }
    return 1;
}

//Size of the work space for the jpeg decoder.
#define WORKSZ 3100

//Define the height and width of the jpeg file. Make sure this matches the actual jpeg
//dimensions.
#define IMAGE_W 256
#define IMAGE_H 256

//Decode the embedded image into pixel lines that can be used with the rest of the logic.
esp_err_t decode_image(const char *filepath, uint16_t ***pixels)
{
    char *work = NULL;
    int r;
    JDEC decoder;
    JpegDev jd_data;
    *pixels = NULL;
    esp_err_t ret = ESP_OK;

    jd_data.fd = fopen(filepath, "r");
    if (!jd_data.fd)
    {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        return ESP_FAIL;
    }

    //Alocate pixel memory. Each line is an array of IMAGE_W 16-bit pixels; the `*pixels` array itself contains pointers to these lines.
    *pixels = calloc(IMAGE_H, sizeof(uint16_t *));
    if (*pixels == NULL)
    {
        ESP_LOGE(TAG, "Error allocating memory for lines");
        ret = ESP_ERR_NO_MEM;
        goto err;
    }
    for (int i = 0; i < IMAGE_H; i++)
    {
        (*pixels)[i] = malloc(IMAGE_W * sizeof(uint16_t));
        if ((*pixels)[i] == NULL)
        {
            ESP_LOGE(TAG, "Error allocating memory for line %d", i);
            ret = ESP_ERR_NO_MEM;
            goto err;
        }
    }

    //Allocate the work space for the jpeg decoder.
    work = calloc(WORKSZ, 1);
    if (work == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate workspace");
        ret = ESP_ERR_NO_MEM;
        goto err;
    }

    //Populate fields of the JpegDev struct.
    jd_data.outData = *pixels;
    jd_data.outW = IMAGE_W;
    jd_data.outH = IMAGE_H;

    //Prepare and decode the jpeg.
    r = jd_prepare(&decoder, infunc, work, WORKSZ, (void *)&jd_data);
    if (r != JDR_OK)
    {
        ESP_LOGE(TAG, "Image decoder: jd_prepare failed (%d)", r);
        ret = ESP_ERR_NOT_SUPPORTED;
        goto err;
    }
    r = jd_decomp(&decoder, outfunc, 0);
    if (r != JDR_OK && r != JDR_FMT1)
    {
        ESP_LOGE(TAG, "Image decoder: jd_decode failed (%d)", r);
        ret = ESP_ERR_NOT_SUPPORTED;
        goto err;
    }
    /* Close file after sending complete */
    fclose(jd_data.fd);
    //All done! Free the work area (as we don't need it anymore) and return victoriously.
    free(work);
    return ret;
err:
    fclose(jd_data.fd);
    //Something went wrong! Exit cleanly, de-allocating everything we allocated.
    if (*pixels != NULL)
    {
        for (int i = 0; i < IMAGE_H; i++)
        {
            free((*pixels)[i]);
        }
        free(*pixels);
    }
    free(work);
    return ret;
}

esp_err_t decode_image_free(uint16_t ***pixels)
{
    if (*pixels != NULL)
    {
        for (int i = 0; i < IMAGE_H; i++)
        {
            free((*pixels)[i]);
        }
        free(*pixels);
    }
    return ESP_OK;
}