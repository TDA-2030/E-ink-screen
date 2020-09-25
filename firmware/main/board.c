#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "multi_button.h"

static const char *TAG = "button";
static struct Button btn1;

#define IO_BUTTON_PIN 34
#define IO_LED_STATUS_PIN 2
#define IO_POWER_CTRL_PIN 32

static uint8_t read_button1_GPIO()
{
	return gpio_get_level(IO_BUTTON_PIN);
}

static void periodic_timer_button_callback(void* arg)
{
    button_ticks();
}

static void BTN1_LONG_PRESS_START_Handler(void* btn)
{
	ESP_LOGW(TAG, "Start to erase nvs and restart!");
	nvs_flash_erase();
	esp_restart();
}

static esp_err_t my_button_init(void)
{
	gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set.
    io_conf.pin_bit_mask = (1ULL << IO_BUTTON_PIN) ;
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
            .callback = &periodic_timer_button_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "button",
    };

    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
	return esp_timer_start_periodic(periodic_timer, 5000);
}


static void periodic_led_callback(void* arg)
{
    static bool i=0;
    gpio_set_level(IO_LED_STATUS_PIN, i);
    i=!i;
}

void board_init(void)
{
    my_button_init();

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << IO_LED_STATUS_PIN) | (1ULL << IO_POWER_CTRL_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_led_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "led",
    };

    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
	esp_timer_start_periodic(periodic_timer, 500000);
}

void board_power_set(uint8_t is_on)
{
    gpio_set_level(IO_POWER_CTRL_PIN, is_on ? 1 : 0);
}

esp_err_t board_button_attach(PressEvent event, BtnCallback cb)
{
	button_attach(&btn1, event, cb);
	return ESP_OK;
}