/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "lp_core_main.h"
#include "ulp_lp_core.h"
#include "lp_core_i2c.h"
#include "driver/rtc_io.h"
#include "ulp_lp_core_lp_timer_shared.h"
#ifdef CONFIG_ULP_UART_PRINT
#include "lp_core_uart.h"
#endif
#ifdef CONFIG_REPORT_THS_DATA
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_user_mapping.h>
#include <esp_rmaker_common_events.h>
#include <app_wifi.h>
#ifdef CONFIG_UPDATE_WEATHER_DATA
#include "app_http.h"
#endif
#endif
#include "driver/gpio.h"
#include "app_epaper.h"
#include "esp_ths_image.h"
#include "file_manage.h"

#ifndef CONFIG_UPDATE_WEATHER_DATA
#define CONFIG_UPDATE_WEATHER_DATA 0
#endif

static const char *TAG = "LP_Environment_Sensor";

extern const uint8_t lp_core_main_bin_start[] asm("_binary_lp_core_main_bin_start");
extern const uint8_t lp_core_main_bin_end[]   asm("_binary_lp_core_main_bin_end");

/* E-paper power control pin, set to 1 to power off the screen, set to 0 to power on the screen. */
#define LDO_CHIP_EN                 GPIO_NUM_2
#define EPAPER_PWR_CTRL             GPIO_NUM_8
#define EPAPER_BUS_HOST             SPI2_HOST
#define EPAPER_REFRESH_BIT          BIT(0)
#define WIFI_CONNECT_DONE_BIT       BIT(1)
#define RMAKER_DATA_REPORT_BIT      BIT(2)
#define WEATHER_DATA_UPDATE_BIT     BIT(3)

static EventGroupHandle_t esp_ths_event_group = NULL;

#ifdef CONFIG_REPORT_THS_DATA
static esp_rmaker_device_t *temp_sensor_device;
#endif

static epd_handle_t s_epaper = NULL;

static char *city_name = "Shanghai";    // City name for weather information retrieval
static char *district_name = "Pudong";  // District name for weather information retrieval
#if CONFIG_UPDATE_WEATHER_DATA
static char *district_id = "310115";    // District ID for weather information retrieval (needs to be queried on Baidu Developer Platform)
#endif
static RTC_SLOW_ATTR char text[10] = {0};
static RTC_SLOW_ATTR char wind_class[10] = {0};
static RTC_SLOW_ATTR char wind_dir[15] = {0};
static RTC_SLOW_ATTR char uptime[15] = {0};
static RTC_SLOW_ATTR char week[10] = {0};
static RTC_SLOW_ATTR int high = 0;
static RTC_SLOW_ATTR int low = 0;
typedef struct {
    float temp;
    float hum;
    bool enable_http;
} esp_ths_data_type_t;

static void lp_core_init(void)
{
    esp_err_t ret = ESP_OK;
    ulp_lp_core_cfg_t cfg = {
        .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER,
        .lp_timer_sleep_duration_us = CONFIG_LP_CPU_WAKEUP_TIME_SECOND * 1000000,
    };

    ret = ulp_lp_core_load_binary(lp_core_main_bin_start, (lp_core_main_bin_end - lp_core_main_bin_start));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP Core load failed");
        abort();
    }

    ret = ulp_lp_core_run(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP Core run failed");
        abort();
    }
}

static void lp_i2c_init(void)
{
    esp_err_t ret = ESP_OK;
    /* Initialize LP I2C with default configuration */
    const lp_core_i2c_cfg_t i2c_cfg = {
        .i2c_pin_cfg.sda_io_num = GPIO_NUM_6,
        .i2c_pin_cfg.scl_io_num = GPIO_NUM_7,
        .i2c_pin_cfg.sda_pullup_en = true,
        .i2c_pin_cfg.scl_pullup_en = true,
        .i2c_timing_cfg.clk_speed_hz = 400000,
        .i2c_src_clk = LP_I2C_SCLK_LP_FAST,
    };
    ret = lp_core_i2c_master_init(LP_I2C_NUM_0, &i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LP I2C init failed");
        abort();
    }
}

#ifdef CONFIG_ULP_UART_PRINT
static void lp_uart_init(void)
{
    lp_core_uart_cfg_t cfg = {
        .uart_pin_cfg.tx_io_num = GPIO_NUM_5,
        .uart_pin_cfg.rx_io_num = GPIO_NUM_4,
        .uart_pin_cfg.rts_io_num = GPIO_NUM_2,
        .uart_pin_cfg.cts_io_num = GPIO_NUM_3,
        .uart_proto_cfg.baud_rate = 115200,
        .uart_proto_cfg.data_bits = UART_DATA_8_BITS,
        .uart_proto_cfg.parity = UART_PARITY_DISABLE,
        .uart_proto_cfg.stop_bits = UART_STOP_BITS_1,
        .uart_proto_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .uart_proto_cfg.rx_flow_ctrl_thresh = 0,
        .lp_uart_source_clk = LP_UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(lp_core_uart_init(&cfg));
}
#endif

#ifdef CONFIG_REPORT_THS_DATA
static void rmaker_data_report_done_event_handler(void* arg, esp_event_base_t event_base,
                                                  int32_t event_id, void* event_data)
{
    if (event_base == RMAKER_COMMON_EVENT) {
        if (event_id == RMAKER_MQTT_EVENT_PUBLISHED) {
            xEventGroupSetBits(esp_ths_event_group, RMAKER_DATA_REPORT_BIT);
        }
    }
}

static void rmaker_task(void *pvParameters)
{
    esp_ths_data_type_t *data = (esp_ths_data_type_t *)pvParameters;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    if (data->hum == 0) {
        app_first_time_wifi_init();
    } else {
        app_second_time_wifi_init();
    }

    esp_event_handler_register(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED,
                               &rmaker_data_report_done_event_handler, NULL);

    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "ESP-THS");

    temp_sensor_device = esp_rmaker_temp_sensor_device_create("ESP-THS", NULL, roundf((data->temp) * 10.0) / 10.0);
    esp_rmaker_param_t *temp_param = esp_rmaker_param_create("温度(Temperature)", NULL, esp_rmaker_float(roundf((data->temp) * 10.0) / 10.0), PROP_FLAG_READ);
    esp_rmaker_param_t *rh_param = esp_rmaker_param_create("湿度(Humidity)", NULL, esp_rmaker_float(roundf((data->hum) * 10.0) / 10.0), PROP_FLAG_READ);
    esp_rmaker_device_add_param(temp_sensor_device, temp_param);
    esp_rmaker_device_add_param(temp_sensor_device, rh_param);
    esp_rmaker_device_assign_primary_param(temp_sensor_device, temp_param);
    esp_rmaker_node_add_device(node, temp_sensor_device);

    esp_rmaker_start();

    if (data->hum == 0) {
        if (app_first_time_wifi_start(POP_TYPE_RANDOM) == ESP_OK) {
            xEventGroupSetBits(esp_ths_event_group, WIFI_CONNECT_DONE_BIT);
        }
    } else {
        if (app_second_time_wifi_start() == ESP_OK) {
            xEventGroupSetBits(esp_ths_event_group, WIFI_CONNECT_DONE_BIT);
        }
    }
    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
#endif

#if CONFIG_UPDATE_WEATHER_DATA
static void http_task(void *pvParameters)
{
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group, WIFI_CONNECT_DONE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & WIFI_CONNECT_DONE_BIT) {
            if (http_rest_with_url(district_id, text, wind_class, wind_dir, uptime, week, &high, &low) == ESP_OK) {
                xEventGroupSetBits(esp_ths_event_group, WEATHER_DATA_UPDATE_BIT);
                ESP_LOGI(TAG, "Weather data get done.");
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
#endif


esp_err_t epaper_display(epd_handle_t paper, epaper_display_data_t display_data)
{
    esp_err_t ret;
    int temp_decimal_part = (display_data.temp - (int)display_data.temp) * 10 + 0.5;
    int hum_decimal_part = (display_data.hum - (int)display_data.hum) * 10 + 0.5;

    epaper_print_number(paper, 10, 16, display_data.temp, 2, &Font24, BLACK);
    Paint_DrawStringAt(paper->paint, 89, 48, ".", &Font24, BLACK);
    epaper_print_number(paper, 89 + 8, 51, temp_decimal_part, 1, &Font24, BLACK);
    Paint_DrawStringAt(paper->paint, 90, 23, "o", &Font8, BLACK);
    Paint_DrawStringAt(paper->paint, 96, 20, "C", &Font24, BLACK);
    Paint_DrawStringAt(paper->paint, 25, 3, "Temperature", &Font12, BLACK);
    Paint_DrawLine(paper->paint, 1, 18, 119, 18, BLACK);

    epaper_print_number(paper, 18, 88, display_data.hum, 2, &Font24, BLACK);
    Paint_DrawStringAt(paper->paint, 54, 98, ".", &Font24, BLACK);
    epaper_print_number(paper, 60, 88, hum_decimal_part, 1, &Font24, BLACK);
    Paint_DrawStringAt(paper->paint, 80, 88, "%", &Font24, BLACK);
    Paint_DrawRectangle(paper->paint, 1, 1, 120, 75, BLACK);
    Paint_DrawRectangle(paper->paint, 1, 77, 120, 128, BLACK);
    Paint_DrawStringAt(paper->paint, 30, 78, "Humidity", &Font12, BLACK);
    Paint_DrawLine(paper->paint, 1, 92, 119, 92, BLACK);

    if (strstr(display_data.text, "晴") != NULL) {
        Paint_DrawStringAt(paper->paint, 145, 5, "Sunny", &Font16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, SUNNY, WHITE);
    } else if (strstr(display_data.text, "多云") != NULL) {
        Paint_DrawStringAt(paper->paint, 128, 5, "Partly Cloudy", &Font12, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, PARTLY_CLOUDY, WHITE);
    } else if (strstr(display_data.text, "阴") != NULL) {
        Paint_DrawStringAt(paper->paint, 140, 5, "Cloudy", &Font16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, CLOUDY, WHITE);
    } else if (strstr(display_data.text, "雪") != NULL) {
        Paint_DrawStringAt(paper->paint, 145, 5, "Snowy", &Font16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, SNOWY, WHITE);
    } else if (strstr(display_data.text, "雨") != NULL) {
        Paint_DrawStringAt(paper->paint, 150, 5, "Rain", &Font16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, RAIN, WHITE);
    } else {
        Paint_DrawStringAt(paper->paint, 140, 5, "No data", &Font16, BLACK);
        epaper_display_picture(paper, 141, 25, 48, 48, NO_WEATHER, WHITE);
    }

    char high_temp[10];
    char low_temp[10];
    sprintf(high_temp, "H:%d", display_data.max_temp);
    sprintf(low_temp, " L:%d", display_data.min_temp);
    char temp_str[12] = "";
    strcat(temp_str, high_temp);
    strcat(temp_str, low_temp);

    Paint_DrawStringAt(paper->paint, 132, 75, temp_str, &Font16, BLACK);
    Paint_DrawStringAt(paper->paint, 134, 92, display_data.city, &Font16, BLACK);
    Paint_DrawStringAt(paper->paint, 140, 108, display_data.district, &Font16, BLACK);

    Paint_DrawRectangle(paper->paint, 122, 1, 210, 128, BLACK);

    Paint_DrawStringAt(paper->paint, 240, 3, "Wind", &Font12, BLACK);
    Paint_DrawLine(paper->paint, 213, 18, 295, 18, BLACK);

    char wind_speed[15] = "Speed level:";
    wind_speed[strlen(wind_speed)] = display_data.wind_class[0];
    wind_speed[strlen(wind_speed) + 1] = '\0';
    Paint_DrawStringAt(paper->paint, 214, 19, wind_speed, &Font12, BLACK);

    if (strstr(display_data.wind_dir, "北风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:N", &Font8, BLACK);
    } else if (strstr(display_data.wind_dir, "东北风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:NE", &Font8, BLACK);
    } else if (strstr(display_data.wind_dir, "东风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:E", &Font8, BLACK);
    } else if (strstr(display_data.wind_dir, "东南风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:SE", &Font8, BLACK);
    } else if (strstr(display_data.wind_dir, "南风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:S", &Font8, BLACK);
    } else if (strstr(display_data.wind_dir, "西南风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:SW", &Font8, BLACK);
    } else if (strstr(display_data.wind_dir, "西风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:W", &Font8, BLACK);
    } else if (strstr(display_data.wind_dir, "西北风") != NULL) {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:NW", &Font8, BLACK);
    } else {
        Paint_DrawStringAt(paper->paint, 214, 32, "Direction:..", &Font8, BLACK);
    }

    Paint_DrawRectangle(paper->paint, 212, 1, 296, 46, BLACK);

    if (strstr(display_data.week, "一") != NULL) {
        Paint_DrawStringAt(paper->paint, 230, 48, "Monday", &Font16, BLACK);
    } else if (strstr(display_data.week, "二") != NULL) {
        Paint_DrawStringAt(paper->paint, 226, 48, "Tuesday", &Font16, BLACK);
    } else if (strstr(display_data.week, "三") != NULL) {
        Paint_DrawStringAt(paper->paint, 218, 48, "Wednesday", &Font16, BLACK);
    } else if (strstr(display_data.week, "四") != NULL) {
        Paint_DrawStringAt(paper->paint, 226, 48, "Thursday", &Font16, BLACK);
    } else if (strstr(display_data.week, "五") != NULL) {
        Paint_DrawStringAt(paper->paint, 230, 48, "Friday", &Font16, BLACK);
    } else if (strstr(display_data.week, "六") != NULL) {
        Paint_DrawStringAt(paper->paint, 222, 48, "Saturday", &Font16, BLACK);
    } else {
        Paint_DrawStringAt(paper->paint, 230, 48, "Sunday", &Font16, BLACK);
    }

    char uptime_str[12];
    uint8_t index = 0;
    for (int i = 0; i < 8; i++) {
        uptime_str[index] = display_data.uptime[i + 4];
        index++;
        if (index == 2) {
            uptime_str[index] = '.';
            index++;
        }
        if (index == 5) {
            uptime_str[index] = ' ';
            index++;
        }
        if (index == 8) {
            uptime_str[index] = ':';
            index++;
        }
        if (index == 11) {
            uptime_str[index] = '\0';
        }
    }
    Paint_DrawStringAt(paper->paint, 222, 66, uptime_str, &Font12, BLACK);
    epaper_display_picture(paper, 236, 85, 36, 36, ESP_LOGO, WHITE);
    Paint_DrawRectangle(paper->paint, 212, 48, 296, 128, BLACK);

    ret = epaper_refresh(paper, 0);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "epaper refresh failed");
    }
    return ret;
}


static void epaper_task(void *pvParameters)
{
    uint8_t refresh_times = 1;
    esp_ths_data_type_t *data = (esp_ths_data_type_t *)pvParameters;

    gpio_config_t epaper_pwr_io_conf = {};
    epaper_pwr_io_conf.pin_bit_mask = (1ULL << EPAPER_PWR_CTRL);
    epaper_pwr_io_conf.mode = GPIO_MODE_OUTPUT;
    epaper_pwr_io_conf.pull_up_en = true;
    gpio_config(&epaper_pwr_io_conf);
    gpio_set_level(EPAPER_PWR_CTRL, 0);

    epaper_config_t _config={0};
    _config.busy_pin = CONFIG_EPAPER_PIN_NUM_BUSY;
    _config.rst_pin = CONFIG_EPAPER_PIN_NUM_RST;
    _config.dc_pin = CONFIG_EPAPER_PIN_NUM_DC;
    _config.spi_host_id = EPAPER_BUS_HOST;
    _config.buscfg.miso_io_num = -1;
    _config.buscfg.miso_io_num = -1;
    _config.buscfg.mosi_io_num = CONFIG_EPAPER_PIN_NUM_MOSI;
    _config.buscfg.sclk_io_num = CONFIG_EPAPER_PIN_NUM_SCLK;
    _config.buscfg.quadwp_io_num = -1;
    _config.buscfg.quadhd_io_num = -1;
    _config.devcfg.clock_speed_hz = 10 * 1000 * 1000;          //Clock out at 10 MHz
    _config.devcfg.spics_io_num = CONFIG_EPAPER_PIN_NUM_CS;    //CS pin
    s_epaper = epaper_create(&_config);
    if (s_epaper == NULL) {
        ESP_LOGE(TAG, "epaper create failed");
    }

    // void lv_port(epd_handle_t panel);
    // lv_port(s_epaper);


    // Paint_DrawStringAt(s_epaper->paint, 0, 20, "sdgdfh", &Font16, 0);
    // Paint_text_str(s_epaper->paint, 0, 0, 100, 50, "啊的算法地方", &Font16GBK, 0);
    // epaper_refresh(s_epaper, 0);
    void image_show_task(void *args);
    image_show_task(s_epaper);
    vTaskDelete(NULL);
    

    epaper_display_data_t epaper_display_data = {
        .city = city_name,
        .district = district_name,
        .temp = data->temp,
        .hum = data->hum,
        .text = text,
        .week = week,
        .max_temp = high,
        .min_temp = low,
        .wind_class = wind_class,
        .wind_dir = wind_dir,
        .uptime = uptime,
    };

    while (1) {
        if (data->enable_http == true) {
            if (CONFIG_UPDATE_WEATHER_DATA) {
                EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group, WEATHER_DATA_UPDATE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
                if (bits & WEATHER_DATA_UPDATE_BIT) {
                    ESP_LOGI(TAG, "http get data done.");
                    ESP_LOGI(TAG, "week: %s", week);
                    ESP_LOGI(TAG, "Highest temp: %d, lowest temp: %d", high, low);
                    ESP_LOGI(TAG, "weather: %s", text);
                    ESP_LOGI(TAG, "wind class: %s, wind dir: %s", wind_class, wind_dir);
                    ESP_LOGI(TAG, "uptime: %s", uptime);
                    epaper_display_data.max_temp = high;
                    epaper_display_data.min_temp = low;
                    epaper_display(s_epaper, epaper_display_data);
                    gpio_set_level(EPAPER_PWR_CTRL, 1);
                    xEventGroupSetBits(esp_ths_event_group, EPAPER_REFRESH_BIT);
                }
            } else {
                if (refresh_times) {
                    epaper_display(s_epaper, epaper_display_data);
                    xEventGroupSetBits(esp_ths_event_group, EPAPER_REFRESH_BIT);
                    gpio_set_level(EPAPER_PWR_CTRL, 1);
                    refresh_times--;
                }
            }
        } else {
            if (refresh_times) {
                epaper_display(s_epaper, epaper_display_data);
                xEventGroupSetBits(esp_ths_event_group, EPAPER_REFRESH_BIT);
                gpio_set_level(EPAPER_PWR_CTRL, 1);
                refresh_times--;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    /*!< Config charge IC enable IO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LDO_CHIP_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LDO_CHIP_EN, 1);
    gpio_hold_en(LDO_CHIP_EN);

    fm_init();

    /* To refresh the e-paper screen, it takes at least 1250ms. */
    esp_ths_event_group = xEventGroupCreate();
    if (esp_ths_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create esp-ths event group.");
    }

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        const esp_ths_data_type_t sensor_data = {
            .temp = 0,
            .hum = 0,
            .enable_http = true,
        };
#ifdef CONFIG_REPORT_THS_DATA
        xTaskCreate(rmaker_task, "rmaker_task", 1024 * 8, (void *)&sensor_data, 5, NULL);
#endif

#if CONFIG_UPDATE_WEATHER_DATA
        xTaskCreate(http_task, "http_task", 1024 * 8, NULL, 10, NULL);
#endif
        xTaskCreate(epaper_task, "epaper_task", 1024 * 8, (void *)&sensor_data, 8, NULL);
        /* Wait for the e-paper refresh to be completed. */
        EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group, EPAPER_REFRESH_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & EPAPER_REFRESH_BIT) {
            ESP_LOGI(TAG, "E-paper refresh done.");
            epaper_delete(&s_epaper);
        }
#ifdef CONFIG_REPORT_THS_DATA
        /* Wait for successful reporting of temperature and humidity data to
         * Rainmaker before proceeding to the next step.
         */
        bits = xEventGroupWaitBits(esp_ths_event_group, RMAKER_DATA_REPORT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & RMAKER_DATA_REPORT_BIT) {
            ESP_LOGI(TAG, "Report esp-ths data done.");
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
#endif
        /* Initialize LP_I2C */
        lp_i2c_init();
#ifdef CONFIG_ULP_UART_PRINT
        /* Initialize LP_UART */
        lp_uart_init();
#endif
        /* Load LP Core binary and start the coprocessor */
        lp_core_init();
    } else if (cause == ESP_SLEEP_WAKEUP_ULP) {
        ESP_LOGI(TAG, "wake up by LP CPU.");
        float *temp = (float*)(&ulp_temp);
        float *hum = (float *)(&ulp_hum);
        bool weather_data_report = false;
#if CONFIG_UPDATE_WEATHER_DATA
        weather_data_report = (ulp_weather_data_update == 1) ? true : false;
#endif
        const esp_ths_data_type_t sensor_data = {
            .temp = *temp,
            .hum = *hum,
            .enable_http = weather_data_report
        };
        ESP_LOGI(TAG, "temperature: %f, humidity: %f", sensor_data.temp, sensor_data.hum);
#if CONFIG_UPDATE_WEATHER_DATA
        if (sensor_data.enable_http == true) {
            xTaskCreate(http_task, "http_task", 1024 * 8, NULL, 10, NULL);
        }
#endif

#ifdef CONFIG_REPORT_THS_DATA
        xTaskCreate(rmaker_task, "rmaker_task", 1024 * 8, (void *)&sensor_data, 5, NULL);
#endif
        xTaskCreate(epaper_task, "epaper_task", 1024 * 8, (void *)&sensor_data, 8, NULL);
#ifdef CONFIG_REPORT_THS_DATA
        esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_type(temp_sensor_device, ESP_RMAKER_PARAM_TEMPERATURE),
            esp_rmaker_float(roundf((*temp) * 10.0) / 10.0));
#endif
        /* Wait for the e-paper refresh to be completed. */
        EventBits_t bits = xEventGroupWaitBits(esp_ths_event_group, EPAPER_REFRESH_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & EPAPER_REFRESH_BIT) {
            ESP_LOGI(TAG, "E-paper refresh done.");
            epaper_delete(&s_epaper);
        }
#ifdef CONFIG_REPORT_THS_DATA
        /* Wait for successful reporting of temperature and humidity data to
         * Rainmaker before proceeding to the next step.
         */
        bits = xEventGroupWaitBits(esp_ths_event_group, RMAKER_DATA_REPORT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (bits & RMAKER_DATA_REPORT_BIT) {
            ESP_LOGI(TAG, "Report esp-ths data done.");
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
#endif
#if CONFIG_UPDATE_WEATHER_DATA
        if (ulp_weather_data_update == 1) {
            ulp_weather_data_update = 0;
            ulp_measure_times = 0;
        }
#endif
        ulp_sensor_data_report = 0;
    }
    /* Setup wakeup triggers */
    ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());
    /* Enter Deep Sleep */
    ESP_LOGI(TAG, "Enter deep sleep.");
    esp_deep_sleep_start();
}
