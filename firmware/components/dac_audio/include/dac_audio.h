// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#ifndef _ADC_AUDIO_H_
#define _ADC_AUDIO_H_
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int dma_buf_count;          /*!< DMA buffer count, number of buffer*/
    int dma_buf_len;            /*!< DMA buffer length, length of each buffer*/
    i2s_channel_fmt_t       channel_format;         /*!< I2S channel format */
    i2s_comm_format_t       communication_format;   /*!< I2S communication format */

} dac_audio_config_t;


esp_err_t dac_audio_init();

esp_err_t dac_audio_set_volume(int8_t volume);

esp_err_t dac_audio_set_param(int rate, int bits, int ch);

esp_err_t dac_audio_write(uint8_t* write_buff, size_t play_len, size_t *bytes_written, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

#endif
