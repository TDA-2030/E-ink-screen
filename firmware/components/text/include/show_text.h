#ifndef APP_SHOW_TEXT_H_
#define APP_SHOW_TEXT_H_

#ifdef __cplusplus 
extern "C" {
#endif

#include "esp_err.h"
#include "epd2in13.h"
#include "epdpaint.h"


esp_err_t app_show_text_init(void);

esp_err_t app_show_text(uint16_t x, uint16_t y, char *font, uint8_t size, uint8_t mode);

esp_err_t app_show_text_str(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const char *string, uint8_t size, uint8_t mode);

#ifdef __cplusplus 
}
#endif

#endif
