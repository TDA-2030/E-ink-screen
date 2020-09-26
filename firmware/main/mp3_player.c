// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_heap_caps.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "mp3dec.h"
#include "pwm_audio.h"
#include "adc_audio.h"
#include "driver/gpio.h"


static const char *TAG = "mp3 player";

#define MP3_PLAYER_CHECK(a, str, ret)  if(!(a)) {                               \
        ESP_LOGE(TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str);   \
        return (ret);                                                           \
    }

#define PLAY_USE_DAC  /**< Comment out this line and use PWM to play audio */


#define AUDIO_EVT_PLAY        BIT0
#define AUDIO_EVT_REQ_STOP    BIT1
#define AUDIO_EVT_STOPPED     BIT2
// #define AUDIO_EVT_PAUSE       BIT3

static EventGroupHandle_t g_mp3_event = NULL;


static esp_err_t mp3_player_init(void);
static esp_err_t mp3_player_deinit(void);

static void audio_task(void *arg)
{
    esp_err_t ret;
    ret = mp3_player_init();

    if (ESP_OK != ret) {
        goto exit;
    }

    char *path = (char *)arg;

    ESP_LOGI(TAG, "start to decode and play %s", path);
    HMP3Decoder hMP3Decoder;

    hMP3Decoder = MP3InitDecoder();

    if (NULL == hMP3Decoder) {
        ESP_LOGE(TAG, "MP3 decoder memory is not enough..");
        goto exit;
    }

    int16_t *out_buffer = malloc(1153 * 4);

    if (NULL == out_buffer) {
        MP3FreeDecoder(hMP3Decoder);
        ESP_LOGE(TAG, "Output buffer malloc failed");
        goto exit;
    }

    FILE *mp3File = fopen(path, "rb");

    if (NULL == mp3File) {
        MP3FreeDecoder(hMP3Decoder);
        free(out_buffer);
        ESP_LOGE(TAG, "MP3 file open failed");
        goto exit;
    }

    uint8_t *readBuf = malloc(MAINBUF_SIZE);

    if (NULL == readBuf) {
        MP3FreeDecoder(hMP3Decoder);
        free(out_buffer);
        fclose(mp3File);
        ESP_LOGE(TAG, "Buffer malloc failed");
        goto exit;
    }

    char tag[10];
    int read_bytes = fread(tag, 1, 10, mp3File);

    if (read_bytes == 10) {
        int tag_len = 0;

        if (memcmp(tag, "ID3", 3) == 0) {
            tag_len = ((tag[6] & 0x7F) << 21) | ((tag[7] & 0x7F) << 14) | ((tag[8] & 0x7F) << 7) | (tag[9] & 0x7F);
            // ESP_LOGI(TAG,"tag_len: %d %x %x %x %x", tag_len,tag[6],tag[7],tag[8],tag[9]);
            fseek(mp3File, tag_len - 10, SEEK_SET);
        } else {
            fseek(mp3File, 0, SEEK_SET);
        }
    } else {
        ESP_LOGE(TAG, "Read mp3 file error");
        goto error;
    }

    int bytesLeft = 0;
    int samplerate = 0;
    uint8_t *readPtr = readBuf;
    MP3FrameInfo mp3FrameInfo;

    while (1) {

        if (AUDIO_EVT_REQ_STOP & xEventGroupGetBits(g_mp3_event)) {
            xEventGroupClearBits(g_mp3_event, AUDIO_EVT_REQ_STOP);
            ESP_LOGI(TAG, "palyer was stopped manually");
            break;
        }

        if (bytesLeft < MAINBUF_SIZE) {
            memmove(readBuf, readPtr, bytesLeft);
            int br = fread(readBuf + bytesLeft, 1, MAINBUF_SIZE - bytesLeft, mp3File);

            if ((br == 0) && (bytesLeft == 0)) {
                ESP_LOGI(TAG, "paly compeleted");
                break;
            }

            bytesLeft = bytesLeft + br;
            readPtr = readBuf;
        }

        int offset = MP3FindSyncWord(readPtr, bytesLeft);

        if (offset < 0) {
            ESP_LOGE(TAG, "MP3FindSyncWord not find");
            bytesLeft = 0;
            continue;
        } else {
            readPtr += offset;                   /*!< data start point */
            bytesLeft -= offset;                 /*!< in buffer */
            int errs = MP3Decode(hMP3Decoder, &readPtr, &bytesLeft, out_buffer, 0);

            if (errs != 0) {
                ESP_LOGE(TAG, "MP3Decode failed ,code is %d ", errs);
                break;
            }

            MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

            if (samplerate != mp3FrameInfo.samprate) {
                samplerate = mp3FrameInfo.samprate;
                ESP_LOGI(TAG, "mp3file info: [bitrate=%d,layer=%d,nChans=%d,samprate=%d,bitsPerSample=%d,outputSamps=%d]",
                         mp3FrameInfo.bitrate, mp3FrameInfo.layer, mp3FrameInfo.nChans, mp3FrameInfo.samprate, mp3FrameInfo.bitsPerSample, mp3FrameInfo.outputSamps);
#ifdef PLAY_USE_DAC
                adc_audio_set_param(mp3FrameInfo.samprate, mp3FrameInfo.bitsPerSample, mp3FrameInfo.nChans);
#else
                pwm_audio_set_param(mp3FrameInfo.samprate, mp3FrameInfo.bitsPerSample, mp3FrameInfo.nChans);
                pwm_audio_start();
#endif
                ESP_LOGI(TAG, "audio start to play ...");
            }

            size_t bytes_write = 0;
            size_t write_len = mp3FrameInfo.outputSamps * (mp3FrameInfo.bitsPerSample / 8);
#ifdef PLAY_USE_DAC
            adc_audio_write((uint8_t *)out_buffer, write_len, &bytes_write, 1000 / portTICK_RATE_MS);
#else
            pwm_audio_write((uint8_t *)out_buffer, write_len, &bytes_write, 1000 / portTICK_RATE_MS);
#endif
            ESP_LOGD(TAG, "i2s_wr_len=%d, bytes_write=%d", write_len, bytes_write);
        }
    }

error:
#ifdef PLAY_USE_DAC
    // i2s_zero_dma_buffer(0);
#else
    // pwm_audio_stop();
#endif
    MP3FreeDecoder(hMP3Decoder);
    free(readBuf);
    free(out_buffer);
    fclose(mp3File);

exit:
    xEventGroupSetBits(g_mp3_event, AUDIO_EVT_STOPPED);
    mp3_player_deinit();

    /**< Play completed and delete this task */
    vTaskDelete(NULL);
}

static esp_err_t mp3_player_init(void)
{
    esp_err_t ret;
#ifdef PLAY_USE_DAC
    adc_audio_init();
#else
    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_10_BIT;
    pac.gpio_num_left      = 26;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = -1;  /**< Use left channel only */
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 8;
    ret = pwm_audio_init(&pac);
    MP3_PLAYER_CHECK(ESP_OK == ret, "pwm audio init failed", ESP_FAIL);

    pwm_audio_set_volume(-12);
#endif

    g_mp3_event = xEventGroupCreate();

    return ESP_OK;
}
#include "board.h"
static esp_err_t mp3_player_deinit(void)
{
#ifdef PLAY_USE_DAC
#else
    pwm_audio_deinit();
#endif
    vEventGroupDelete(g_mp3_event);board_pa_ctrl(false);
    g_mp3_event = NULL;
    return ESP_OK;
}

esp_err_t mp3_player_start(const char *path)
{
    int timeout = 0;
    xTaskCreate(audio_task, "audio_task", 4096, (void *)path, 5, NULL);

    while (1) {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        if (NULL != g_mp3_event) {
            break;
        } else if (++timeout > 10) {
            ESP_LOGW(TAG, "start mp3 player task timeout");
        }
    }

    return ESP_OK;
}

esp_err_t mp3_player_stop(void)
{
    xEventGroupSetBits(g_mp3_event, AUDIO_EVT_REQ_STOP);
    return ESP_OK;
}

esp_err_t mp3_player_wait(TickType_t xTicksToWait)
{
    if (NULL == g_mp3_event) {
        return ESP_ERR_INVALID_STATE;
    }
    EventBits_t bits = xEventGroupWaitBits(g_mp3_event, AUDIO_EVT_STOPPED, pdTRUE, pdTRUE, xTicksToWait);
    if (AUDIO_EVT_STOPPED & bits) {
        return ESP_OK;
    } else {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t mp3_player_set_volume(int8_t volume)
{
    return pwm_audio_set_volume(volume);
}
