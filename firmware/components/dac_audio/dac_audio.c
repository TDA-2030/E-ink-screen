#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "sdkconfig.h"

static const char *TAG = "DAC audio";

#define ADC_AUDIO_CHECK(a, str, ret_val)                          \
    if (!(a))                                                     \
    {                                                             \
        ESP_LOGE(TAG, "%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val);                                         \
    }

#ifndef CONFIG_IDF_TARGET_ESP32
    #warning "I2S DAC only support ESP32"
#else

//i2s number
#define EXAMPLE_I2S_NUM           (0)
//i2s data bits
#define EXAMPLE_I2S_SAMPLE_BITS   (16)

//I2S data format
#define EXAMPLE_I2S_FORMAT        (I2S_CHANNEL_FMT_ONLY_LEFT)
//I2S channel number
#define EXAMPLE_I2S_CHANNEL_NUM   ((EXAMPLE_I2S_FORMAT < I2S_CHANNEL_FMT_ONLY_RIGHT) ? (2) : (1))

#define VOLUME_0DB          (16)


static uint8_t *i2s_write_buff;
static int32_t g_volume;
static DRAM_ATTR i2s_dev_t* I2S[I2S_NUM_MAX] = {&I2S0, &I2S1};


esp_err_t dac_audio_set_param(int rate, int bits, int ch)
{
    i2s_set_clk(EXAMPLE_I2S_NUM, rate, bits, ch);
    i2s_stop(EXAMPLE_I2S_NUM);
    I2S[EXAMPLE_I2S_NUM]->clkm_conf.clkm_div_num = 40000000/rate/16;
    I2S[EXAMPLE_I2S_NUM]->clkm_conf.clkm_div_b = 0;
    I2S[EXAMPLE_I2S_NUM]->clkm_conf.clkm_div_a = 1;
    I2S[EXAMPLE_I2S_NUM]->sample_rate_conf.tx_bck_div_num = 2;
    i2s_start(EXAMPLE_I2S_NUM);
    return ESP_OK;
}

esp_err_t dac_audio_set_volume(int8_t volume)
{
    if (volume < 0) {
        ADC_AUDIO_CHECK(-volume <= VOLUME_0DB, "Volume out of range", ESP_ERR_INVALID_ARG);
    } else {
        ADC_AUDIO_CHECK(volume <= VOLUME_0DB, "Volume out of range", ESP_ERR_INVALID_ARG);
    }

    g_volume = volume + VOLUME_0DB;
    return ESP_OK;
}

/**
 * @brief Scale data to 16bit/32bit for I2S DMA output.
 *        DAC can only output 8bit data value.
 *        I2S DMA will still send 16 bit or 32bit data, the highest 8bit contains DAC data.
 */
static int example_i2s_dac_data_scale(uint8_t *d_buff, uint8_t *s_buff, uint32_t len)
{
    uint32_t j = 0;
#if (EXAMPLE_I2S_SAMPLE_BITS == 16)
    len >>= 1;
    uint16_t *b16 = (uint16_t *)s_buff;
    for (int i = 0; i < len; i++) {
        int16_t t = b16[i] ;
        t = t * g_volume / VOLUME_0DB;
        t = t + 0x7fff;
        d_buff[j++] = 0;
        d_buff[j++] = t >> 8;
    }
    return (j);
#else
    for (int i = 0; i < len; i++) {
        d_buff[j++] = 0;
        d_buff[j++] = 0;
        d_buff[j++] = 0;
        d_buff[j++] = s_buff[i];
    }
    return (len * 4);
#endif
}




esp_err_t dac_audio_write(uint8_t *write_buff, size_t play_len, size_t *bytes_written, TickType_t ticks_to_wait)
{
    int i2s_wr_len = example_i2s_dac_data_scale(i2s_write_buff, write_buff, play_len);
    i2s_write(EXAMPLE_I2S_NUM, i2s_write_buff, i2s_wr_len, bytes_written, ticks_to_wait);
    return ESP_OK;
}

esp_err_t dac_audio_init()
{
    int i2s_num = EXAMPLE_I2S_NUM;
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER |  I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate =  16000,
        .bits_per_sample = EXAMPLE_I2S_SAMPLE_BITS,
        .communication_format = I2S_COMM_FORMAT_I2S_MSB,
        .channel_format = EXAMPLE_I2S_FORMAT,
        .intr_alloc_flags = 0,
        .dma_buf_count = 16,
        .dma_buf_len = 1024,
        .use_apll = 1,
    };
    //install and start i2s driver
    i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
    //init DAC pad
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
    //init ADC pad
    //  i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL);
    ESP_LOGI(TAG, "DAC audio ok");

    dac_audio_set_volume(0);

    i2s_write_buff = (uint8_t *) calloc(16 * 1024, sizeof(char));

    return ESP_OK;
}

#endif
