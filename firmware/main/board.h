#ifndef MY_BOARD_H_
#define MY_BOARD_H_

#include "esp_log.h"
#include "multi_button.h"

#ifdef __cplusplus 
extern "C" {
#endif

void board_init(void);

void board_power_set(uint8_t is_on);

esp_err_t board_pa_ctrl(bool enable);

esp_err_t board_button_attach(PressEvent event, BtnCallback cb);

#ifdef __cplusplus 
}
#endif

#endif
