/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "epdpaint.h"

typedef struct {
    spi_host_device_t spi_host_id;
    spi_bus_config_t buscfg;
    spi_device_interface_config_t devcfg;
    int32_t rst_pin;
    int32_t dc_pin;
    int32_t busy_pin;
} epaper_config_t;

typedef enum {
    EPD_REFRESH_MODE_FULL,         // Full refresh mode
    EPD_REFRESH_MODE_FULL_FAST,    // Fast refresh mode
    EPD_REFRESH_MODE_FULL_FAST2,   // Fastest refresh mode
    EPD_REFRESH_MODE_PART_AS_FULL, // Partial refresh mode to refresh full screen
    EPD_REFRESH_MODE_PART,         // Partial refresh mode
} epaper_refresh_mode_t;


typedef struct {
    char *city;
    char *district;
    float temp;
    float hum;
    char *text;
    char *week;
    int8_t max_temp;
    int8_t min_temp;
    char *wind_class;
    char *wind_dir;
    char *uptime;
} epaper_display_data_t;


typedef struct {
    epaper_config_t config;
    epd_paint_t *paint;
    spi_device_handle_t spi_handle;
    SemaphoreHandle_t busy_state;
} epaper_panel_t;

typedef epaper_panel_t *epd_handle_t;


#define WHITE 0xFF
#define BLACK 0x00

#include "GDEY029T94/Display_EPD_W21.h"

esp_err_t epaper_refresh_partial_start(epd_handle_t epd_handle, uint8_t *data);


/**
 * @brief Draw a point on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x_point X-coordinate of the point
 * @param[in] y_point Y-coordinate of the point
 */
void epaper_draw_point(epd_handle_t epaper_handle, uint16_t x_point, uint16_t y_point, uint16_t color);

/**
 * @brief Draw a line on the e-paper display
 *
 * This function draws a line between the specified start and end points on an e-paper display with
 * the specified color.
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x_start X-coordinate of the start point
 * @param[in] y_start Y-coordinate of the start point
 * @param[in] x_end X-coordinate of the end point
 * @param[in] y_end Y-coordinate of the end point
 * @param[in] color Color of the line
 */
void epaper_draw_line(epd_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color);

/**
 * @brief Draw a rectangle on the e-paper display
 *
 * This function draws a rectangle on an e-paper display with the specified color and drawing mode.
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x_start X-coordinate of the top-left corner of the rectangle
 * @param[in] y_start Y-coordinate of the top-left corner of the rectangle
 * @param[in] x_end X-coordinate of the bottom-right corner of the rectangle
 * @param[in] y_end Y-coordinate of the bottom-right corner of the rectangle
 * @param[in] color Color of the rectangle
 * @param[in] mode Drawing mode of the rectangle (1:filled or 0:outline)
 */
void epaper_draw_rectangle(epd_handle_t epaper_handle, uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end, uint16_t color, uint8_t mode);

/**
 * @brief Print a character on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the character
 * @param[in] y Y-coordinate of the top-left corner of the character
 * @param[in] chr Unicode code point of the character to be printed
 * @param[in] size Size of the character to be printed
 * @param[in] color Color of the character
 */
void epaper_print_char(epd_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t chr, uint16_t size, uint16_t color);

/**
 * @brief Print a string on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the string
 * @param[in] y Y-coordinate of the top-left corner of the string
 * @param[in] chr Pointer to the null-terminated string to be printed
 * @param[in] size Size of the characters in the string to be printed
 * @param[in] color Color of the string
 */
void epaper_print_string(epd_handle_t epaper_handle, uint16_t x, uint16_t y, char *chr, uint16_t size, uint16_t color);

/**
 * @brief Print a number on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the number
 * @param[in] y Y-coordinate of the top-left corner of the number
 * @param[in] num Unsigned 32-bit integer to be printed
 * @param[in] len Maximum number of number to be printed
 * @param[in] size Size of the number to be printed
 * @param[in] color Color of the number
 */
void epaper_print_number(epd_handle_t epd_handle, uint16_t x, uint16_t y, uint32_t num, uint16_t len, const sFONT *font, uint16_t color);

/**
 * @brief Print a Chinese character on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the character
 * @param[in] y Y-coordinate of the top-left corner of the character
 * @param[in] num Unicode code point of the Chinese character to be printed
 * @param[in] size Size of the character to be printed
 * @param[in] color Color of the character
 */
void epaper_print_chinese(epd_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t num, uint16_t size, uint16_t color);

/**
 * @brief Print a picture on the e-paper display
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] x X-coordinate of the top-left corner of the picture
 * @param[in] y Y-coordinate of the top-left corner of the picture
 * @param[in] x_size Width of the picture
 * @param[in] y_size Height of the picture
 * @param[in] bmp Pointer to the bitmap data of the picture
 * @param[in] color Color of the picture
 */
void epaper_display_picture(epd_handle_t epaper_handle, uint16_t x, uint16_t y, uint16_t x_size, uint16_t y_size, const uint8_t bmp[], uint16_t color);

/**
 * @brief Refreshes the e-paper display with new image data
 *
 * This function refreshes the e-paper display with new image data specified by the image parameter.
 *
 * @param[in] epaper_handle The e-paper handle
 * @param[in] mode refresh mode
 */
esp_err_t epaper_refresh(epd_handle_t epd_handle, epaper_refresh_mode_t mode);

/**
 * @brief Creates a new e-paper display instance
 *
 * This function initializes and creates a new e-paper display instance using the specified SPI host ID
 *
 * @param[in] spi_host_id The SPI host ID to be used for communication
 *
 * @return The handle to the newly created e-paper display instance,
 *         or NULL if the creation failed.
 */
epd_handle_t epaper_create(epaper_config_t *_config);

/**
 * @brief Deletes the specified e-paper display instance
 *
 * This function releases the resources associated with the specified e-paper display instance.
 *
 * @param[in] epaper_handle Pointer to the handle of the e-paper display instance
 *                          to be deleted.
 *
 * @return  - ESP_OK if the e-paper display instance is successfully deleted.
 *          - An error code if there was an error deleting the instance.
 */
esp_err_t epaper_delete(epd_handle_t *epaper_handle);


#ifdef __cplusplus
}
#endif
