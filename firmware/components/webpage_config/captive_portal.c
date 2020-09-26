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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "adapt/include/esp32_wifi.h"
#include "adapt/include/esp32_httpd.h"
#include "cgi/include/cgiwifi.h"
#include "captive_portal.h"

static const char *TAG = "captive_portal";
static bool g_configed=0;

esp_err_t captive_portal_start(const char *ap_ssid, const char *ap_pwd, bool *configured)
{
    esp_err_t ret;
    

    ESP_LOGI(TAG, "Setup Wifi ...");
    wifiIinitialize(ap_ssid, ap_pwd, &g_configed);
    *configured = g_configed;
    if (1 == g_configed)
    {
        ESP_LOGI(TAG, "WiFi already configed");
        return ESP_OK;
    }

    /* start http server task */
    ESP_LOGD(TAG, "Free heap size before enable http server: %d", esp_get_free_heap_size());
    ret = esp32HttpServerEnable();

    if (ESP_OK != ret)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Http server ready ...");

    return ESP_OK;
}

esp_err_t captive_portal_wait(uint32_t ticks_to_wait)
{
    esp_err_t ret = ESP_OK;

    /* waiting for wifi connected */
    if (wifiConnected(ticks_to_wait) != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi connect timeout!");
        return ESP_FAIL;
    }
    if (1 == g_configed)
    {
        return ESP_OK;
    }
    
    portTickType ticks_start = xTaskGetTickCount();

    while (1)
    {
        TickType_t wait_time = xTaskGetTickCount();

        if ((wait_time - ticks_start) >= ticks_to_wait)
        {
            ESP_LOGE(TAG, "receive config success message timeout!");
            return ESP_FAIL;
        }

        if (ESP_OK == cgi_is_success())
        {
            ESP_LOGI(TAG, "Http server deleted ...");
            esp32HttpServerDisable();
            break;
        }

        vTaskDelay(100 / portTICK_RATE_MS);
    }

    return ret;
}