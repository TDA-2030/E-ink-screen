#ifndef __ESP32_HTTPD_H__
#define __ESP32_HTTPD_H__

#include "esp_err.h"

// Start http server
esp_err_t esp32HttpServerEnable(void);

// Disable http server
esp_err_t esp32HttpServerDisable(void);

#endif // __ESP32_HTTPD_H__