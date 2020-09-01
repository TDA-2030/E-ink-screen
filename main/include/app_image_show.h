#ifndef APP_IMAGE_SHOW_H_
#define APP_IMAGE_SHOW_H_

#ifdef __cplusplus 
extern "C" {
#endif

#include "esp_err.h"
#include "epd2in13.h"
#include "epdpaint.h"

esp_err_t image_show_start(Paint *paint, Epd *epd);

esp_err_t image_show_stop(void);


#ifdef __cplusplus 
}
#endif

#endif
