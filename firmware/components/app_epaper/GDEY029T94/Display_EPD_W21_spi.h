#ifndef _DISPLAY_EPD_W21_SPI_
#define _DISPLAY_EPD_W21_SPI_

//IO settings

#define isEPD_W21_BUSY gpio_get_level(panel->config.busy_pin)  //BUSY
#define EPD_W21_RST_0 gpio_set_level(panel->config.rst_pin, 0)  //RES
#define EPD_W21_RST_1 gpio_set_level(panel->config.rst_pin, 1)
#define EPD_W21_DC_0  //gpio_set_level(CONFIG_EPAPER_PIN_NUM_DC, 0) //DC
#define EPD_W21_DC_1  //gpio_set_level(CONFIG_EPAPER_PIN_NUM_DC, 1)
#define EPD_W21_CS_0 //digitalWrite(A17,LOW) //CS
#define EPD_W21_CS_1 //digitalWrite(A17,HIGH)


#define EPD_W21_WriteCMD(command)  epaper_write_command(panel->spi_handle, (command), false)
#define EPD_W21_WriteDATA(datas) {uint8_t d = (datas);epaper_write_data(panel->spi_handle, &d, 1);}


#endif 
