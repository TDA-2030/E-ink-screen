

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "multi_button.h"

static const char *TAG = "button";
static struct Button btn1;

#define PIN_NUM_BUTTON 34

static uint8_t read_button1_GPIO()
{
	return gpio_get_level(PIN_NUM_BUTTON);
}

static void periodic_timer_callback(void* arg)
{
    button_ticks();
}

static void BTN1_LONG_PRESS_START_Handler(void* btn)
{
	ESP_LOGW(TAG, "Start to erase nvs and restart!");
	nvs_flash_erase();
	esp_restart();
}

esp_err_t my_button_init(void)
{
	gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set.
    io_conf.pin_bit_mask = (1ULL << PIN_NUM_BUTTON) ;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

	button_init(&btn1, read_button1_GPIO, 1);
	button_attach(&btn1, LONG_PRESS_START, BTN1_LONG_PRESS_START_Handler);

	button_start(&btn1);

	//make the timer invoking the button_ticks() interval 5ms.
	//This function is implemented by yourself.

	const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "button",
    };

    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
	return esp_timer_start_periodic(periodic_timer, 5000);
}

esp_err_t my_button_attach(PressEvent event, BtnCallback cb)
{
	button_attach(&btn1, event, cb);
	return ESP_OK;
}
