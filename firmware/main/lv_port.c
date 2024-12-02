
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "app_epaper.h"



#if CONFIG_EXAMPLE_DOUBLE_FB
#define EXAMPLE_LCD_NUM_FB             2
#else
#define EXAMPLE_LCD_NUM_FB             1
#endif // CONFIG_EXAMPLE_DOUBLE_FB

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2

#define BIT_SET(a,b) ((a) |= (1U<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1U<<(b)))

static SemaphoreHandle_t lvgl_mux = NULL;

static const char *TAG = "lv_port";

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

bool example_lvgl_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void example_lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (example_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}


static void _set_px_cb(lv_disp_drv_t * disp_drv, uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
        lv_color_t color, lv_opa_t opa)
{
    // printf("_set_px_cb buf_w=%d, x=%d, y=%d, color=%d, opa=%d\n", buf_w, x, y, color.full, opa);
    x = buf_w -1 - x; // mirror x axis
    uint16_t byte_index = (x + y * buf_w)/8;
    uint8_t  bit_index  = x % 8;

    if ((color.full == 0) && (LV_OPA_TRANSP != opa)) {
        buf[byte_index] &= ~(0x80 >> bit_index);
    } else {
        buf[byte_index] |= 0x80 >> bit_index;
    }
}

static void _flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{

    printf("_flush buf=%p\n", color_p);

    // err = send_pixels(disp_drv, color_p, OLED_COLUMNS * (1 + row2 - row1));
    // assert(0 == err);
    printf("flush area %d,%d %d,%d\n", area->x1, area->y1, area->x2, area->y2);
    epd_handle_t panel = (epd_handle_t)disp_drv->user_data;
    // Paint_DrawImage(panel->paint);

    EPD_WhiteScreen_ALL_Fast(panel, color_p); //To display one image using fast refresh.

    
    if(lv_disp_flush_is_last(&disp_drv)){
        printf("flush end\n");
        EPD_DeepSleep(panel); //Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.
    }
    lv_disp_flush_ready(disp_drv);
    printf("flush d0=%u\n", color_p[0].full);
}

static void _rounder(lv_disp_drv_t * disp_drv, lv_area_t *area)
{
    printf("rounder area %d,%d %d,%d\n", area->x1, area->y1, area->x2, area->y2);
    /* Update the areas as needed.
   * For example it makes the area to start only on 8th rows and have Nx8 pixel height.*/
   area->y1 = area->y1 & 0x07;
   area->y2 = (area->y2 & 0x07) + 8;
    // uint8_t hor_max = disp_drv->hor_res;
    // uint8_t ver_max = disp_drv->ver_res;

    // area->x1 = 0;
    // area->y1 = 0;
    // area->x2 = hor_max - 1;
    // area->y2 = ver_max - 1;
}

static void _render_start(struct _lv_disp_drv_t * disp_drv)
{
    printf("_render_start\n");
    epd_handle_t panel = (epd_handle_t)disp_drv->user_data;
    // epaper_refresh_partial_start(panel, );
    EPD_HW_Init_Fast(panel); //Fast refresh initialization.

    
}
static void _monitor(struct _lv_disp_drv_t * disp_drv, uint32_t time, uint32_t px)
{
    epd_handle_t panel = (epd_handle_t)disp_drv->user_data;
    printf("_monitor px=%lu, time=%lu\n", px, time);
}


void example_lvgl_demo_ui(lv_disp_t *disp)
{

    static lv_style_t style;
    lv_style_init(&style);

    /*Set a background color and a radius*/
    lv_style_set_radius(&style, 10);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    // lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREY, 1));

    /*Add border to the bottom+right*/
    // lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_width(&style, 5);
    // lv_style_set_border_opa(&style, LV_OPA_50);
    // lv_style_set_border_side(&style, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT);


    lv_obj_t * win = lv_win_create(lv_scr_act(), 40);

    lv_obj_add_style(win, &style, 0);
    lv_obj_center(win);


    lv_obj_t * btn;
    btn = lv_win_add_btn(win, LV_SYMBOL_LEFT, 40);

    lv_win_add_title(win, "A title");

    btn = lv_win_add_btn(win, LV_SYMBOL_RIGHT, 40);

    btn = lv_win_add_btn(win, LV_SYMBOL_CLOSE, 60);

    lv_obj_t * cont = lv_win_get_content(win);  /*Content can be added here*/
    lv_obj_t * label = lv_label_create(cont);
    lv_label_set_text(label, "This is\n"
                      "a pretty\n"
                      "long text\n"
                      "to see how\n"
                      "the window\n"
                      "becomes\n"
                      "scrollable.\n"
                      "\n"
                      "\n"
                      "Some more\n"
                      "text to be\n"
                      "sure it\n"
                      "overflows. :)");
}

void lv_port(epd_handle_t panel)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    void *buf1 = NULL;
    void *buf2 = NULL;
    size_t buffer_size = panel->paint->height_memory*panel->paint->width_memory;

    ESP_LOGI(TAG, "Allocate separate LVGL draw buffers from PSRAM");
    buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DEFAULT);
    assert(buf1);
    printf("lv_port buf=%p\n", buf1);

    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = panel->paint->width_memory;
    disp_drv.ver_res = panel->paint->height_memory;
    disp_drv.flush_cb = _flush;
    disp_drv.set_px_cb = _set_px_cb;
    disp_drv.rounder_cb = _rounder;
    disp_drv.render_start_cb = _render_start;
    disp_drv.monitor_cb = _monitor;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.full_refresh = 1;
    disp_drv.user_data = panel;

    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);


    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (example_lvgl_lock(-1)) {
        example_lvgl_demo_ui(disp);
        // Release the mutex
        example_lvgl_unlock();
    }
}




