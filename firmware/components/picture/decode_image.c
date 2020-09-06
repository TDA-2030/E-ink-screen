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



//Size of the work space for the jpeg decoder.
#define WORKSZ 3100
#define USE_RGB565 0

//Input function for jpeg decoder. Just returns bytes from the inData field of the JpegDev structure.
static uint16_t infunc(JDEC *decoder, uint8_t *buf, uint16_t len)
{
    //Read bytes from input file
    Jpegdata_t *jd = (Jpegdata_t *)decoder->device;
    size_t chunksize = 0;
    if (buf != NULL)
    {
        chunksize = fread(buf, 1, len, jd->fd);
    }
    else{
        fseek(jd->fd, len, SEEK_CUR);
    }
    
    return len;
}

//Output function. Re-encodes the RGB888 data from the decoder as big-endian RGB565 and
//stores it in the outData array of the JpegDev structure.
static uint16_t outfunc(JDEC *decoder, void *bitmap, JRECT *rect)
{
    Jpegdata_t *jd = (Jpegdata_t *)decoder->device;
    uint8_t *in = (uint8_t *)bitmap;
    // ESP_LOGI(TAG, "output t=%d, b=%d, l=%d, r=%d", rect->top, rect->bottom, rect->left, rect->right);
    for (int y = rect->top; y <= rect->bottom; y++)
    {
        for (int x = rect->left; x <= rect->right; x++)
        {
#if USE_RGB565
            //We need to convert the 3 bytes in `in` to a rgb565 value.
            uint16_t v = 0;
            v |= ((in[0] >> 3) << 11);
            v |= ((in[1] >> 2) << 5);
            v |= ((in[2] >> 3) << 0);
            //The LCD wants the 16-bit value in big-endian, so swap bytes
            v = (v >> 8) | (v << 8);
            jd->outData[y][x] = v;
#else
            /**< Convert to Gray */
            uint8_t v = (in[0] + in[1] + in[2]) / 3;
            jd->outData[y][x] = v;
#endif
            in += 3;
        }
    }
    return 1;
}

//Decode the embedded image into pixel lines that can be used with the rest of the logic.
esp_err_t decode_image(const char *filepath, Jpegdata_t *data_out)
{
    int r;
    JDEC decoder;
    // Jpegdata_t jd_data;
    esp_err_t ret = ESP_OK;

    data_out->fd = fopen(filepath, "r");
    if (!data_out->fd)
    {
        ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        return ESP_FAIL;
    }

    //Allocate the work space for the jpeg decoder.
    char *work = calloc(WORKSZ, 1);
    if (work == NULL)
    {
        ESP_LOGE(TAG, "Cannot allocate workspace");
        ret = ESP_ERR_NO_MEM;
        fclose(data_out->fd);
        return ret;
    }

    //Prepare and decode the jpeg.
    r = jd_prepare(&decoder, infunc, work, WORKSZ, (void *)data_out);
    if (r != JDR_OK)
    {
        ESP_LOGE(TAG, "Image decoder: jd_prepare failed (%d)", r);
        ret = ESP_ERR_NOT_SUPPORTED;
        free(work);
        fclose(data_out->fd);
        return ret;
    }

    //Populate fields of the JpegDev struct.
    data_out->outW = decoder.width;
    data_out->outH = decoder.height;

    //Alocate pixel memory. Each line is an array of IMAGE_W 16-bit pixels; the `*pixels` array itself contains pointers to these lines.
    data_out->outData = calloc(data_out->outH, sizeof(pixel_data_t *));
    if (data_out->outData == NULL)
    {
        ESP_LOGE(TAG, "Error allocating memory for lines");
        ret = ESP_ERR_NO_MEM;
        goto err;
    }
    for (int i = 0; i < data_out->outH; i++)
    {
        data_out->outData[i] = malloc(data_out->outW * sizeof(pixel_data_t));
        if (data_out->outData[i] == NULL)
        {
            ESP_LOGE(TAG, "Error allocating memory for line %d", i);
            ret = ESP_ERR_NO_MEM;
            goto err;
        }
    }
    
    r = jd_decomp(&decoder, outfunc, 0);
    if (r != JDR_OK && r != JDR_FMT1)
    {
        ESP_LOGE(TAG, "Image decoder: jd_decode failed (%d)", r);
        ret = ESP_ERR_NOT_SUPPORTED;
        goto err;
    }
    /* Close file after sending complete */
    fclose(data_out->fd);
    //All done! Free the work area (as we don't need it anymore) and return victoriously.
    free(work);
    return ret;
err:
    fclose(data_out->fd);
    //Something went wrong! Exit cleanly, de-allocating everything we allocated.
    decode_image_free(data_out);
    free(work);
    return ret;
}

esp_err_t decode_image_free(Jpegdata_t *pixels_data)
{
    pixel_data_t **p = pixels_data->outData;
    if (p != NULL)
    {
        for (int i = 0; i < pixels_data->outH; i++)
        {
            free(p[i]);
        }
        free(p);
        pixels_data->outData = NULL;
    }
    return ESP_OK;
}